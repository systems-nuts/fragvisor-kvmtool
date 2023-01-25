#include "kvm/virtio-pci.h"

#include "kvm/ioport.h"
#include "kvm/kvm.h"
#include "kvm/kvm-cpu.h"
#include "kvm/virtio-pci-dev.h"
#include "kvm/irq.h"
#include "kvm/virtio.h"
#include "kvm/ioeventfd.h"

#include <sys/ioctl.h>
#include <linux/virtio_pci.h>
#include <linux/byteorder.h>
#include <string.h>

#include <popcorn/utils.h>

static void virtio_pci__ioevent_callback(struct kvm *kvm, void *param)
{
	struct virtio_pci_ioevent_param *ioeventfd = param;
	struct virtio_pci *vpci = ioeventfd->vdev->virtio;

#ifdef CONFIG_POPCORN_HYPE
	POP_PRINTF("\t[net %d/%d/%d] %s(): vpci %p ioeventfd %p\n",
			pop_get_nid(), getpid(), popcorn_gettid(), __func__, vpci, ioeventfd);
#endif
	ioeventfd->vdev->ops->notify_vq(kvm, vpci->dev, ioeventfd->vq);
}

static int virtio_pci__init_ioeventfd(struct kvm *kvm, struct virtio_device *vdev, u32 vq)
{
	struct ioevent ioevent;
	struct virtio_pci *vpci = vdev->virtio;
	int r, flags = 0;
	int fd;

	vpci->ioeventfds[vq] = (struct virtio_pci_ioevent_param) {
		.vdev		= vdev,
		.vq		= vq,
	};

	ioevent = (struct ioevent) {
		.fn		= virtio_pci__ioevent_callback,
		.fn_ptr		= &vpci->ioeventfds[vq],
		.datamatch	= vq,
		.fn_kvm		= kvm,
	};

	/*
	 * Vhost will poll the eventfd in host kernel side, otherwise we
	 * need to poll in userspace.
	 */
	if (!vdev->use_vhost ||
		((u32)vdev->ops->get_vq_count(kvm, vpci->dev) == (vq + 1)) )
		flags |= IOEVENTFD_FLAG_USER_POLL;

	/**********/
	/* ioport */
	/**********/
	ioevent.io_addr	= vpci->port_addr + VIRTIO_PCI_QUEUE_NOTIFY;
	ioevent.io_len	= sizeof(u16);
	ioevent.fd	= fd = eventfd(0, 0);

#ifdef CONFIG_POPCORN_HYPE
	POP_PRINTF("\t[net %d/%d/%d] %s %s(): vhost \"%s\" "
			"ioevent.fd fd %d (15/SYSC_eventfd2) "
			"((u32)vdev->ops->get_vq_count(kvm, vpci->dev)==(vq+1)) \"%s\"\n",
			pop_get_nid(), getpid(), popcorn_gettid(), __FILE__, __func__,
			vdev->use_vhost ? "ON" : "OFF", ioevent.fd,
			((u32)vdev->ops->get_vq_count(kvm, vpci->dev) == (vq + 1)) ? "YES" : "NO");
#endif

	r = ioeventfd__add_event(&ioevent, flags | IOEVENTFD_FLAG_PIO);
	if (r)
		return r;


	/********/
	/* mmio */
	/********/
	ioevent.io_addr	= vpci->mmio_addr + VIRTIO_PCI_QUEUE_NOTIFY;
	ioevent.io_len	= sizeof(u16);
	ioevent.fd	= eventfd(0, 0); ///////////////

#ifdef CONFIG_POPCORN_HYPE
    POP_PRINTF("\t[net %d/%d/%d] %s(): ioevent.fd fd %d (16/SYSC_eventfd2)\n",
			pop_get_nid(), getpid(), popcorn_gettid(), __func__, ioevent.fd);
#endif

	r = ioeventfd__add_event(&ioevent, flags);
	if (r)
		goto free_ioport_evt;

#ifdef CONFIG_POPCORN_HYPE
	if (vdev->ops->notify_vq_eventfd) {
		POP_PRINTF("\t[net %d/%d/%d] %s(): "
				"vdev->ops->notify_vq_eventfd %p -> init_vq\n",
				pop_get_nid(), getpid(), popcorn_gettid(),
				__func__, vdev->ops->notify_vq_eventfd);
	}
#endif

	if (vdev->ops->notify_vq_eventfd)
		vdev->ops->notify_vq_eventfd(kvm, vpci->dev, vq, fd); // -> init_vq
	return 0;

free_ioport_evt:
	ioeventfd__del_event(vpci->port_addr + VIRTIO_PCI_QUEUE_NOTIFY, vq);
	return r;
}

static void virtio_pci_exit_vq(struct kvm *kvm, struct virtio_device *vdev,
			       int vq)
{
	struct virtio_pci *vpci = vdev->virtio;

	ioeventfd__del_event(vpci->mmio_addr + VIRTIO_PCI_QUEUE_NOTIFY, vq);
	ioeventfd__del_event(vpci->port_addr + VIRTIO_PCI_QUEUE_NOTIFY, vq);
	virtio_exit_vq(kvm, vdev, vpci->dev, vq);
}

static inline bool virtio_pci__msix_enabled(struct virtio_pci *vpci)
{
	return vpci->pci_hdr.msix.ctrl & cpu_to_le16(PCI_MSIX_FLAGS_ENABLE);
}

static bool virtio_pci__specific_io_in(struct kvm *kvm, struct virtio_device *vdev, u16 port,
					void *data, int size, int offset)
{
	u32 config_offset;
	struct virtio_pci *vpci = vdev->virtio;
	int type = virtio__get_dev_specific_field(offset - 20,
							virtio_pci__msix_enabled(vpci),
							&config_offset);
	if (type == VIRTIO_PCI_O_MSIX) {
		switch (offset) {
		case VIRTIO_MSI_CONFIG_VECTOR:
			ioport__write16(data, vpci->config_vector);
			break;
		case VIRTIO_MSI_QUEUE_VECTOR:
			ioport__write16(data, vpci->vq_vector[vpci->queue_selector]);
			break;
		};

		return true;
	} else if (type == VIRTIO_PCI_O_CONFIG) {
		u8 cfg;

		cfg = vdev->ops->get_config(kvm, vpci->dev)[config_offset];
		ioport__write8(data, cfg);
		return true;
	}

	return false;
}

static bool virtio_pci__io_in(struct ioport *ioport, struct kvm_cpu *vcpu, u16 port, void *data, int size)
{
	unsigned long offset;
	bool ret = true;
	struct virtio_device *vdev;
	struct virtio_pci *vpci;
	struct virt_queue *vq;
	struct kvm *kvm;
	u32 val;

	kvm = vcpu->kvm;
	vdev = ioport->priv;
	vpci = vdev->virtio;
	offset = port - vpci->port_addr;

	switch (offset) {
	case VIRTIO_PCI_HOST_FEATURES:
		val = vdev->ops->get_host_features(kvm, vpci->dev);
		ioport__write32(data, val);
		break;
	case VIRTIO_PCI_QUEUE_PFN:
		vq = vdev->ops->get_vq(kvm, vpci->dev, vpci->queue_selector);
		ioport__write32(data, vq->pfn);
		break;
	case VIRTIO_PCI_QUEUE_NUM:
		val = vdev->ops->get_size_vq(kvm, vpci->dev, vpci->queue_selector);
		ioport__write16(data, val);
		break;
	case VIRTIO_PCI_STATUS:
		ioport__write8(data, vpci->status);
		break;
	case VIRTIO_PCI_ISR:
		ioport__write8(data, vpci->isr);
		kvm__irq_line(kvm, vpci->legacy_irq_line, VIRTIO_IRQ_LOW);
		vpci->isr = VIRTIO_IRQ_LOW;
		break;
	default:
		ret = virtio_pci__specific_io_in(kvm, vdev, port, data, size, offset);
		break;
	};

	return ret;
}

static void update_msix_map(struct virtio_pci *vpci,
			    struct msix_table *msix_entry, u32 vecnum)
{
	u32 gsi, i;

	/* Find the GSI number used for that vector */
	if (vecnum == vpci->config_vector) {
		gsi = vpci->config_gsi;
	} else {
		for (i = 0; i < VIRTIO_PCI_MAX_VQ; i++)
			if (vpci->vq_vector[i] == vecnum)
				break;
		if (i == VIRTIO_PCI_MAX_VQ)
			return;
		gsi = vpci->gsis[i];
	}

	if (gsi == 0)
		return;

	msix_entry = &msix_entry[vecnum];
	irq__update_msix_route(vpci->kvm, gsi, &msix_entry->msg);
}

static bool virtio_pci__specific_io_out(struct kvm *kvm, struct virtio_device *vdev, u16 port,
					void *data, int size, int offset)
{
	struct virtio_pci *vpci = vdev->virtio;
	u32 config_offset, vec;
	int gsi;
	int type = virtio__get_dev_specific_field(offset - 20, virtio_pci__msix_enabled(vpci),
							&config_offset);

#ifdef CONFIG_POPCORN_HYPE
	POP_PRINTF("\t[net %d/%d] %s: %s(): type %d offset %d\n",
			pop_get_nid(), popcorn_gettid(), __FILE__, __func__, type, offset);
#endif
	if (type == VIRTIO_PCI_O_MSIX) {
		switch (offset) {
		case VIRTIO_MSI_CONFIG_VECTOR:
			vec = vpci->config_vector = ioport__read16(data);
			if (vec == VIRTIO_MSI_NO_VECTOR)
				break;

#ifdef CONFIG_POPCORN_HYPE
			POP_PRINTF("\t[net %d/%d] %s: %s(): VIRTIO_MSI_CONFIG_VECTOR 20\n",
					pop_get_nid(), popcorn_gettid(), __FILE__, __func__);
#endif
			/* popbroadcast in irq__update_msix_routes()
				since irq__add_msix_route has heap data */
			gsi = irq__add_msix_route(kvm,
						  &vpci->msix_table[vec].msg,
						  vpci->dev_hdr.dev_num << 3);
			/*
			 * We don't need IRQ routing if we can use
			 * MSI injection via the KVM_SIGNAL_MSI ioctl.
			 */
			if (gsi == -ENXIO &&
			    vpci->features & VIRTIO_PCI_F_SIGNAL_MSI)
				break;

			if (gsi < 0) {
				die("failed to configure MSIs");
				break;
			}

			vpci->config_gsi = gsi;
			break;
		case VIRTIO_MSI_QUEUE_VECTOR:
			vec = ioport__read16(data);
			vpci->vq_vector[vpci->queue_selector] = vec;
			if (vec == VIRTIO_MSI_NO_VECTOR)
				break;

#ifdef CONFIG_POPCORN_HYPE
			POP_PRINTF("\t[net %d/%d/%d] %s: %s(): VIRTIO_MSI_QUEUE_VECTOR 22\n",
				pop_get_nid(), getpid(), popcorn_gettid(), __FILE__, __func__);
#endif
			/* popbroadcast in irq__update_msix_routes()
				since irq__add_msix_route has heap data */
			gsi = irq__add_msix_route(kvm,
						  &vpci->msix_table[vec].msg,
						  vpci->dev_hdr.dev_num << 3);
			/*
			 * We don't need IRQ routing if we can use
			 * MSI injection via the KVM_SIGNAL_MSI ioctl.
			 */
			if (gsi == -ENXIO &&
			    vpci->features & VIRTIO_PCI_F_SIGNAL_MSI)
				break;

			if (gsi < 0) {
				die("failed to configure MSIs");
				break;
			}

			vpci->gsis[vpci->queue_selector] = gsi;
			if (vdev->ops->notify_vq_gsi) {
				vdev->ops->notify_vq_gsi(kvm, vpci->dev,
							 vpci->queue_selector,
							 gsi);
#ifdef CONFIG_POPCORN_HYPE
				int i, nid = pop_get_nid();
				BUG_ON(nid && "This may happen if guestOS not recompiled");
				POP_PRINTF("\n\n======= popbroadcast - "
							"notify_vq_gsi() ===========\n");
				POP_PRINTF("\t\t22 = KVM_SET_GSI_ROUTING "
					"irq__update_msix_routes(): ioctl(KVM_SET_GSI_ROUTING)  + "
					"notify_vq_gsi():\n");
				for (i = 1; i < nodes; i++) {
					if (i == nid) { continue; }
					POP_PRINTF("init 22 [*] net: %s: %s(): "
							"replaying notify_vq_gsi() at [%d]\n",
							__FILE__, __func__, i);
					migrate(i, NULL, NULL); /**********************************/

					POP_DPRINTF(pop_get_nid(), "\t\t22 [from *] => <%d> [%d] %s(): "
								"replaying notify_vq_gsi()\n\n",
								pop_get_nid(), popcorn_gettid(), __func__);
					POP_PRINTF("\t\tinit 22 [from *] => <%d> [%d] %s(): "
								"replaying notify_vq_gsi()\n\n",
								pop_get_nid(), popcorn_gettid(), __func__);
#if HOST_NET
					vdev->ops->notify_vq_gsi(kvm, vpci->dev,
								 vpci->queue_selector, gsi);
#endif
					VHOSTPK("msix registered: q=%d, vec=%d, gsi=%d, hi=%x, lo=%x\n", vpci->queue_selector, vec, gsi,
								vpci->msix_table[vec].msg.address_lo, vpci->msix_table[vec].msg.address_hi);
					migrate(0, NULL, NULL); /**********************************/
					POP_PRINTF("[*] net: notify_vq_gsi() done at remote %d\n", i);
				}
				POP_PRINTF("======= popbroadcast done 22 ===========\n\n\n");
#endif				
			}
			break;
		};

		return true;
	} else if (type == VIRTIO_PCI_O_CONFIG) {
		vdev->ops->get_config(kvm, vpci->dev)[config_offset] = *(u8 *)data;

		return true;
	}

	return false;
}

static bool virtio_pci__io_out(struct ioport *ioport, struct kvm_cpu *vcpu, u16 port, void *data, int size)
{
	unsigned long offset;
	bool ret = true;
	struct virtio_device *vdev;
	struct virtio_pci *vpci;
	struct kvm *kvm;
	u32 val;

	kvm = vcpu->kvm;
	vdev = ioport->priv;
	vpci = vdev->virtio;
	offset = port - vpci->port_addr;

#ifdef CONFIG_POPCORN_HYPE
	/* important - vdev = ioport->priv; */
	POP_PRINTF("\t[net %d/%d/%d] %s: %s(): vpci->dev %p vdev %p offset %lu\n",
			pop_get_nid(), getpid(), popcorn_gettid(),
			__FILE__, __func__, vpci->dev, vdev, offset);
#endif

	switch (offset) {
	case VIRTIO_PCI_GUEST_FEATURES:
		val = ioport__read32(data);
		virtio_set_guest_features(kvm, vdev, vpci->dev, val);
		POP_PRINTF("\t\t[net %d/%d/%d] %s(): VIRTIO_PCI_GUEST_FEATURES 4 "
				"vpci->dev %p val read pfn %u vdev %p\n",
				pop_get_nid(), getpid(), popcorn_gettid(),
				__func__, vpci->dev, val, vdev);
		break;
	case VIRTIO_PCI_QUEUE_PFN:
		val = ioport__read32(data);
		POP_PRINTF("\t\t[net %d/%d/%d] %s(): VIRTIO_PCI_QUEUE_PFN 8 "
				"vpci->dev %p val read pfn %u "
				"vdev %p vdev->use_vhost %c ->vdev->ops->init_vq() %p\n",
				pop_get_nid(), getpid(), popcorn_gettid(), __func__, vpci->dev,
				val, vdev, vdev->use_vhost ? 'O' : 'X', vdev->ops->init_vq);
		if (val) {
            virtio_pci__init_ioeventfd(kvm, vdev,
                           vpci->queue_selector);
            vdev->ops->init_vq(kvm, vpci->dev, vpci->queue_selector,
                       1 << VIRTIO_PCI_QUEUE_ADDR_SHIFT,
                       VIRTIO_PCI_VRING_ALIGN, val);

#ifdef CONFIG_POPCORN_HYPE
			POP_PRINTF("\n\n\t\t[net %d/%d/%d] %s(): "
						"WORKING ON brodcast kernel states\n\n\n",
						pop_get_nid(), getpid(), popcorn_gettid(), __func__);

			/* stop the world solution which is fine since this is init at runtime */
			POP_PRINTF("\t======= popbroadcast "
										"- init_vq() ===========\n");
			int i, nid = pop_get_nid();
			BUG_ON(nid && "This may happen if guestOS not recompiled");
			for (i = 1; i < nodes; i++) {
				if (i == nid) { continue; }
				POP_PRINTF("\t\t[*] net: %s: %s(): replaying init_vq() "
							"at [%d]\n", __FILE__, __func__, i);
				migrate(i, NULL, NULL); /**********************************/

				POP_DPRINTF(pop_get_nid(), "\t\t8 [from *] => <%d> [%d] %s(): "
							"going to call init_vq()\n\n",
							pop_get_nid(), popcorn_gettid(), __func__);
				POP_PRINTF("\t\t(runtime net vq init)[from *] => <%d> [%d] %s(): "
							"going to call init_vq()\n\n",
							pop_get_nid(), popcorn_gettid(), __func__);
#if HOST_NET
				virtio_pci__init_ioeventfd(kvm, vdev,
							   vpci->queue_selector);
				vdev->ops->init_vq(kvm, vpci->dev, vpci->queue_selector,
						   1 << VIRTIO_PCI_QUEUE_ADDR_SHIFT,
						   VIRTIO_PCI_VRING_ALIGN, val);
#endif

				migrate(0, NULL, NULL); /**********************************/
				POP_PRINTF("\t\t[*] net: init_vq() done at remote %d\n", i);
			}
			POP_PRINTF("\t===== popbroadcast done =======\n");
#endif
		} else {
			virtio_pci_exit_vq(kvm, vdev, vpci->queue_selector);
		}
		break;
	case VIRTIO_PCI_QUEUE_SEL: /* 14 */
		vpci->queue_selector = ioport__read16(data);
		break;
	case VIRTIO_PCI_QUEUE_NOTIFY: /* 16 */
		val = ioport__read16(data);
		vdev->ops->notify_vq(kvm, vpci->dev, val);
		break;
	case VIRTIO_PCI_STATUS: /* 18 */
		vpci->status = ioport__read8(data);
		if (!vpci->status) /* Sample endianness on reset */
			vdev->endian = kvm_cpu__get_endianness(vcpu);
		POP_PRINTF("\t[net %d/%d/%d] %s(): VIRTIO_PCI_STATUS 18 "
				"vpci->dev %p val read %u\n",
				pop_get_nid(), getpid(), popcorn_gettid(), __func__,
				vpci->dev, vpci->status);
		virtio_notify_status(kvm, vdev, vpci->dev, vpci->status);
		break;
	default:
		POP_PRINTF("\t[net %d/%d/%d] %s: %s(): default\n",
				pop_get_nid(), getpid(), popcorn_gettid(), __FILE__, __func__);
		ret = virtio_pci__specific_io_out(kvm, vdev, port, data, size, offset);
		break;
	};

	return ret;
}

static struct ioport_operations virtio_pci__io_ops = {
	.io_in	= virtio_pci__io_in,
	.io_out	= virtio_pci__io_out,
};

static void virtio_pci__msix_mmio_callback(struct kvm_cpu *vcpu,
					   u64 addr, u8 *data, u32 len,
					   u8 is_write, void *ptr)
{
	struct virtio_pci *vpci = ptr;
	struct msix_table *table;
	int vecnum;
	size_t offset;

#ifndef CONFIG_POPCORN_HYPE
	int nid = pop_get_nid();
	POP_PRINTF("\t[net %d/%d] %s(): NET_MSI first ping "
			"has 7 mmio_exit6 = this call\n",
			pop_get_nid(), popcorn_gettid(), __func__);
	
	BUG_ON(pop_get_nid()); /* THIS MAY BE REMOVED SOON!!!!!*/
	/* the following is solved by NET_MSI*/
	/* net -> mmio_exit -> this callback func() (happens at remote...) */
	/* HACK - migrate to origin and finish it and migrate back */
	if (nid) {
		POP_PRINTF("\t\t\t<%d> migrate to %d\n", nid, 0);
		migrate(0, NULL, NULL);
	}
#endif

	if (addr > vpci->msix_io_block + PCI_IO_SIZE) {
		if (is_write)
			return;
		table  = (struct msix_table *)&vpci->msix_pba;
		offset = addr - (vpci->msix_io_block + PCI_IO_SIZE);
	} else {
		table  = vpci->msix_table;
		offset = addr - vpci->msix_io_block;
	}
	vecnum = offset / sizeof(struct msix_table);
	offset = offset % sizeof(struct msix_table);

	if (!is_write) {
		memcpy(data, (void *)&table[vecnum] + offset, len);
		return;
	}

	memcpy((void *)&table[vecnum] + offset, data, len);

	/* Did we just update the address or payload? */
	if (offset < offsetof(struct msix_table, ctrl))
		update_msix_map(vpci, table, vecnum);

#ifndef CONFIG_POPCORN_HYPE
	if (nid) {
		POP_PRINTF("\t\t\t <*> migrate back to %d\n", nid);
		migrate(nid, NULL, NULL);
	}
#endif
}

static void virtio_pci__signal_msi(struct kvm *kvm, struct virtio_pci *vpci,
				   int vec)
{
	struct kvm_msi msi = {
		.address_lo = vpci->msix_table[vec].msg.address_lo,
		.address_hi = vpci->msix_table[vec].msg.address_hi,
		.data = vpci->msix_table[vec].msg.data,
	};

	if (kvm->msix_needs_devid) {
		msi.flags = KVM_MSI_VALID_DEVID;
		msi.devid = vpci->dev_hdr.dev_num << 3;
	}

	irq__signal_msi(kvm, &msi);
}

int virtio_pci__signal_vq(struct kvm *kvm, struct virtio_device *vdev, u32 vq)
{
	struct virtio_pci *vpci = vdev->virtio;
	int tbl = vpci->vq_vector[vq];

	if (virtio_pci__msix_enabled(vpci) && tbl != VIRTIO_MSI_NO_VECTOR) {
		if (vpci->pci_hdr.msix.ctrl & cpu_to_le16(PCI_MSIX_FLAGS_MASKALL) ||
		    vpci->msix_table[tbl].ctrl & cpu_to_le16(PCI_MSIX_ENTRY_CTRL_MASKBIT)) {

			vpci->msix_pba |= 1 << tbl;
			return 0;
		}

		if (vpci->features & VIRTIO_PCI_F_SIGNAL_MSI)
			virtio_pci__signal_msi(kvm, vpci, vpci->vq_vector[vq]);
		else
			kvm__irq_trigger(kvm, vpci->gsis[vq]);
	} else {
		vpci->isr = VIRTIO_IRQ_HIGH;
		kvm__irq_trigger(kvm, vpci->legacy_irq_line);
	}
	return 0;
}

int virtio_pci__signal_config(struct kvm *kvm, struct virtio_device *vdev)
{
	struct virtio_pci *vpci = vdev->virtio;
	int tbl = vpci->config_vector;

	if (virtio_pci__msix_enabled(vpci) && tbl != VIRTIO_MSI_NO_VECTOR) {
		if (vpci->pci_hdr.msix.ctrl & cpu_to_le16(PCI_MSIX_FLAGS_MASKALL) ||
		    vpci->msix_table[tbl].ctrl & cpu_to_le16(PCI_MSIX_ENTRY_CTRL_MASKBIT)) {

			vpci->msix_pba |= 1 << tbl;
			return 0;
		}

		if (vpci->features & VIRTIO_PCI_F_SIGNAL_MSI)
			virtio_pci__signal_msi(kvm, vpci, tbl);
		else
			kvm__irq_trigger(kvm, vpci->config_gsi);
	} else {
		vpci->isr = VIRTIO_PCI_ISR_CONFIG;
		kvm__irq_trigger(kvm, vpci->legacy_irq_line);
	}

	return 0;
}

static void virtio_pci__io_mmio_callback(struct kvm_cpu *vcpu,
					 u64 addr, u8 *data, u32 len,
					 u8 is_write, void *ptr)
{
	struct virtio_pci *vpci = ptr;
	int direction = is_write ? KVM_EXIT_IO_OUT : KVM_EXIT_IO_IN;
	u16 port = vpci->port_addr + (addr & (IOPORT_SIZE - 1));

	kvm__emulate_io(vcpu, port, data, direction, len, 1);
}

int virtio_pci__init(struct kvm *kvm, void *dev, struct virtio_device *vdev,
		     int device_id, int subsys_id, int class)
{
	struct virtio_pci *vpci = vdev->virtio;
	int r;

#ifdef CONFIG_POPCORN_HYPE
    POP_PRINTF("\t\t[%d] %s %s(): JACK vpci %p from virtio_init(1.5init)\n"
			"\t\tvirtio_pci__io_mmio_callback %p (pci mmio_fn but not used) \\\n"
			"\t\tvirtio_pci__msix_mmio_callback %p = **mmio_fn**\\\n"
			"\t\tJACK virtio_pci__io_ops -> "
			"register function for registering (virtio_pci__io_out) "
			"pci_out case init fd 15 16 17\\\n"
			"\t\tioport__register() { device__register() (can skip) } (pophype TODO)\\\n"
			"\t\tpci_get_io_space_block*2 (DANGEROUS!!!!!)\\\n"
			"\t\tkvm__register_mmio() *2 (can skip)\\\n"
			"\t\tdevice__register() { usr rb_tree insert }\n"
			"\n\n",
			pop_get_nid(), __FILE__, __func__, vpci,
			virtio_pci__io_mmio_callback,
			virtio_pci__msix_mmio_callback);
	dump_stack();
#endif
	vpci->kvm = kvm;
	vpci->dev = dev;

	r = ioport__register(kvm, IOPORT_EMPTY, &virtio_pci__io_ops, IOPORT_SIZE, vdev);
	if (r < 0)
		return r;
	vpci->port_addr = (u16)r;

#ifdef CONFIG_POPCORN_HYPE
	if (pop_get_nid()) {
		POP_PRINTF("\t\tExit after ioport__register()\n");
		return 0;
	}

	if (!pop_get_nid())
		vpci->mmio_addr = pci_get_io_space_block(IOPORT_SIZE); /* calc new addr */
	else
		return 0;
#else
	/* register mmio in usr rb_tree (no need to replicate) */
	vpci->mmio_addr = pci_get_io_space_block(IOPORT_SIZE); /* calc new addr */
#endif
	POP_DPRINTF(pop_get_nid(), "\t\t <%d> [%d] %s(): vpci->mmio_addr %lx "
											"(who's first 1)\n",
				pop_get_nid(), popcorn_gettid(), __func__, vpci->mmio_addr);
	r = kvm__register_mmio(kvm, vpci->mmio_addr, IOPORT_SIZE, false,
			       virtio_pci__io_mmio_callback, vpci);
	if (r < 0)
		goto free_ioport;

#ifdef CONFIG_POPCORN_HYPE /*HACK*/
	if (!pop_get_nid())
		vpci->msix_io_block = pci_get_io_space_block(PCI_IO_SIZE * 2);
	else
		BUG_ON("MUST HAVE EXITED");
#else
	/* register mmio in usr rb_tree (no need to replicate) */
	vpci->msix_io_block = pci_get_io_space_block(PCI_IO_SIZE * 2);
#endif
	POP_DPRINTF(pop_get_nid(), "\t\t <%d> [%d] %s(): who's first 2\n",
							pop_get_nid(), popcorn_gettid(), __func__);
	r = kvm__register_mmio(kvm, vpci->msix_io_block, PCI_IO_SIZE * 2, false,
			       virtio_pci__msix_mmio_callback, vpci);
	if (r < 0)
		goto free_mmio;

#ifdef CONFIG_POPCORN_HYPE
    POP_PRINTF("\n\t\t=============\n"
			"\t\t[%d] %s %s(): (heap sanity check calssic problem) "
			"vpci->mmio_addr 0x%x ->msix_io_block 0x%x\n"
			"\t\t=============\n",
			pop_get_nid(), __FILE__, __func__,
			vpci->mmio_addr, vpci->msix_io_block);
#endif
	vpci->pci_hdr = (struct pci_device_header) {
		.vendor_id		= cpu_to_le16(PCI_VENDOR_ID_REDHAT_QUMRANET),
		.device_id		= cpu_to_le16(device_id),
		.command		= PCI_COMMAND_IO | PCI_COMMAND_MEMORY,
		.header_type		= PCI_HEADER_TYPE_NORMAL,
		.revision_id		= 0,
		.class[0]		= class & 0xff,
		.class[1]		= (class >> 8) & 0xff,
		.class[2]		= (class >> 16) & 0xff,
		.subsys_vendor_id	= cpu_to_le16(PCI_SUBSYSTEM_VENDOR_ID_REDHAT_QUMRANET),
		.subsys_id		= cpu_to_le16(subsys_id),
		.bar[0]			= cpu_to_le32(vpci->port_addr
							| PCI_BASE_ADDRESS_SPACE_IO),
		.bar[1]			= cpu_to_le32(vpci->mmio_addr
							| PCI_BASE_ADDRESS_SPACE_MEMORY),
		.bar[2]			= cpu_to_le32(vpci->msix_io_block
							| PCI_BASE_ADDRESS_SPACE_MEMORY),
		.status			= cpu_to_le16(PCI_STATUS_CAP_LIST),
		.capabilities		= (void *)&vpci->pci_hdr.msix - (void *)&vpci->pci_hdr,
		.bar_size[0]		= cpu_to_le32(IOPORT_SIZE),
		.bar_size[1]		= cpu_to_le32(IOPORT_SIZE),
		.bar_size[2]		= cpu_to_le32(PCI_IO_SIZE*2),
	};

	vpci->dev_hdr = (struct device_header) {
		.bus_type		= DEVICE_BUS_PCI,
		.data			= &vpci->pci_hdr,
	};

	vpci->pci_hdr.msix.cap = PCI_CAP_ID_MSIX;
	vpci->pci_hdr.msix.next = 0;
	/*
	 * We at most have VIRTIO_PCI_MAX_VQ entries for virt queue,
	 * VIRTIO_PCI_MAX_CONFIG entries for config.
	 *
	 * To quote the PCI spec:
	 *
	 * System software reads this field to determine the
	 * MSI-X Table Size N, which is encoded as N-1.
	 * For example, a returned value of "00000000011"
	 * indicates a table size of 4.
	 */
	vpci->pci_hdr.msix.ctrl = cpu_to_le16(VIRTIO_PCI_MAX_VQ + VIRTIO_PCI_MAX_CONFIG - 1);

	/* Both table and PBA are mapped to the same BAR (2) */
	vpci->pci_hdr.msix.table_offset = cpu_to_le32(2);
	vpci->pci_hdr.msix.pba_offset = cpu_to_le32(2 | PCI_IO_SIZE);
	vpci->config_vector = 0;

	if (irq__can_signal_msi(kvm))
		vpci->features |= VIRTIO_PCI_F_SIGNAL_MSI;

	r = device__register(&vpci->dev_hdr); /* pophype: can skip */
	if (r < 0)
		goto free_msix_mmio;

	/* save the IRQ that device__register() has allocated */
	vpci->legacy_irq_line = vpci->pci_hdr.irq_line;

	return 0;

free_msix_mmio:
	kvm__deregister_mmio(kvm, vpci->msix_io_block);
free_mmio:
	kvm__deregister_mmio(kvm, vpci->mmio_addr);
free_ioport:
	ioport__unregister(kvm, vpci->port_addr);
	return r;
}

int virtio_pci__reset(struct kvm *kvm, struct virtio_device *vdev)
{
	int vq;
	struct virtio_pci *vpci = vdev->virtio;

#ifdef CONFIG_POPCORN_HYPE
	POP_PRINTF("\n\n\n"
			"\t\t[net %d/%d] %s(): vpci %p (NEED YOUR ATTENTION!!!!!!)\n\n\n",
			getpid(), popcorn_gettid(), __func__, vpci);
#endif
	for (vq = 0; vq < vdev->ops->get_vq_count(kvm, vpci->dev); vq++)
		virtio_pci_exit_vq(kvm, vdev, vq);

	return 0;
}

int virtio_pci__exit(struct kvm *kvm, struct virtio_device *vdev)
{
	struct virtio_pci *vpci = vdev->virtio;

	virtio_pci__reset(kvm, vdev);
	kvm__deregister_mmio(kvm, vpci->mmio_addr);
	kvm__deregister_mmio(kvm, vpci->msix_io_block);
	ioport__unregister(kvm, vpci->port_addr);

	return 0;
}
