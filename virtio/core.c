#include <linux/virtio_ring.h>
#include <linux/types.h>
#include <sys/uio.h>
#include <stdlib.h>

#include "kvm/guest_compat.h"
#include "kvm/barrier.h"
#include "kvm/virtio.h"
#include "kvm/virtio-pci.h"
#include "kvm/virtio-mmio.h"
#include "kvm/util.h"
#include "kvm/kvm.h"

#include <popcorn/utils.h>

const char* virtio_trans_name(enum virtio_trans trans)
{
	if (trans == VIRTIO_PCI)
		return "pci";
	else if (trans == VIRTIO_MMIO)
		return "mmio";
	return "unknown";
}

void virt_queue__used_idx_advance(struct virt_queue *queue, u16 jump)
{
	u16 idx = virtio_guest_to_host_u16(queue, queue->vring.used->idx);

	/*
	 * Use wmb to assure that used elem was updated with head and len.
	 * We need a wmb here since we can't advance idx unless we're ready
	 * to pass the used element to the guest.
	 */
	wmb();
	idx += jump;
	queue->vring.used->idx = virtio_host_to_guest_u16(queue, idx);

	/*
	 * Use wmb to assure used idx has been increased before we signal the guest.
	 * Without a wmb here the guest may ignore the queue since it won't see
	 * an updated idx.
	 */
	wmb();
}

struct vring_used_elem *
virt_queue__set_used_elem_no_update(struct virt_queue *queue, u32 head,
				    u32 len, u16 offset)
{
	struct vring_used_elem *used_elem;
	u16 idx = virtio_guest_to_host_u16(queue, queue->vring.used->idx);

	idx += offset;
	used_elem	= &queue->vring.used->ring[idx % queue->vring.num];
	used_elem->id	= virtio_host_to_guest_u32(queue, head);
	used_elem->len	= virtio_host_to_guest_u32(queue, len);

	return used_elem;
}

struct vring_used_elem *virt_queue__set_used_elem(struct virt_queue *queue, u32 head, u32 len)
{
	struct vring_used_elem *used_elem;

	used_elem = virt_queue__set_used_elem_no_update(queue, head, len, 0);
	virt_queue__used_idx_advance(queue, 1);

	return used_elem;
}

static inline bool virt_desc__test_flag(struct virt_queue *vq,
					struct vring_desc *desc, u16 flag)
{
	return !!(virtio_guest_to_host_u16(vq, desc->flags) & flag);
}

/*
 * Each buffer in the virtqueues is actually a chain of descriptors.  This
 * function returns the next descriptor in the chain, or max if we're at the
 * end.
 */
static unsigned next_desc(struct virt_queue *vq, struct vring_desc *desc,
			  unsigned int i, unsigned int max)
{
	unsigned int next;

	/* If this descriptor says it doesn't chain, we're done. */
	if (!virt_desc__test_flag(vq, &desc[i], VRING_DESC_F_NEXT))
		return max;

	next = virtio_guest_to_host_u16(vq, desc[i].next);

	/* Ensure they're not leading us off end of descriptors. */
	return min(next, max);
}

u16 virt_queue__get_head_iov(struct virt_queue *vq, struct iovec iov[], u16 *out, u16 *in, u16 head, struct kvm *kvm)
{
	struct vring_desc *desc;
	u16 idx;
	u16 max;

	idx = head;
	*out = *in = 0;
	max = vq->vring.num;
	desc = vq->vring.desc;

	if (virt_desc__test_flag(vq, &desc[idx], VRING_DESC_F_INDIRECT)) {
		max = virtio_guest_to_host_u32(vq, desc[idx].len) / sizeof(struct vring_desc);
		desc = guest_flat_to_host(kvm, virtio_guest_to_host_u64(vq, desc[idx].addr));
		idx = 0;
	}

	do {
		/* Grab the first descriptor, and check it's OK. */
		iov[*out + *in].iov_len = virtio_guest_to_host_u32(vq, desc[idx].len);
		iov[*out + *in].iov_base = guest_flat_to_host(kvm,
							      virtio_guest_to_host_u64(vq, desc[idx].addr));
		/* If this is an input descriptor, increment that count. */
		if (virt_desc__test_flag(vq, &desc[idx], VRING_DESC_F_WRITE))
			(*in)++;
		else
			(*out)++;
	} while ((idx = next_desc(vq, desc, idx, max)) != max);

	return head;
}

u16 virt_queue__get_iov(struct virt_queue *vq, struct iovec iov[], u16 *out, u16 *in, struct kvm *kvm)
{
	u16 head;

	head = virt_queue__pop(vq);

	return virt_queue__get_head_iov(vq, iov, out, in, head, kvm);
}

/* in and out are relative to guest */
u16 virt_queue__get_inout_iov(struct kvm *kvm, struct virt_queue *queue,
			      struct iovec in_iov[], struct iovec out_iov[],
			      u16 *in, u16 *out)
{
	struct vring_desc *desc;
	u16 head, idx;

	idx = head = virt_queue__pop(queue);
	*out = *in = 0;
	do {
		u64 addr;
		desc = virt_queue__get_desc(queue, idx);
		addr = virtio_guest_to_host_u64(queue, desc->addr);
		if (virt_desc__test_flag(queue, desc, VRING_DESC_F_WRITE)) {
			in_iov[*in].iov_base = guest_flat_to_host(kvm, addr);
			in_iov[*in].iov_len = virtio_guest_to_host_u32(queue, desc->len);
			(*in)++;
		} else {
			out_iov[*out].iov_base = guest_flat_to_host(kvm, addr);
			out_iov[*out].iov_len = virtio_guest_to_host_u32(queue, desc->len);
			(*out)++;
		}
		if (virt_desc__test_flag(queue, desc, VRING_DESC_F_NEXT))
			idx = virtio_guest_to_host_u16(queue, desc->next);
		else
			break;
	} while (1);

	return head;
}

void virtio_exit_vq(struct kvm *kvm, struct virtio_device *vdev,
			   void *dev, int num)
{
	struct virt_queue *vq = vdev->ops->get_vq(kvm, dev, num);

	if (vq->enabled && vdev->ops->exit_vq)
		vdev->ops->exit_vq(kvm, dev, num);
	memset(vq, 0, sizeof(*vq));
}

int virtio__get_dev_specific_field(int offset, bool msix, u32 *config_off)
{
	if (msix) {
		if (offset < 4)
			return VIRTIO_PCI_O_MSIX;
		else
			offset -= 4;
	}

	*config_off = offset;

	return VIRTIO_PCI_O_CONFIG;
}

bool virtio_queue__should_signal(struct virt_queue *vq)
{
	u16 old_idx, new_idx, event_idx;

	if (!vq->use_event_idx) {
		/*
		 * When VIRTIO_RING_F_EVENT_IDX isn't negotiated, interrupt the
		 * guest if it didn't explicitly request to be left alone.
		 */
		return !(virtio_guest_to_host_u16(vq, vq->vring.avail->flags) &
			 VRING_AVAIL_F_NO_INTERRUPT);
	}

	old_idx		= vq->last_used_signalled;
	new_idx		= virtio_guest_to_host_u16(vq, vq->vring.used->idx);
	event_idx	= virtio_guest_to_host_u16(vq, vring_used_event(&vq->vring));

	if (vring_need_event(event_idx, new_idx, old_idx)) {
		vq->last_used_signalled = new_idx;
		return true;
	}

	return false;
}

void virtio_set_guest_features(struct kvm *kvm, struct virtio_device *vdev,
			       void *dev, u32 features)
{
	/* TODO: fail negotiation if features & ~host_features */

	vdev->features = features;
	vdev->ops->set_guest_features(kvm, dev, features);
}

void virtio_notify_status(struct kvm *kvm, struct virtio_device *vdev,
			  void *dev, u8 status)
{
	u32 ext_status = status;

#ifdef CONFIG_POPCORN_HYPE
    POP_PRINTF("\t\t[%d/%d] %s(): vdev %p dev %p "
			"virtio_pci__reset/reset?? + ops %p (notify_satus)\n",
			pop_get_nid(), popcorn_gettid(), __func__, vdev, dev,
			vdev->ops->notify_status);
#endif

	vdev->status &= ~VIRTIO_CONFIG_S_MASK;
	vdev->status |= status;

	/* Add a few hints to help devices */
	if ((status & VIRTIO_CONFIG_S_DRIVER_OK) &&
	    !(vdev->status & VIRTIO__STATUS_START)) {
		vdev->status |= VIRTIO__STATUS_START;
		ext_status |= VIRTIO__STATUS_START;

	} else if (!status && (vdev->status & VIRTIO__STATUS_START)) {
		vdev->status &= ~VIRTIO__STATUS_START;
		ext_status |= VIRTIO__STATUS_STOP;

		/*
		 * Reset virtqueues and stop all traffic now, so that the device
		 * can safely reset the backend in notify_status().
		 */
		if (ext_status & VIRTIO__STATUS_STOP)
			vdev->ops->reset(kvm, vdev);
	}

	if (vdev->ops->notify_status)
		vdev->ops->notify_status(kvm, dev, ext_status);
}

int virtio_init(struct kvm *kvm, void *dev, struct virtio_device *vdev,
		struct virtio_ops *ops, enum virtio_trans trans,
		int device_id, int subsys_id, int class)
{
	void *virtio;

#ifdef CONFIG_POPCORN_HYPE
	/* Current implementation assumes */
    POP_DPRINTF(pop_get_nid(),"\t[%d/%d] %s: %s(): vdev %p trans %d\n",
			pop_get_nid(), popcorn_gettid(), __FILE__, __func__, vdev, trans);
    POP_PRINTF("\t[%d/%d] %s: %s(): vdev %p trans %d\n",
			pop_get_nid(), popcorn_gettid(), __FILE__, __func__, vdev, trans);
	//if (!pop_get_nid())
		//dump_stack();
#endif

	switch (trans) {
	case VIRTIO_PCI:
#ifdef CONFIG_POPCORN_HYPE
		if (!pop_get_nid()) {
			virtio = calloc(sizeof(struct virtio_pci), 1);
			if (!virtio)
				return -ENOMEM;
			vdev->virtio			= virtio;
			vdev->ops			= ops;
			vdev->ops->signal_vq		= virtio_pci__signal_vq;
			vdev->ops->signal_config	= virtio_pci__signal_config;
			vdev->ops->init			= virtio_pci__init;
			vdev->ops->exit			= virtio_pci__exit;
			vdev->ops->reset		= virtio_pci__reset;
		}
		vdev->ops->init(kvm, dev, vdev, device_id, subsys_id, class);
#else
		virtio = calloc(sizeof(struct virtio_pci), 1);
		if (!virtio)
			return -ENOMEM;
		vdev->virtio			= virtio;
		vdev->ops			= ops;
		vdev->ops->signal_vq		= virtio_pci__signal_vq;
		vdev->ops->signal_config	= virtio_pci__signal_config;
		vdev->ops->init			= virtio_pci__init;
		vdev->ops->exit			= virtio_pci__exit;
		vdev->ops->reset		= virtio_pci__reset;
		vdev->ops->init(kvm, dev, vdev, device_id, subsys_id, class);
#endif
#ifdef CONFIG_POPCORN_HYPE
		POP_PRINTF("\t\t[%d] regstered virtio for pci! .init %p\n",
								pop_get_nid(), vdev->ops->init);
#endif
		break;
	case VIRTIO_MMIO:
#ifdef CONFIG_POPCORN_HYPE
		if (!pop_get_nid()) {
			virtio = calloc(sizeof(struct virtio_mmio), 1);
			if (!virtio)
				return -ENOMEM;
			vdev->virtio			= virtio;
			vdev->ops			= ops;
			vdev->ops->signal_vq		= virtio_mmio_signal_vq;
			vdev->ops->signal_config	= virtio_mmio_signal_config;
			vdev->ops->init			= virtio_mmio_init;
			vdev->ops->exit			= virtio_mmio_exit;
			vdev->ops->reset		= virtio_mmio_reset;
		}
		vdev->ops->init(kvm, dev, vdev, device_id, subsys_id, class);
#else
		virtio = calloc(sizeof(struct virtio_mmio), 1);
		if (!virtio)
			return -ENOMEM;
		vdev->virtio			= virtio;
		vdev->ops			= ops;
		vdev->ops->signal_vq		= virtio_mmio_signal_vq;
		vdev->ops->signal_config	= virtio_mmio_signal_config;
		vdev->ops->init			= virtio_mmio_init;
		vdev->ops->exit			= virtio_mmio_exit;
		vdev->ops->reset		= virtio_mmio_reset;
		vdev->ops->init(kvm, dev, vdev, device_id, subsys_id, class);
#endif
#ifdef CONFIG_POPCORN_HYPE
		POP_PRINTF("\t\t[%d] regstered virtio for mmio! .init %p\n",
									pop_get_nid(), vdev->ops->init);
#endif
		break;
	default:
		return -1;
	};

	return 0;
}

int virtio_compat_add_message(const char *device, const char *config)
{
	int len = 1024;
	int compat_id;
	char *title;
	char *desc;

	title = malloc(len);
	if (!title)
		return -ENOMEM;

	desc = malloc(len);
	if (!desc) {
		free(title);
		return -ENOMEM;
	}

	snprintf(title, len, "%s device was not detected.", device);
	snprintf(desc,  len, "While you have requested a %s device, "
			     "the guest kernel did not initialize it.\n"
			     "\tPlease make sure that the guest kernel was "
			     "compiled with %s=y enabled in .config.",
			     device, config);

	compat_id = compat__add_message(title, desc);

	free(desc);
	free(title);

	return compat_id;
}
