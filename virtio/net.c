#include "kvm/virtio-pci-dev.h"
#include "kvm/virtio-net.h"
#include "kvm/virtio.h"
#include "kvm/mutex.h"
#include "kvm/util.h"
#include "kvm/kvm.h"
#include "kvm/irq.h"
#include "kvm/uip.h"
#include "kvm/guest_compat.h"
#include "kvm/iovec.h"
#include "kvm/strbuf.h"

#include <linux/vhost.h>
#include <linux/virtio_net.h>
#include <linux/if_tun.h>
#include <linux/types.h>

#include <arpa/inet.h>
#include <net/if.h>

#include <unistd.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/eventfd.h>

#include <popcorn/utils.h>

#ifdef CONFIG_POPCORN_HYPE
#define VIRTIO_NET_QUEUE_SIZE	32768 /* Jack TODO testing: 1024 (O) 8192 (O) 16384 (?) 32768 (O) 65536 (X)*/
#else
#define VIRTIO_NET_QUEUE_SIZE		256
#endif
#define VIRTIO_NET_NUM_QUEUES		8

struct net_dev;

struct net_dev_operations {
	int (*rx)(struct iovec *iov, u16 in, struct net_dev *ndev);
	int (*tx)(struct iovec *iov, u16 in, struct net_dev *ndev);
};

struct net_dev_queue {
	int				id;
	struct net_dev			*ndev;
	struct virt_queue		vq;
	pthread_t			thread;
	struct mutex			lock;
	pthread_cond_t			cond;
	int				gsi;
	int				irqfd;
#ifdef CONFIG_POPCORN_HYPE
	int				to_nid;
#endif
};

struct net_dev {
	struct mutex			mutex;
	struct virtio_device		vdev;
	struct list_head		list;

	struct net_dev_queue		queues[VIRTIO_NET_NUM_QUEUES * 2 + 1];
	struct virtio_net_config	config;
	u32				features, queue_pairs;

	int				vhost_fd;
	int				vhost_fds[VIRTIO_NET_NUM_QUEUES];
	int				tap_fd;
	int				tap_fds[VIRTIO_NET_NUM_QUEUES];
	char			tap_name[IFNAMSIZ];
	bool			tap_ufo;

	int				mode;

	struct uip_info			info;
	struct net_dev_operations	*ops;
	struct kvm			*kvm;

	struct virtio_net_params	*params;
};

static LIST_HEAD(ndevs);
static int compat_id = -1;

#define MAX_PACKET_SIZE 65550

static bool has_virtio_feature(struct net_dev *ndev, u32 feature)
{
	return ndev->features & (1 << feature);
}

static void virtio_net_fix_tx_hdr(struct virtio_net_hdr *hdr, struct net_dev *ndev)
{
	hdr->hdr_len		= virtio_guest_to_host_u16(&ndev->vdev, hdr->hdr_len);
	hdr->gso_size		= virtio_guest_to_host_u16(&ndev->vdev, hdr->gso_size);
	hdr->csum_start		= virtio_guest_to_host_u16(&ndev->vdev, hdr->csum_start);
	hdr->csum_offset	= virtio_guest_to_host_u16(&ndev->vdev, hdr->csum_offset);
}

static void virtio_net_fix_rx_hdr(struct virtio_net_hdr *hdr, struct net_dev *ndev)
{
	hdr->hdr_len		= virtio_host_to_guest_u16(&ndev->vdev, hdr->hdr_len);
	hdr->gso_size		= virtio_host_to_guest_u16(&ndev->vdev, hdr->gso_size);
	hdr->csum_start		= virtio_host_to_guest_u16(&ndev->vdev, hdr->csum_start);
	hdr->csum_offset	= virtio_host_to_guest_u16(&ndev->vdev, hdr->csum_offset);
}

static void *virtio_net_rx_thread(void *p)
{
	struct iovec iov[VIRTIO_NET_QUEUE_SIZE];
	struct net_dev_queue *queue = p;
	struct virt_queue *vq = &queue->vq;
	struct net_dev *ndev = queue->ndev;
	struct kvm *kvm;
	u16 out, in;
	u16 head;
	int len, copied;

#ifdef CONFIG_POPCORN_HYPE
	POP_PRINTF("\t[%d/%d] %s(): this thread \"virtio_net_rx_thread\"\n",
							pop_get_nid(), popcorn_gettid(), __func__);
#endif
	kvm__set_thread_name("virtio-net-rx");

	kvm = ndev->kvm;
	while (1) {
		mutex_lock(&queue->lock);
		if (!virt_queue__available(vq))
			pthread_cond_wait(&queue->cond, &queue->lock.mutex);
		mutex_unlock(&queue->lock);

		while (virt_queue__available(vq)) {
			unsigned char buffer[MAX_PACKET_SIZE + sizeof(struct virtio_net_hdr_mrg_rxbuf)];
			struct iovec dummy_iov = {
				.iov_base = buffer,
				.iov_len  = sizeof(buffer),
			};
			struct virtio_net_hdr_mrg_rxbuf *hdr;
			u16 num_buffers;

#ifdef CONFIG_POPCORN_HYPE
			POP_PRINTF("\t[net %d/%d] %s(): <- vq %p rx available\n",
					pop_get_nid(), popcorn_gettid(), __func__, vq);
#endif
			len = ndev->ops->rx(&dummy_iov, 1, ndev);
			if (len < 0) {
				pr_warning("%s: rx on vq %u failed (%d), exiting thread\n",
						__func__, queue->id, len);
				goto out_err;
			}

			copied = num_buffers = 0;
			head = virt_queue__get_iov(vq, iov, &out, &in, kvm);
			hdr = iov[0].iov_base;
			while (copied < len) {
				size_t iovsize = min_t(size_t, len - copied, iov_size(iov, in));

				memcpy_toiovec(iov, buffer + copied, iovsize);
				copied += iovsize;
				virt_queue__set_used_elem_no_update(vq, head, iovsize, num_buffers++);
				if (copied == len)
					break;
				while (!virt_queue__available(vq))
					sleep(0);
				head = virt_queue__get_iov(vq, iov, &out, &in, kvm);
			}

			virtio_net_fix_rx_hdr(&hdr->hdr, ndev);
			if (has_virtio_feature(ndev, VIRTIO_NET_F_MRG_RXBUF))
				hdr->num_buffers = virtio_host_to_guest_u16(vq, num_buffers);

			virt_queue__used_idx_advance(vq, num_buffers);

			/* We should interrupt guest right now, otherwise latency is huge. */
			if (virtio_queue__should_signal(vq))
				ndev->vdev.ops->signal_vq(kvm, &ndev->vdev, queue->id);
		}
	}

out_err:
	pthread_exit(NULL);
	return NULL;

}

static void *virtio_net_tx_thread(void *p)
{
	struct iovec iov[VIRTIO_NET_QUEUE_SIZE];
	struct net_dev_queue *queue = p;
	struct virt_queue *vq = &queue->vq;
	struct net_dev *ndev = queue->ndev;
	struct kvm *kvm;
	u16 out, in;
	u16 head;
	int len;

#ifdef CONFIG_POPCORN_HYPE
	POP_PRINTF("\t[net %d/%d] %s(): this thread \"virtio_net_tx_thread\"\n",
							pop_get_nid(), popcorn_gettid(), __func__);
#endif

	kvm__set_thread_name("virtio-net-tx");

	kvm = ndev->kvm;

	while (1) {
		mutex_lock(&queue->lock);
		if (!virt_queue__available(vq))
			pthread_cond_wait(&queue->cond, &queue->lock.mutex); /* blocking */
		mutex_unlock(&queue->lock);

		while (virt_queue__available(vq)) {
			struct virtio_net_hdr *hdr;
			head = virt_queue__get_iov(vq, iov, &out, &in, kvm);
			hdr = iov[0].iov_base;
			virtio_net_fix_tx_hdr(hdr, ndev);

#ifdef CONFIG_POPCORN_HYPE
			POP_PRINTF("\t[net %d/%d] %s(): vq %p tx ->\n",
					pop_get_nid(), popcorn_gettid(), __func__, vq);
#endif
			len = ndev->ops->tx(iov, out, ndev);
			if (len < 0) {
				pr_warning("%s: tx on vq %u failed (%d)\n",
						__func__, queue->id, errno);
				goto out_err;
			}

			virt_queue__set_used_elem(vq, head, len);
		}

		if (virtio_queue__should_signal(vq))
			ndev->vdev.ops->signal_vq(kvm, &ndev->vdev, queue->id);
	}

out_err:
	pthread_exit(NULL);
	return NULL;
}

static virtio_net_ctrl_ack virtio_net_handle_mq(struct kvm* kvm, struct net_dev *ndev, struct virtio_net_ctrl_hdr *ctrl)
{
	/* Not much to do here */
	return VIRTIO_NET_OK;
}

static void *virtio_net_ctrl_thread(void *p)
{
	struct iovec iov[VIRTIO_NET_QUEUE_SIZE];
	struct net_dev_queue *queue = p;
	struct virt_queue *vq = &queue->vq;
	struct net_dev *ndev = queue->ndev;
	u16 out, in, head;
	struct kvm *kvm = ndev->kvm;
	struct virtio_net_ctrl_hdr *ctrl;
	virtio_net_ctrl_ack *ack;

#ifdef CONFIG_POPCORN_HYPE
//    POP_PRINTF("\t[net %d/%d] %s: %s(): this thread \"virtio_net_ctrl_thread\""
//			"created at runtime\n",
//			pop_get_nid(), popcorn_gettid(), __FILE__, __func__);
//	//kvm__set_thread_name("virtio-net-ctrl");
//    POP_PRINTF("\t[net %d/%d] %s: %s(): AGAIN? BUG WHEN ASSIGNING THREAD NAME?\n",
//			pop_get_nid(), popcorn_gettid(), __FILE__, __func__);

	if (queue->to_nid > 0) {
		POP_PRINTF("\t[net %d*] %s: %s(): "
				"migrating \"virtio_net_ctrl_thread\" to [%d]\n",
				pop_get_nid(), __FILE__, __func__, queue->to_nid);
		migrate(queue->to_nid, NULL, NULL); /**** migrate to the node it suppose to run on ****/
	}
	POP_PRINTF("\t[net %d] %s: %s(): "
			"\"virtio_net_ctrl_thread\" running at remote\n",
			pop_get_nid(), __FILE__, __func__);
#else
	kvm__set_thread_name("virtio-net-ctrl");
#endif

	while (1) {
		mutex_lock(&queue->lock);
		if (!virt_queue__available(vq))
			pthread_cond_wait(&queue->cond, &queue->lock.mutex);
		mutex_unlock(&queue->lock);

#ifdef CONFIG_POPCORN_HYPE
		POP_PRINTF("\t[net %d/%d] %s: %s(): got 1 local req\n",
				pop_get_nid(), popcorn_gettid(), __FILE__, __func__);
#endif

		while (virt_queue__available(vq)) {
			head = virt_queue__get_iov(vq, iov, &out, &in, kvm);
			ctrl = iov[0].iov_base;
			ack = iov[out].iov_base;

			switch (ctrl->class) {
			case VIRTIO_NET_CTRL_MQ:
				*ack = virtio_net_handle_mq(kvm, ndev, ctrl);
				break;
			default:
				*ack = VIRTIO_NET_ERR;
				break;
			}
			virt_queue__set_used_elem(vq, head, iov[out].iov_len);
		}

		if (virtio_queue__should_signal(vq))
			ndev->vdev.ops->signal_vq(kvm, &ndev->vdev, queue->id);
	}

	pthread_exit(NULL);

	return NULL;
}

static void virtio_net_handle_callback(struct kvm *kvm, struct net_dev *ndev, int queue)
{
	struct net_dev_queue *net_queue = &ndev->queues[queue];

	if ((u32)queue >= (ndev->queue_pairs * 2 + 1)) {
		pr_warning("Unknown queue index %u", queue);
		return;
	}

#ifdef CONFIG_POPCORN_HYPE
	POP_PRINTF("\t[net %d/%d] %s(): net_queue %p &net_queue->vq %p signal _/-> (tx)\n",
		pop_get_nid(), popcorn_gettid(), __func__, net_queue, &net_queue->vq);
#endif

	mutex_lock(&net_queue->lock);
	pthread_cond_signal(&net_queue->cond);
	mutex_unlock(&net_queue->lock);
}

static int virtio_net_request_tap(struct net_dev *ndev, struct ifreq *ifr,
				  const char *tapname)
{
	int ret;

	memset(ifr, 0, sizeof(*ifr));
	ifr->ifr_flags = IFF_TAP | IFF_NO_PI | IFF_VNET_HDR;
	if (ndev->queue_pairs > 1)
		ifr->ifr_flags |= IFF_MULTI_QUEUE;
	if (tapname)
		strlcpy(ifr->ifr_name, tapname, sizeof(ifr->ifr_name));

	ret = ioctl(ndev->tap_fd, TUNSETIFF, ifr);

	if (ret >= 0)
		strlcpy(ndev->tap_name, ifr->ifr_name, sizeof(ndev->tap_name));

	if (ndev->queue_pairs > 1) {
		u32 i;
		for (i = 1; i < ndev->queue_pairs; i++) {
			if (ioctl(ndev->tap_fds[i], TUNSETIFF, ifr) == -1) {
				return -1;
			}
		}
	}

	return ret;
}

static int virtio_net_exec_script(const char* script, const char *tap_name)
{
	pid_t pid;
	int status;

	pid = fork();
	if (pid == 0) {
		execl(script, script, tap_name, NULL);
		_exit(1);
	} else {
		waitpid(pid, &status, 0);
		if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
			pr_warning("Fail to setup tap by %s", script);
			return -1;
		}
	}
	return 0;
}

static bool virtio_net__tap_init(struct net_dev *ndev)
{
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	int hdr_len;
	struct sockaddr_in sin = {0};
	struct ifreq ifr;
	const struct virtio_net_params *params = ndev->params;
	bool skipconf = !!params->tapif;

#ifdef CONFIG_POPCORN_HYPE
    POP_PRINTF("\t[net %d/%d/%d] %s():\n",
			pop_get_nid(), getpid(), popcorn_gettid(), __func__);
#endif

	hdr_len = has_virtio_feature(ndev, VIRTIO_NET_F_MRG_RXBUF) ?
			sizeof(struct virtio_net_hdr_mrg_rxbuf) :
			sizeof(struct virtio_net_hdr);
	if (ioctl(ndev->tap_fd, TUNSETVNETHDRSZ, &hdr_len) < 0)
		pr_warning("Config tap device TUNSETVNETHDRSZ error"); /* BUG - origin */

	if (strcmp(params->script, "none")) {
		if (virtio_net_exec_script(params->script, ndev->tap_name) < 0)
			goto fail;
	} else if (!skipconf) {
		memset(&ifr, 0, sizeof(ifr));
		strncpy(ifr.ifr_name, ndev->tap_name, sizeof(ifr.ifr_name));
		sin.sin_addr.s_addr = inet_addr(params->host_ip);
		memcpy(&(ifr.ifr_addr), &sin, sizeof(ifr.ifr_addr));
		ifr.ifr_addr.sa_family = AF_INET;
		if (ioctl(sock, SIOCSIFADDR, &ifr) < 0) {
			pr_warning("Could not set ip address on tap device");
			goto fail;
		}
	}

	if (!skipconf) {
		memset(&ifr, 0, sizeof(ifr));
		strncpy(ifr.ifr_name, ndev->tap_name, sizeof(ifr.ifr_name));
		ioctl(sock, SIOCGIFFLAGS, &ifr);
		ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
		if (ioctl(sock, SIOCSIFFLAGS, &ifr) < 0)
			pr_warning("Could not bring tap device up");
	}

	close(sock);

	return 1;

fail:
	if (sock >= 0)
		close(sock);
	if (ndev->tap_fd >= 0)
		close(ndev->tap_fd);

	return 0;
}

static void virtio_net__tap_exit(struct net_dev *ndev)
{
	int sock;
	struct ifreq ifr;

	if (ndev->params->tapif)
		return;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	strncpy(ifr.ifr_name, ndev->tap_name, sizeof(ifr.ifr_name));
	ioctl(sock, SIOCGIFFLAGS, &ifr);
	ifr.ifr_flags &= ~(IFF_UP | IFF_RUNNING);
	if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0)
		pr_warning("Count not bring tap device down");
	close(sock);
}

static bool virtio_net__tap_create(struct net_dev *ndev)
{
	u32 i = 1;
	int offload;
	struct ifreq ifr;
	const struct virtio_net_params *params = ndev->params;
	bool macvtap = (!!params->tapif) && (params->tapif[0] == '/');

	/* Did the user already gave us the FD? */
	if (params->fd)
		ndev->tap_fd = params->fd;
	else {
		const char *tap_file = "/dev/net/tun";

		/* Did the user ask us to use macvtap? */
		if (macvtap)
			tap_file = params->tapif;

		ndev->tap_fd = open(tap_file, O_RDWR); /* pophype broadcast */
		if (ndev->tap_fd < 0) {
			pr_warning("Unable to open %s", tap_file);
			return 0;
		}
#ifdef CONFIG_POPCORN_HYPE
		POP_PRINTF("\t\tnet: tap: %s(): fd %d "
					"(remote needs to create this fd)\n",
									__func__, ndev->tap_fd);
#endif

		if (ndev->queue_pairs > 1) {
			int fd;
			ndev->tap_fds[0] = ndev->tap_fd;
			for (; i < ndev->queue_pairs; i++) {
				fd = open(tap_file, O_RDWR);
				if (fd < 0) {
					pr_warning("Unable to open %s", tap_file);
					goto fail;
				}
				ndev->tap_fds[i] = fd;
			}
		}

	}

#ifdef CONFIG_POPCORN_HYPE
	POP_PRINTF("\t\tnet: tap: %s(): fd %d (ioctl TUNSETIFF)\n",
									__func__, ndev->tap_fd);
#endif
	if (!macvtap &&
	    virtio_net_request_tap(ndev, &ifr, params->tapif) < 0) { /* pophype broadcast */
		pr_warning("Config tap device error. Are you root?");
		goto fail;
	}

	/*
	 * The UFO support had been removed from kernel in commit:
	 * ID: fb652fdfe83710da0ca13448a41b7ed027d0a984
	 * https://www.spinics.net/lists/netdev/msg443562.html
	 * In oder to support the older kernels without this commit,
	 * we set the TUN_F_UFO to offload by default to test the status of
	 * UFO kernel support.
	 */
	ndev->tap_ufo = true;
	offload = TUN_F_CSUM | TUN_F_TSO4 | TUN_F_TSO6 | TUN_F_UFO;
#ifdef CONFIG_POPCORN_HYPE
	POP_PRINTF("\t\tnet: tap: %s(): fd %d (ioctl TUNSETOFFLOAD)\n",
									__func__, ndev->tap_fd);
#endif
	if (ioctl(ndev->tap_fd, TUNSETOFFLOAD, offload) < 0) { /* pophype broadcast */
		/*
		 * Is this failure caused by kernel remove the UFO support?
		 * Try TUNSETOFFLOAD without TUN_F_UFO.
		 */
		offload &= ~TUN_F_UFO;
#ifdef CONFIG_POPCORN_HYPE
		POP_PRINTF("\t\tnet: tap: %s(): fd %d (ioctl TUNSETOFFLOAD)\n",
										__func__, ndev->tap_fd);
#endif
		if (ioctl(ndev->tap_fd, TUNSETOFFLOAD, offload) < 0) { /* pophype broadcast */
			pr_warning("Config tap device TUNSETOFFLOAD error");
			goto fail;
		}
		ndev->tap_ufo = false;
	}

	return 1;

fail:
	for (--i; i > 0; i--)
		close(ndev->tap_fds[i]);

	if ((ndev->tap_fd >= 0) || (!params->fd) )
		close(ndev->tap_fd);

	return 0;
}

static inline int tap_ops_tx(struct iovec *iov, u16 out, struct net_dev *ndev)
{
	return writev(ndev->tap_fd, iov, out);
}

static inline int tap_ops_rx(struct iovec *iov, u16 in, struct net_dev *ndev)
{
	return readv(ndev->tap_fd, iov, in);
}

static inline int uip_ops_tx(struct iovec *iov, u16 out, struct net_dev *ndev)
{
	return uip_tx(iov, out, &ndev->info);
}

static inline int uip_ops_rx(struct iovec *iov, u16 in, struct net_dev *ndev)
{
	return uip_rx(iov, in, &ndev->info);
}

static struct net_dev_operations tap_ops = {
	.rx	= tap_ops_rx,
	.tx	= tap_ops_tx,
};

static struct net_dev_operations uip_ops = {
	.rx	= uip_ops_rx,
	.tx	= uip_ops_tx,
};

static u8 *get_config(struct kvm *kvm, void *dev)
{
	struct net_dev *ndev = dev;

	return ((u8 *)(&ndev->config));
}

static u32 get_host_features(struct kvm *kvm, void *dev)
{
	u32 features;
	struct net_dev *ndev = dev;

	features = 1UL << VIRTIO_NET_F_MAC
		| 1UL << VIRTIO_NET_F_CSUM
		| 1UL << VIRTIO_NET_F_HOST_TSO4
		| 1UL << VIRTIO_NET_F_HOST_TSO6
		| 1UL << VIRTIO_NET_F_GUEST_TSO4
		| 1UL << VIRTIO_NET_F_GUEST_TSO6
		| 1UL << VIRTIO_RING_F_EVENT_IDX
		| 1UL << VIRTIO_RING_F_INDIRECT_DESC
		| 1UL << VIRTIO_NET_F_CTRL_VQ
		| 1UL << VIRTIO_NET_F_MRG_RXBUF
		| 1UL << (ndev->queue_pairs > 1 ? VIRTIO_NET_F_MQ : 0);

	/*
	 * The UFO feature for host and guest only can be enabled when the
	 * kernel has TAP UFO support.
	 */
	if (ndev->tap_ufo)
		features |= (1UL << VIRTIO_NET_F_HOST_UFO
				| 1UL << VIRTIO_NET_F_GUEST_UFO);

	return features;
}

static int virtio_net__vhost_set_features(struct net_dev *ndev)
{
	u64 features = 1UL << VIRTIO_RING_F_EVENT_IDX;
	u64 vhost_features;
	int i, r = 0;

	if (ioctl(ndev->vhost_fd, VHOST_GET_FEATURES, &vhost_features) != 0)
		die_perror("VHOST_GET_FEATURES failed");

	/* make sure both side support mergable rx buffers */
	if (vhost_features & 1UL << VIRTIO_NET_F_MRG_RXBUF &&
			has_virtio_feature(ndev, VIRTIO_NET_F_MRG_RXBUF))
		features |= 1UL << VIRTIO_NET_F_MRG_RXBUF;

	for (i = 0; ((u32)i < ndev->queue_pairs) && (r >= 0); i++)
		r = ioctl(ndev->vhost_fds[i], VHOST_SET_FEATURES, &features);
	return r;
}

static void set_guest_features(struct kvm *kvm, void *dev, u32 features)
{
	struct net_dev *ndev = dev;
	struct virtio_net_config *conf = &ndev->config;

	ndev->features = features;

	conf->status = virtio_host_to_guest_u16(&ndev->vdev, conf->status);
	conf->max_virtqueue_pairs = virtio_host_to_guest_u16(&ndev->vdev,
							     conf->max_virtqueue_pairs);
}

static void virtio_net_start(struct net_dev *ndev)
{
#ifdef CONFIG_POPCORN_HYPE
    POP_PRINTF("\t[net %d/%d/%d] %s(): %s:\n",
			pop_get_nid(), getpid(), popcorn_gettid(), __func__, __FILE__);
#endif
	if (ndev->mode == NET_MODE_TAP) {
		if (!virtio_net__tap_init(ndev))
			die_perror("TAP device initialized failed because"); /* BUG - remote */

		if (ndev->vhost_fd &&
				virtio_net__vhost_set_features(ndev) != 0)
			die_perror("VHOST_SET_FEATURES failed");
	} else {
		ndev->info.vnet_hdr_len = has_virtio_feature(ndev, VIRTIO_NET_F_MRG_RXBUF) ?
						sizeof(struct virtio_net_hdr_mrg_rxbuf) :
						sizeof(struct virtio_net_hdr);
		uip_init(&ndev->info);
	}
}

static void virtio_net_stop(struct net_dev *ndev)
{
	/* Undo whatever start() did */
	if (ndev->mode == NET_MODE_TAP)
		virtio_net__tap_exit(ndev);
	else
		uip_exit(&ndev->info);
}

static void notify_status(struct kvm *kvm, void *dev, u32 status)
{
#ifdef CONFIG_POPCORN_HYPE
    POP_PRINTF("\t[net %d/%d/%d] %s(): %s: action = \"%s\"\n",
		pop_get_nid(), getpid(), popcorn_gettid(), __func__, __FILE__,
		status & VIRTIO__STATUS_START ? "START" :
			status & VIRTIO__STATUS_STOP ? "STOP" : "do nothing");
#endif
	if (status & VIRTIO__STATUS_START)
		virtio_net_start(dev);
	else if (status & VIRTIO__STATUS_STOP)
		virtio_net_stop(dev);
}

static bool is_ctrl_vq(struct net_dev *ndev, u32 vq)
{
	return vq == (u32)(ndev->queue_pairs * 2);
}

static int init_vq(struct kvm *kvm, void *dev, u32 vq, u32 page_size, u32 align,
		   u32 pfn)
{
	struct vhost_vring_state state = { .index = (vq % 2)};
	struct net_dev_queue *net_queue;
	struct vhost_vring_addr addr;
	struct net_dev *ndev = dev;
	struct virt_queue *queue;
	void *p;
	int r;

#ifdef CONFIG_POPCORN_HYPE
	POP_PRINTF("\t[net %d/%d/%d] %s: %s() %p: virtio_init_device_vq vq %u "
			"(0:rx, 1:tx, 2:ctl), ndev->vhost_fds[%d] fd %d "
			"is_ctrl_vq(ndev, vq) %d\n",
			pop_get_nid(), getpid(), popcorn_gettid(), __FILE__, __func__,
			init_vq, vq, vq / 2, ndev->vhost_fds[(vq / 2)],
			is_ctrl_vq(ndev, vq));
#endif

	compat__remove_message(compat_id);

	net_queue	= &ndev->queues[vq];
	net_queue->id	= vq;
	net_queue->ndev	= ndev;
	queue		= &net_queue->vq;
	queue->pfn	= pfn;
	p		= virtio_get_vq(kvm, queue->pfn, page_size);

	vring_init(&queue->vring, VIRTIO_NET_QUEUE_SIZE, p, align); /* cannot locat??? */
	virtio_init_device_vq(&ndev->vdev, queue); /* pophype can skip */

	mutex_init(&net_queue->lock);
	pthread_cond_init(&net_queue->cond, NULL);
	if (is_ctrl_vq(ndev, vq)) {
#ifdef CONFIG_POPCORN_HYPE
		POP_PRINTF("\t\t[net %d/%d/%d] %s: %s() %p: vq %u creates "
				"\"virtio_net_ctrl_thread\"\n", pop_get_nid(), getpid(),
				popcorn_gettid(), __FILE__, __func__, init_vq, vq);
		//BUG_ON(pop_get_nid() > 0 && "Popcorn cannot create threads at remote");
		int nid = net_queue->to_nid = pop_get_nid();
		migrate(0, NULL, NULL); /************* migrate to 0 *******/
#endif
		pthread_create(&net_queue->thread, NULL, virtio_net_ctrl_thread,
			       net_queue);
#ifdef CONFIG_POPCORN_HYPE
		migrate(nid, NULL, NULL); /************* migrate back **************/
		/* but in ths special cast, no need */
#endif

		return 0;
	} else if (ndev->vhost_fd == 0 ) {
		if (vq & 1) {
#ifdef CONFIG_POPCORN_HYPE
			BUG_ON("NEVER REACHED!!!!");
			//POP_PRINTF("\t\t[net %d/%d] %s: %s() %p: vq %u creates "
			//		"\"virtio_net_tx_thread\"\n", pop_get_nid(),
			//		popcorn_gettid(), __FILE__, __func__, init_vq, vq);
			//BUG_ON(pop_get_nid() > 0 && "Popcorn cannot create threads at remote");
#endif
			pthread_create(&net_queue->thread, NULL,
				       virtio_net_tx_thread, net_queue);
		} else {
#ifdef CONFIG_POPCORN_HYPE
			BUG_ON("NEVER REACHED!!!!");
			//POP_PRINTF("\t\t[net %d/%d] %s: %s() %p: vq %u creates "
			//		"\"virtio_net_rx_thread\"\n", pop_get_nid(),
			//		popcorn_gettid(), __FILE__, __func__, init_vq, vq);
			//BUG_ON(pop_get_nid() > 0 && "Popcorn cannot create threads at remote");
#endif
			pthread_create(&net_queue->thread, NULL,
				       virtio_net_rx_thread, net_queue);
		}
		return 0;
	}

	if (queue->endian != VIRTIO_ENDIAN_HOST)
		die_perror("VHOST requires the same endianness in guest and host");

	state.num = queue->vring.num; // number of decriptors
	r = ioctl(ndev->vhost_fds[(vq / 2)], VHOST_SET_VRING_NUM, &state);
	if (r < 0)
		die_perror("VHOST_SET_VRING_NUM failed");
	state.num = 0; // descriptors base
	r = ioctl(ndev->vhost_fds[(vq / 2)], VHOST_SET_VRING_BASE, &state);
	if (r < 0)
		die_perror("VHOST_SET_VRING_BASE failed");

	addr = (struct vhost_vring_addr) {
		.index = (vq % 2),
		.desc_user_addr = (u64)(unsigned long)queue->vring.desc,
		.avail_user_addr = (u64)(unsigned long)queue->vring.avail,
		.used_user_addr = (u64)(unsigned long)queue->vring.used,
	};

	r = ioctl(ndev->vhost_fds[(vq /2)], VHOST_SET_VRING_ADDR, &addr);
	if (r < 0)
		die_perror("VHOST_SET_VRING_ADDR failed");

	return 0;
}

static void exit_vq(struct kvm *kvm, void *dev, u32 vq)
{
	struct net_dev *ndev = dev;
	struct net_dev_queue *queue = &ndev->queues[vq];

	if (!is_ctrl_vq(ndev, vq) && queue->gsi) {
		irq__del_irqfd(kvm, queue->gsi, queue->irqfd);
		close(queue->irqfd);
		queue->gsi = queue->irqfd = 0;
	}

	/*
	 * TODO: vhost reset owner. It's the only way to cleanly stop vhost, but
	 * we can't restart it at the moment.
	 */
	if (ndev->vhost_fd && !is_ctrl_vq(ndev, vq)) {
		pr_warning("Cannot reset VHOST queue");
		ioctl(ndev->vhost_fds[(vq / 2)], VHOST_RESET_OWNER);
		return;
	}

	/*
	 * Threads are waiting on cancellation points (readv or
	 * pthread_cond_wait) and should stop gracefully.
	 */
	pthread_cancel(queue->thread);
	pthread_join(queue->thread, NULL);
}

static void notify_vq_gsi(struct kvm *kvm, void *dev, u32 vq, u32 gsi)
{
	struct net_dev *ndev = dev;
	struct net_dev_queue *queue = &ndev->queues[vq];
	struct vhost_vring_file file;
	int r;

	if (ndev->vhost_fd == 0)
		return;

	file = (struct vhost_vring_file) {
		.index	= (vq % 2),
		.fd	= eventfd(0, 0),
	};

#ifdef CONFIG_POPCORN_HYPE
    POP_PRINTF("\tnet: *[%d] %s(): called at runtime(kernel init): "
			"VIRTIO_MSI_QUEUE_VECTOR(22)-> "
			"eventfd() file.fd fd %d (?/SYSC_eventfd2) "
			"op on fd %d (vhost_fd) ioctl(KVM_IRQFD)\n"
			"\t\t==[[host notify  host->guest]]\n"
			"\t\t\tThis directly registers a kvmirq in Kernel\n\n",
			pop_get_nid(), __func__, file.fd, ndev->vhost_fd);
#endif

	/* ioctl(KVM_IRQFD) */
	r = irq__add_irqfd(kvm, gsi, file.fd, -1); /* x86: irq.c irq__common_add_irqfd */
	if (r < 0)
		die_perror("KVM_IRQFD failed");

	queue->irqfd = file.fd;
	queue->gsi = gsi;
	
	VHOSTPK("notify_vq_gsi: vq=%d, gsi=%d, irqfd=%d\n", vq, gsi, file.fd);

#ifdef CONFIG_POPCORN_HYPE
    POP_PRINTF("\tnet: *[%d] %s(): ioctl(ndev->vhost_fds[%d] %d, "
			"VHOST_SET_VRING_CALL) TODO pophype replay kerneldata\n",
			pop_get_nid(), __func__, vq / 2, ndev->vhost_fds[(vq / 2)]);
#endif
	r = ioctl(ndev->vhost_fds[(vq / 2)], VHOST_SET_VRING_CALL, &file);
	if (r < 0)
		die_perror("VHOST_SET_VRING_CALL failed");

	if (ndev->queue_pairs > 1)
		file.fd = ndev->tap_fds[(vq / 2)];
	else
		file.fd = ndev->tap_fd;
	//file.fd = ndev->tap_fd;
#ifdef CONFIG_POPCORN_HYPE
    POP_PRINTF("\tnet: *[%d] %s(): file.fd = ndev->tap_fd = fd %d\n",
			pop_get_nid(), __func__, ndev->tap_fd);
    POP_PRINTF("\tnet: *[%d] %s(): ioctl(ndev->vhost_fds[%d] %d, VHOST_NET_SET_BACKEND) "
			"to register fd %d\n",
			pop_get_nid(), __func__, vq / 2, ndev->vhost_fds[(vq / 2)], ndev->tap_fd);
#endif
	r = ioctl(ndev->vhost_fds[(vq / 2)], VHOST_NET_SET_BACKEND, &file);
	if (r != 0)
		die("VHOST_NET_SET_BACKEND failed %d", errno);

}

static void notify_vq_eventfd(struct kvm *kvm, void *dev, u32 vq, u32 efd)
{
	struct net_dev *ndev = dev;
	struct vhost_vring_file file = {
		.index	= (vq % 2),
		.fd	= efd,
	};
	int r;

#ifdef CONFIG_POPCORN_HYPE
     POP_PRINTF("\tnet: [%d] %s %s(): called at runtime(kernel init): "
	 		"op on fd %d (vhost_fd)\n"
			"\t\t==[[guest notify guest->host]]\n",
			pop_get_nid(), __FILE__, __func__, ndev->vhost_fd);
#endif

	if (ndev->vhost_fd == 0 || is_ctrl_vq(ndev, vq)) {
		return;
	}

	r = ioctl(ndev->vhost_fds[(vq / 2)], VHOST_SET_VRING_KICK, &file);
	if (r < 0)
		die_perror("VHOST_SET_VRING_KICK failed");
}

static int notify_vq(struct kvm *kvm, void *dev, u32 vq)
{
	struct net_dev *ndev = dev;

	virtio_net_handle_callback(kvm, ndev, vq);

	return 0;
}

static struct virt_queue *get_vq(struct kvm *kvm, void *dev, u32 vq)
{
	struct net_dev *ndev = dev;

	return &ndev->queues[vq].vq;
}

static int get_size_vq(struct kvm *kvm, void *dev, u32 vq)
{
	/* FIXME: dynamic */
	return VIRTIO_NET_QUEUE_SIZE;
}

static int set_size_vq(struct kvm *kvm, void *dev, u32 vq, int size)
{
	/* FIXME: dynamic */
	return size;
}

static int get_vq_count(struct kvm *kvm, void *dev)
{
	struct net_dev *ndev = dev;

	return ndev->queue_pairs * 2 + 1;
}

static struct virtio_ops net_dev_virtio_ops = {
	.get_config		= get_config,
	.get_host_features	= get_host_features,
	.set_guest_features	= set_guest_features,
	.get_vq_count		= get_vq_count,
	.init_vq		= init_vq,
	.exit_vq		= exit_vq,
	.get_vq			= get_vq,
	.get_size_vq		= get_size_vq,
	.set_size_vq		= set_size_vq,
	.notify_vq		= notify_vq,
	.notify_vq_gsi		= notify_vq_gsi,
	.notify_vq_eventfd	= notify_vq_eventfd,
	.notify_status		= notify_status,
};

static void virtio_net__vhost_init(struct kvm *kvm, struct net_dev *ndev)
{
	struct kvm_mem_bank *bank;
	struct vhost_memory *mem;
	int r, i;


	mem = calloc(1, sizeof(*mem) + kvm->mem_slots * sizeof(struct vhost_memory_region));
	if (mem == NULL)
		die("Failed allocating memory for vhost memory map");

	i = 0;
	list_for_each_entry(bank, &kvm->mem_banks, list) {
		mem->regions[i] = (struct vhost_memory_region) {
			.guest_phys_addr = bank->guest_phys_addr,
			.memory_size	 = bank->size,
			.userspace_addr	 = (unsigned long)bank->host_addr,
		};
		i++;
	}
	mem->nregions = i;

	int nid = pop_get_nid();

	/* multiqueue vhost */
	for (i = 0; ((u32)i < ndev->queue_pairs) &&
			(i < VIRTIO_NET_NUM_QUEUES); i++) {
		ndev->vhost_fds[i] = open("/dev/vhost-net", O_RDWR);
		if (ndev->vhost_fds[i] < 0)
			die_perror("Failed openning vhost-net device");
#ifdef CONFIG_POPCORN_HYPE
		POP_PRINTF("\t\t[*] net: %s(): [[[VHOST]]] vhost_fds[i] fd %d (pophype TODO)\n",
									__func__, ndev->vhost_fds[i]);
#endif
		VHOSTPK("vhost_pk: vhost-net fd[%d]=%d on node %d\n", i, ndev->vhost_fds[i], nid);
#ifdef CONFIG_POPCORN_HYPE
		POP_PRINTF("\t\t[*] net: %s(): [VHOST] vhost_fds[i] fd %d "
					"ioctl(VHOST_SET_OWNER) (pophype TODO)\n",
								__func__, ndev->vhost_fds[i]);
#endif
		r = ioctl(ndev->vhost_fds[i], VHOST_SET_OWNER);
		if (r != 0)
			die_perror("VHOST_SET_OWNER failed");

#ifdef CONFIG_POPCORN_HYPE
		POP_PRINTF("\t\t[*] net: %s(): [VHOST] vhost_fds[i] fd %d "
					"ioctl(VHOST_SET_MEM_TABLE) (pophype TODO)\n",
									__func__, ndev->vhost_fds[i]);
#endif
		r = ioctl(ndev->vhost_fds[i], VHOST_SET_MEM_TABLE, mem);
		if (r != 0)
			die_perror("VHOST_SET_MEM_TABLE failed");
	}
	ndev->vhost_fd = ndev->vhost_fds[0];

	ndev->vdev.use_vhost = true;

	free(mem);
}

static inline void str_to_mac(const char *str, char *mac)
{
	sscanf(str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
		mac, mac+1, mac+2, mac+3, mac+4, mac+5);
}
static int set_net_param(struct kvm *kvm, struct virtio_net_params *p,
			const char *param, const char *val)
{
	if (strcmp(param, "guest_mac") == 0) {
		str_to_mac(val, p->guest_mac);
	} else if (strcmp(param, "mode") == 0) {
		if (!strncmp(val, "user", 4)) {
			int i;

			for (i = 0; i < kvm->cfg.num_net_devices; i++)
				if (kvm->cfg.net_params[i].mode == NET_MODE_USER)
					die("Only one usermode network device allowed at a time");
			p->mode = NET_MODE_USER;
		} else if (!strncmp(val, "tap", 3)) {
			p->mode = NET_MODE_TAP;
		} else if (!strncmp(val, "none", 4)) {
			kvm->cfg.no_net = 1;
			return -1;
		} else
			die("Unknown network mode %s, please use user, tap or none", kvm->cfg.network);
	} else if (strcmp(param, "script") == 0) {
		p->script = strdup(val);
	} else if (strcmp(param, "downscript") == 0) {
		p->downscript = strdup(val);
	} else if (strcmp(param, "guest_ip") == 0) {
		p->guest_ip = strdup(val);
	} else if (strcmp(param, "host_ip") == 0) {
		p->host_ip = strdup(val);
	} else if (strcmp(param, "trans") == 0) {
		p->trans = strdup(val);
	} else if (strcmp(param, "tapif") == 0) {
		p->tapif = strdup(val);
	} else if (strcmp(param, "vhost") == 0) {
		p->vhost = atoi(val);
	} else if (strcmp(param, "fd") == 0) {
		p->fd = atoi(val);
	} else if (strcmp(param, "mq") == 0) {
		p->mq = atoi(val);
	} else
		die("Unknown network parameter %s", param);

	return 0;
}

int netdev_parser(const struct option *opt, const char *arg, int unset)
{
	struct virtio_net_params p;
	char *buf = NULL, *cmd = NULL, *cur = NULL;
	bool on_cmd = true;
	struct kvm *kvm = opt->ptr;

	if (arg) {
		buf = strdup(arg);
		if (buf == NULL)
			die("Failed allocating new net buffer");
		cur = strtok(buf, ",=");
	}

	p = (struct virtio_net_params) {
		.guest_ip	= DEFAULT_GUEST_ADDR,
		.host_ip	= DEFAULT_HOST_ADDR,
		.script		= DEFAULT_SCRIPT,
		.downscript	= DEFAULT_SCRIPT,
		.mode		= NET_MODE_TAP,
	};

	str_to_mac(DEFAULT_GUEST_MAC, p.guest_mac);
	p.guest_mac[5] += kvm->cfg.num_net_devices;

	while (cur) {
		if (on_cmd) {
			cmd = cur;
		} else {
			if (set_net_param(kvm, &p, cmd, cur) < 0)
				goto done;
		}
		on_cmd = !on_cmd;

		cur = strtok(NULL, ",=");
	};

	kvm->cfg.num_net_devices++;

	kvm->cfg.net_params = realloc(kvm->cfg.net_params, kvm->cfg.num_net_devices * sizeof(*kvm->cfg.net_params));
	if (kvm->cfg.net_params == NULL)
		die("Failed adding new network device");

	kvm->cfg.net_params[kvm->cfg.num_net_devices - 1] = p;

done:
	free(buf);
	return 0;
}

static int virtio_net__init_one(struct virtio_net_params *params)
{
	int i, err;
	struct net_dev *ndev;
	struct virtio_ops *ops;
	enum virtio_trans trans = VIRTIO_DEFAULT_TRANS(params->kvm);

#ifdef CONFIG_POPCORN_HYPE
	POP_PRINTF("\t\t[*] %s: %s(): net:\n", __FILE__, __func__);
#endif
	ndev = calloc(1, sizeof(struct net_dev));
	if (ndev == NULL)
		return -ENOMEM;

	ops = malloc(sizeof(*ops));
	if (ops == NULL) {
		err = -ENOMEM;
		goto err_free_ndev;
	}

	list_add_tail(&ndev->list, &ndevs);

	ndev->kvm = params->kvm;
	ndev->params = params;

	mutex_init(&ndev->mutex);
	ndev->queue_pairs = max(1, min(VIRTIO_NET_NUM_QUEUES, params->mq));
	ndev->config.status = VIRTIO_NET_S_LINK_UP;
	if (ndev->queue_pairs > 1)
		ndev->config.max_virtqueue_pairs = ndev->queue_pairs;

	for (i = 0 ; i < 6 ; i++) {
		ndev->config.mac[i]		= params->guest_mac[i];
		ndev->info.guest_mac.addr[i]	= params->guest_mac[i];
		ndev->info.host_mac.addr[i]	= params->host_mac[i];
	}
#ifdef CONFIG_POPCORN_HYPE
	POP_PRINTF("\t\t[*] %s: %s(): net: pophype multiqueue = "
								"ndev->queue_pairs = %d\n",
								__FILE__, __func__, ndev->queue_pairs);
#endif
	ndev->mode = params->mode;
	if (ndev->mode == NET_MODE_TAP) {
		ndev->ops = &tap_ops;
		if (!virtio_net__tap_create(ndev))
			die_perror("You have requested a TAP device, but creation of one has failed because");

#ifdef CONFIG_POPCORN_HYPE
        int i, nid = pop_get_nid();
        BUG_ON(nid);
		POP_PRINTF("\n\n\n\t======= popbroadcast "
					"- virtio_net__tap_create() ===========\n");
        for (i = 1; i < nodes; i++) {
            if (i == nid) { continue; }
			POP_PRINTF("\t\t1 init [*] net: %s: %s(): "
					"replaying virtio_net__tap_create() "
					"at remote %d\n", __FILE__, __func__, i);
            migrate(i, NULL, NULL); /**********************************/

			//POP_DPRINTF(pop_get_nid(), "\t\t[from *] => <%d> [%d] %s(): "
            //            "going to call virtio_net__tap_create()\n\n",
            //            pop_get_nid(), popcorn_gettid(), __func__);
			POP_PRINTF("\t\t1 init [from *] => <%d> [%d] %s(): "
                        "replaying virtio_net__tap_create()\n\n",
                        pop_get_nid(), popcorn_gettid(), __func__);
			if (!virtio_net__tap_create(ndev)) {
				die_perror("You have requested a TAP device, "
							"but creation of one has failed because");
			}

            migrate(0, NULL, NULL); /**********************************/
			POP_PRINTF("\t\t[*] net: virtio_net__tap_create() done at remote %d\n", i);
        }
		POP_PRINTF("\t======= popbroadcast done ===========\n\n\n");
#endif
	} else {
		ndev->info.host_ip		= ntohl(inet_addr(params->host_ip));
		ndev->info.guest_ip		= ntohl(inet_addr(params->guest_ip));
		ndev->info.guest_netmask	= ntohl(inet_addr("255.255.255.0"));
		ndev->info.buf_nr		= 20,
		ndev->ops = &uip_ops;
		uip_static_init(&ndev->info);

#ifdef CONFIG_POPCORN_HYPE
		POP_PRINTF("\t<*> net: %s(): guest ip \"%s\"\n",
							__func__, params->guest_ip);
#endif
	}

	*ops = net_dev_virtio_ops;

	if (params->trans) {
		if (strcmp(params->trans, "mmio") == 0)
			trans = VIRTIO_MMIO;
		else if (strcmp(params->trans, "pci") == 0)
			trans = VIRTIO_PCI;
		else
			pr_warning("virtio-net: Unknown transport method : %s, "
				   "falling back to %s.", params->trans,
				   virtio_trans_name(trans));
	}


#ifdef CONFIG_POPCORN_HYPE
	POP_PRINTF("\t1.5 init [*] net: calling virtio_init()\n");
	POP_PRINTF("\tnet: this one is important "
			"for later exit_mmio to init in runtime\n");
	//POP_PRINTF("\tnet: WORKING virtio_init()\n");
#endif
	virtio_init(params->kvm, ndev, &ndev->vdev, ops, trans,
		    PCI_DEVICE_ID_VIRTIO_NET, VIRTIO_ID_NET, PCI_CLASS_NET);

#ifdef CONFIG_POPCORN_HYPE
	int nid = pop_get_nid();
	BUG_ON(nid);
	POP_PRINTF("\n\n\n======= popbroadcast - virtio_init() ===========\n");
	for (i = 1; i < nodes; i++) {
		if (i == nid) { continue; }
		POP_PRINTF("1.5 init [*] net: %s: %s(): "
				"replaying virtio_init() at [%d]\n", __FILE__, __func__, i);
		migrate(i, NULL, NULL); /**********************************/

		POP_DPRINTF(pop_get_nid(), "\t\t1.5 [from *] => <%d> [%d] %s(): "
					"calling virtio_init()\n\n",
					pop_get_nid(), popcorn_gettid(), __func__);
		POP_PRINTF("\t\t1.5 init [from *] => <%d> [%d] %s(): "
					"calling virtio_init()\n\n",
					pop_get_nid(), popcorn_gettid(), __func__);
#if HOST_NET
//		virtio_init(params->kvm, ndev, &ndev->vdev, ops, trans,
//				PCI_DEVICE_ID_VIRTIO_NET, VIRTIO_ID_NET, PCI_CLASS_NET);
#endif
		POP_PRINTF("\nTODO virtio_pci__init cannot be recalled on remote nodes "
								"because of pci_get_io_space_block()\n\n");

		migrate(0, NULL, NULL); /**********************************/
		POP_PRINTF("[*] net: virtio_init() done at remote %d\n", i);
	}
	POP_PRINTF("======= popbroadcast done ===========\n\n\n");
#endif


	if (params->vhost) {
#ifdef CONFIG_POPCORN_HYPE
		//POP_PRINTF("==========================================\n"
		//		"\t[*] net: going to call virtio_net__vhost_init()\n");
		POP_PRINTF("\t[*] net: calling virtio_net__vhost_init()\n");
#endif
		virtio_net__vhost_init(params->kvm, ndev);
#ifdef CONFIG_POPCORN_HYPE
		//POP_PRINTF("\t[*] net: virtio_net__vhost_init() done\n"
		//		"==========================================\n"
		//		"==========================================\n\n\n");

        int i, nid = pop_get_nid();
        BUG_ON(nid);
		POP_PRINTF("\n\n\n======= popbroadcast "
						"- virtio_net__vhost_init() ===========\n");
        for (i = 1; i < nodes; i++) {
            if (i == nid) { continue; }
				POP_PRINTF("2 init [*] net: %s: %s(): replaying "
						"virtio_net__vhost_init() "
						"at [%d]\n", __FILE__, __func__, i);
            migrate(i, NULL, NULL); /**********************************/

			//POP_DPRINTF(pop_get_nid(), "\t\t2 init [from *] => <%d> [%d] %s(): "
            //            "going to call virtio_net__vhost_init()\n\n",
            //            pop_get_nid(), popcorn_gettid(), __func__);
			POP_PRINTF("\t\t2 init [from *] => <%d> [%d] %s(): "
                        "replaying virtio_net__vhost_init()\n\n",
                        pop_get_nid(), popcorn_gettid(), __func__);
			POP_PRINTF("\t\t3 init done, then run-time\n");
			virtio_net__vhost_init(params->kvm, ndev);

            migrate(0, NULL, NULL); /**********************************/
			POP_PRINTF("[*] net: virtio_net__vhost_init() done at remote %d\n", i);
        }
		POP_PRINTF("======= popbroadcast done ===========\n\n\n");
#endif
	}

	if (compat_id == -1)
		compat_id = virtio_compat_add_message("virtio-net", "CONFIG_VIRTIO_NET");

	return 0;

err_free_ndev:
	free(ndev);
	return err;
}

int virtio_net__init(struct kvm *kvm)
{
#ifdef CONFIG_POPCORN_HYPE
	POP_PRINTF("\t\t=========================================\n");
	POP_PRINTF("\t\t================== net ==================\n");
	POP_PRINTF("\t\t=========================================\n");
    if (pop_get_nid()) {
		POP_DPRINTF(pop_get_nid(), "\t<%d> %s(): NOT NET!!!! SKIP..."
					"BUT no vhost kernel meta\n", pop_get_nid(), __func__);
    	//POP_PRINTF("\t[net %d/%d] %s(): NO NET!!!!! SKIP......................\n",
		//						pop_get_nid(), popcorn_gettid(), __func__);
        return 0;
	} else {
		POP_PRINTF("\t<*> Why creating net will cause remote kvm__emulate_mmio?\n");
		POP_PRINTF("\t<*> If we let origin start to init net, we will start to see "
			"mmio since the net device is a pci device causing mmio faults.\n");
	}

#endif
	int i;

	for (i = 0; i < kvm->cfg.num_net_devices; i++) {
		kvm->cfg.net_params[i].kvm = kvm;
#ifdef CONFIG_POPCORN_HYPE
		POP_PRINTF("\t<*> %s(): net: create net dev %d\n", __func__, i);
#endif
		virtio_net__init_one(&kvm->cfg.net_params[i]);
	}

	if (kvm->cfg.num_net_devices == 0 && kvm->cfg.no_net == 0) {
		static struct virtio_net_params net_params;

		net_params = (struct virtio_net_params) {
			.guest_ip	= kvm->cfg.guest_ip,
			.host_ip	= kvm->cfg.host_ip,
			.kvm		= kvm,
			.script		= kvm->cfg.script,
			.mode		= NET_MODE_USER,
		};
		str_to_mac(kvm->cfg.guest_mac, net_params.guest_mac);
		str_to_mac(kvm->cfg.host_mac, net_params.host_mac);

		virtio_net__init_one(&net_params);
	}

	return 0;
}
virtio_dev_init(virtio_net__init);

int virtio_net__exit(struct kvm *kvm)
{
	struct virtio_net_params *params;
	struct net_dev *ndev;
	struct list_head *ptr;

	list_for_each(ptr, &ndevs) {
		ndev = list_entry(ptr, struct net_dev, list);
		params = ndev->params;
		/* Cleanup any tap device which attached to bridge */
		if (ndev->mode == NET_MODE_TAP &&
		    strcmp(params->downscript, "none"))
			virtio_net_exec_script(params->downscript, ndev->tap_name);
	}
	return 0;
}
virtio_dev_exit(virtio_net__exit);
