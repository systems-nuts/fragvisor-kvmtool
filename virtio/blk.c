#include "kvm/virtio-blk.h"

#include "kvm/virtio-pci-dev.h"
#include "kvm/disk-image.h"
#include "kvm/mutex.h"
#include "kvm/util.h"
#include "kvm/kvm.h"
#include "kvm/pci.h"
#include "kvm/threadpool.h"
#include "kvm/ioeventfd.h"
#include "kvm/guest_compat.h"
#include "kvm/virtio-pci.h"
#include "kvm/virtio.h"

#include <linux/virtio_ring.h>
#include <linux/virtio_blk.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/types.h>
#include <pthread.h>

#include <popcorn/utils.h>

#define VIRTIO_BLK_MAX_DEV		4

/*
 * the header and status consume too entries
 */
#define DISK_SEG_MAX			(VIRTIO_BLK_QUEUE_SIZE - 2)
#define VIRTIO_BLK_QUEUE_SIZE		256
#define NUM_VIRT_QUEUES			1

struct blk_dev_req {
	struct virt_queue		*vq;
	struct blk_dev			*bdev;
	struct iovec			iov[VIRTIO_BLK_QUEUE_SIZE];
	u16				out, in, head;
	struct kvm			*kvm;
};

struct blk_dev {
	struct mutex			mutex;

	struct list_head		list;

	struct virtio_device		vdev;
	struct virtio_blk_config	blk_config;
	struct disk_image		*disk;
	u32				features;

	struct virt_queue		vqs[NUM_VIRT_QUEUES];
	struct blk_dev_req		reqs[VIRTIO_BLK_QUEUE_SIZE];

	pthread_t			io_thread;
	int				io_efd;

	struct kvm			*kvm;
};

static LIST_HEAD(bdevs);
static int compat_id = -1;

void virtio_blk_complete(void *param, long len)
{
	struct blk_dev_req *req = param;
	struct blk_dev *bdev = req->bdev;
	int queueid = req->vq - bdev->vqs;
	u8 *status;

	/* status */
	status	= req->iov[req->out + req->in - 1].iov_base;
	*status	= (len < 0) ? VIRTIO_BLK_S_IOERR : VIRTIO_BLK_S_OK;

	mutex_lock(&bdev->mutex);
	virt_queue__set_used_elem(req->vq, req->head, len);
	mutex_unlock(&bdev->mutex);

	if (virtio_queue__should_signal(&bdev->vqs[queueid]))
		bdev->vdev.ops->signal_vq(req->kvm, &bdev->vdev, queueid);
}

static void virtio_blk_do_io_request(struct kvm *kvm, struct virt_queue *vq, struct blk_dev_req *req)
{
	struct virtio_blk_outhdr *req_hdr;
	ssize_t block_cnt;
	struct blk_dev *bdev;
	struct iovec *iov;
	u16 out, in;
	u32 type;
	u64 sector;

	block_cnt	= -1;
	bdev		= req->bdev;
	iov		= req->iov;
	out		= req->out;
	in		= req->in;
	req_hdr		= iov[0].iov_base;

	type = virtio_guest_to_host_u32(vq, req_hdr->type);
	sector = virtio_guest_to_host_u64(vq, req_hdr->sector);

	switch (type) {
	case VIRTIO_BLK_T_IN:
		block_cnt = disk_image__read(bdev->disk, sector,
				iov + 1, in + out - 2, req);
		break;
	case VIRTIO_BLK_T_OUT:
		block_cnt = disk_image__write(bdev->disk, sector,
				iov + 1, in + out - 2, req);
		break;
	case VIRTIO_BLK_T_FLUSH:
		block_cnt = disk_image__flush(bdev->disk);
		virtio_blk_complete(req, block_cnt);
		break;
	case VIRTIO_BLK_T_GET_ID:
		block_cnt = VIRTIO_BLK_ID_BYTES;
		disk_image__get_serial(bdev->disk,
				(iov + 1)->iov_base, &block_cnt);
		virtio_blk_complete(req, block_cnt);
		break;
	default:
		pr_warning("request type %d", type);
		block_cnt	= -1;
		break;
	}
}

static void virtio_blk_do_io(struct kvm *kvm, struct virt_queue *vq, struct blk_dev *bdev)
{
	struct blk_dev_req *req;
	u16 head;

	while (virt_queue__available(vq)) {
		head		= virt_queue__pop(vq);
		req		= &bdev->reqs[head];
		req->head	= virt_queue__get_head_iov(vq, req->iov, &req->out,
					&req->in, head, kvm);
		req->vq		= vq;

		virtio_blk_do_io_request(kvm, vq, req);
	}
}

static u8 *get_config(struct kvm *kvm, void *dev)
{
	struct blk_dev *bdev = dev;

	return ((u8 *)(&bdev->blk_config));
}

static u32 get_host_features(struct kvm *kvm, void *dev)
{
	return	1UL << VIRTIO_BLK_F_SEG_MAX
		| 1UL << VIRTIO_BLK_F_FLUSH
		| 1UL << VIRTIO_RING_F_EVENT_IDX
		| 1UL << VIRTIO_RING_F_INDIRECT_DESC;
}

static void set_guest_features(struct kvm *kvm, void *dev, u32 features)
{
	struct blk_dev *bdev = dev;
	struct virtio_blk_config *conf = &bdev->blk_config;
	struct virtio_blk_geometry *geo = &conf->geometry;

	bdev->features = features;

	conf->capacity = virtio_host_to_guest_u64(&bdev->vdev, conf->capacity);
	conf->size_max = virtio_host_to_guest_u32(&bdev->vdev, conf->size_max);
	conf->seg_max = virtio_host_to_guest_u32(&bdev->vdev, conf->seg_max);

	/* Geometry */
	geo->cylinders = virtio_host_to_guest_u16(&bdev->vdev, geo->cylinders);

	conf->blk_size = virtio_host_to_guest_u32(&bdev->vdev, conf->blk_size);
	conf->min_io_size = virtio_host_to_guest_u16(&bdev->vdev, conf->min_io_size);
	conf->opt_io_size = virtio_host_to_guest_u32(&bdev->vdev, conf->opt_io_size);
}

static void notify_status(struct kvm *kvm, void *dev, u32 status)
{
}

static void *virtio_blk_thread(void *dev)
{
	struct blk_dev *bdev = dev;
	u64 data;
	int r;

	kvm__set_thread_name("virtio-blk-io");
#ifdef CONFIG_POPCORN_HYPE
	POP_PRINTF("\t[%d/%d] %s(): this thread \"virtio-blk-io\"\n",
					pop_get_nid(), popcorn_gettid(), __func__);
#endif

	while (1) {
		r = read(bdev->io_efd, &data, sizeof(u64));
		if (r < 0)
			continue;
		virtio_blk_do_io(bdev->kvm, &bdev->vqs[0], bdev);
	}

	pthread_exit(NULL);
	return NULL;
}

static int init_vq(struct kvm *kvm, void *dev, u32 vq, u32 page_size, u32 align,
		   u32 pfn)
{
	unsigned int i;
	struct blk_dev *bdev = dev;
	struct virt_queue *queue;
	void *p;

	compat__remove_message(compat_id);

	queue		= &bdev->vqs[vq];
	queue->pfn	= pfn;
	p		= virtio_get_vq(kvm, queue->pfn, page_size);

	vring_init(&queue->vring, VIRTIO_BLK_QUEUE_SIZE, p, align);
	virtio_init_device_vq(&bdev->vdev, queue);

	if (vq != 0)
		return 0;

	for (i = 0; i < ARRAY_SIZE(bdev->reqs); i++) {
		bdev->reqs[i] = (struct blk_dev_req) {
			.bdev = bdev,
			.kvm = kvm,
		};
	}

	mutex_init(&bdev->mutex);
	bdev->io_efd = eventfd(0, 0);
	if (bdev->io_efd < 0)
		return -errno;

#ifdef CONFIG_POPCORN_HYPE
	printf("<%d> %s %s(): init_vq %p\n", pop_get_nid(), __FILE__,  __func__, init_vq);
	printf("<%d> %s(): eventfd() bdev->io_efd fd %d (17/SYSC_eventfd2)\n\n",
										pop_get_nid(), __func__, bdev->io_efd);
	dump_stack();
	// from virtio_pci__io_out
	//
	// kvm__emulate_io
	// 		ret = ops->io_out(entry, vcpu, port, ptr, size);
	// KVM_EXIT_IO
#endif

#ifdef CONFIG_POPCORN_HYPE
	if (!pop_get_nid()) {
		printf("origin: creating virtio blk thread\n");
		if (pthread_create(&bdev->io_thread, NULL, virtio_blk_thread, bdev))
			return -errno;
	} else
		printf("remote: skip creating virtio blk thread\n");
#else
	if (pthread_create(&bdev->io_thread, NULL, virtio_blk_thread, bdev))
		return -errno;
#endif

	return 0;
}

static void exit_vq(struct kvm *kvm, void *dev, u32 vq)
{
	struct blk_dev *bdev = dev;

	if (vq != 0)
		return;

	close(bdev->io_efd);
	pthread_cancel(bdev->io_thread);
	pthread_join(bdev->io_thread, NULL);
}

static int notify_vq(struct kvm *kvm, void *dev, u32 vq)
{
	struct blk_dev *bdev = dev;
	u64 data = 1;
	int r;

	r = write(bdev->io_efd, &data, sizeof(data));
	if (r < 0)
		return r;

	return 0;
}

static struct virt_queue *get_vq(struct kvm *kvm, void *dev, u32 vq)
{
	struct blk_dev *bdev = dev;

	return &bdev->vqs[vq];
}

static int get_size_vq(struct kvm *kvm, void *dev, u32 vq)
{
	/* FIXME: dynamic */
	return VIRTIO_BLK_QUEUE_SIZE;
}

static int set_size_vq(struct kvm *kvm, void *dev, u32 vq, int size)
{
	/* FIXME: dynamic */
	return size;
}

static int get_vq_count(struct kvm *kvm, void *dev)
{
	return NUM_VIRT_QUEUES;
}

static struct virtio_ops blk_dev_virtio_ops = {
	.get_config		= get_config,
	.get_host_features	= get_host_features,
	.set_guest_features	= set_guest_features,
	.get_vq_count		= get_vq_count,
	.init_vq		= init_vq,
	.exit_vq		= exit_vq,
	.notify_status		= notify_status,
	.notify_vq		= notify_vq,
	.get_vq			= get_vq,
	.get_size_vq		= get_size_vq,
	.set_size_vq		= set_size_vq,
};

static int virtio_blk__init_one(struct kvm *kvm, struct disk_image *disk)
{
	struct blk_dev *bdev;

	if (!disk)
		return -EINVAL;

	bdev = calloc(1, sizeof(struct blk_dev));
	if (bdev == NULL)
		return -ENOMEM;

	*bdev = (struct blk_dev) {
		.disk			= disk,
		.blk_config		= (struct virtio_blk_config) {
			.capacity	= disk->size / SECTOR_SIZE,
			.seg_max	= DISK_SEG_MAX,
		},
		.kvm			= kvm,
	};

	virtio_init(kvm, bdev, &bdev->vdev, &blk_dev_virtio_ops,
		    VIRTIO_DEFAULT_TRANS(kvm), PCI_DEVICE_ID_VIRTIO_BLK,
		    VIRTIO_ID_BLOCK, PCI_CLASS_BLK);

	list_add_tail(&bdev->list, &bdevs);

	disk_image__set_callback(bdev->disk, virtio_blk_complete);

	if (compat_id == -1)
		compat_id = virtio_compat_add_message("virtio-blk", "CONFIG_VIRTIO_BLK");

	return 0;
}

static int virtio_blk__exit_one(struct kvm *kvm, struct blk_dev *bdev)
{
	list_del(&bdev->list);
	free(bdev);

	return 0;
}

int virtio_blk__init(struct kvm *kvm)
{
#ifdef CONFIG_POPCORN_HYPE
    if (pop_get_nid()) {
		POP_PRINTF("\t[%d/%d] %s(): SKIP..........................\n",
						pop_get_nid(), popcorn_gettid(), __func__);
		return 0;
	}
#endif
	int i, r = 0;

	for (i = 0; i < kvm->nr_disks; i++) {
		if (kvm->disks[i]->wwpn)
			continue;
		r = virtio_blk__init_one(kvm, kvm->disks[i]);
		if (r < 0)
			goto cleanup;
	}

	return 0;
cleanup:
	return virtio_blk__exit(kvm);
}
virtio_dev_init(virtio_blk__init);

int virtio_blk__exit(struct kvm *kvm)
{
	while (!list_empty(&bdevs)) {
		struct blk_dev *bdev;

		bdev = list_first_entry(&bdevs, struct blk_dev, list);
		virtio_blk__exit_one(kvm, bdev);
	}

	return 0;
}
virtio_dev_exit(virtio_blk__exit);
