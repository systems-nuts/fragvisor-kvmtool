#include "kvm/devices.h"
#include "kvm/pci.h"
#include "kvm/ioport.h"
#include "kvm/irq.h"
#include "kvm/util.h"
#include "kvm/kvm.h"

#include <linux/err.h>
#include <assert.h>

static u32 pci_config_address_bits;

/* This is within our PCI gap - in an unused area.
 * Note this is a PCI *bus address*, is used to assign BARs etc.!
 * (That's why it can still 32bit even with 64bit guests-- 64bit
 * PCI isn't currently supported.)
 */
static u32 io_space_blocks		= KVM_PCI_MMIO_AREA;

/*
 * BARs must be naturally aligned, so enforce this in the allocator.
 */
u32 pci_get_io_space_block(u32 size)
{
	u32 block = ALIGN(io_space_blocks, size);
	io_space_blocks = block + size;
	return block;
}

void *pci_find_cap(struct pci_device_header *hdr, u8 cap_type)
{
	u8 pos;
	struct pci_cap_hdr *cap;

	pci_for_each_cap(pos, cap, hdr) {
		if (cap->type == cap_type)
			return cap;
	}

	return NULL;
}

void pci__assign_irq(struct device_header *dev_hdr)
{
	struct pci_device_header *pci_hdr = dev_hdr->data;

	/*
	 * PCI supports only INTA#,B#,C#,D# per device.
	 *
	 * A#,B#,C#,D# are allowed for multifunctional devices so stick
	 * with A# for our single function devices.
	 */
	pci_hdr->irq_pin	= 1;
	pci_hdr->irq_line	= irq__alloc_line();

	if (!pci_hdr->irq_type)
		pci_hdr->irq_type = IRQ_TYPE_EDGE_RISING;
}

static void *pci_config_address_ptr(u16 port)
{
	unsigned long offset;
	void *base;

	offset	= port - PCI_CONFIG_ADDRESS;
	base	= &pci_config_address_bits;

	return base + offset;
}

static bool pci_config_address_out(struct ioport *ioport, struct kvm_cpu *vcpu, u16 port, void *data, int size)
{
	void *p = pci_config_address_ptr(port);

	memcpy(p, data, size);

	return true;
}

static bool pci_config_address_in(struct ioport *ioport, struct kvm_cpu *vcpu, u16 port, void *data, int size)
{
	void *p = pci_config_address_ptr(port);

	memcpy(data, p, size);

	return true;
}

static struct ioport_operations pci_config_address_ops = {
	.io_in	= pci_config_address_in,
	.io_out	= pci_config_address_out,
};

static bool pci_device_exists(u8 bus_number, u8 device_number, u8 function_number)
{
	union pci_config_address pci_config_address;

	pci_config_address.w = ioport__read32(&pci_config_address_bits);

	if (pci_config_address.bus_number != bus_number)
		return false;

	if (pci_config_address.function_number != function_number)
		return false;

	return !IS_ERR_OR_NULL(device__find_dev(DEVICE_BUS_PCI, device_number));
}

static bool pci_config_data_out(struct ioport *ioport, struct kvm_cpu *vcpu, u16 port, void *data, int size)
{
	union pci_config_address pci_config_address;

	pci_config_address.w = ioport__read32(&pci_config_address_bits);
	/*
	 * If someone accesses PCI configuration space offsets that are not
	 * aligned to 4 bytes, it uses ioports to signify that.
	 */
	pci_config_address.reg_offset = port - PCI_CONFIG_DATA;

	pci__config_wr(vcpu->kvm, pci_config_address, data, size);

	return true;
}

static bool pci_config_data_in(struct ioport *ioport, struct kvm_cpu *vcpu, u16 port, void *data, int size)
{
	union pci_config_address pci_config_address;

	pci_config_address.w = ioport__read32(&pci_config_address_bits);
	/*
	 * If someone accesses PCI configuration space offsets that are not
	 * aligned to 4 bytes, it uses ioports to signify that.
	 */
	pci_config_address.reg_offset = port - PCI_CONFIG_DATA;

	pci__config_rd(vcpu->kvm, pci_config_address, data, size);

	return true;
}

static struct ioport_operations pci_config_data_ops = {
	.io_in	= pci_config_data_in,
	.io_out	= pci_config_data_out,
};

void pci__config_wr(struct kvm *kvm, union pci_config_address addr, void *data, int size)
{
	void *base;
	u8 bar, offset;
	struct pci_device_header *pci_hdr;
	u8 dev_num = addr.device_number;

	if (!pci_device_exists(addr.bus_number, dev_num, 0))
		return;

	offset = addr.w & PCI_DEV_CFG_MASK;
	base = pci_hdr = device__find_dev(DEVICE_BUS_PCI, dev_num)->data;

	if (pci_hdr->cfg_ops.write)
		pci_hdr->cfg_ops.write(kvm, pci_hdr, offset, data, size);

	/*
	 * legacy hack: ignore writes to uninitialized regions (e.g. ROM BAR).
	 * Not very nice but has been working so far.
	 */
	if (*(u32 *)(base + offset) == 0)
		return;

	bar = (offset - PCI_BAR_OFFSET(0)) / sizeof(u32);

	/*
	 * If the kernel masks the BAR it would expect to find the size of the
	 * BAR there next time it reads from it. When the kernel got the size it
	 * would write the address back.
	 */
	if (bar < 6 && ioport__read32(data) == 0xFFFFFFFF) {
		u32 sz = pci_hdr->bar_size[bar];
		memcpy(base + offset, &sz, sizeof(sz));
	} else {
		memcpy(base + offset, data, size);
	}
}

void pci__config_rd(struct kvm *kvm, union pci_config_address addr, void *data, int size)
{
	u8 offset;
	struct pci_device_header *pci_hdr;
	u8 dev_num = addr.device_number;

	if (pci_device_exists(addr.bus_number, dev_num, 0)) {
		pci_hdr = device__find_dev(DEVICE_BUS_PCI, dev_num)->data;
		offset = addr.w & PCI_DEV_CFG_MASK;

		if (pci_hdr->cfg_ops.read)
			pci_hdr->cfg_ops.read(kvm, pci_hdr, offset, data, size);

		memcpy(data, (void *)pci_hdr + offset, size);
	} else {
		memset(data, 0xff, size);
	}
}

static void pci_config_mmio_access(struct kvm_cpu *vcpu, u64 addr, u8 *data,
				   u32 len, u8 is_write, void *kvm)
{
	union pci_config_address cfg_addr;

	addr			-= KVM_PCI_CFG_AREA;
	cfg_addr.w		= (u32)addr;
	cfg_addr.enable_bit	= 1;

	if (is_write)
		pci__config_wr(kvm, cfg_addr, data, len);
	else
		pci__config_rd(kvm, cfg_addr, data, len);
}

struct pci_device_header *pci__find_dev(u8 dev_num)
{
	struct device_header *hdr = device__find_dev(DEVICE_BUS_PCI, dev_num);

	if (IS_ERR_OR_NULL(hdr))
		return NULL;

	return hdr->data;
}

#include <popcorn/utils.h>
int pci__init(struct kvm *kvm)
{
	int r;

#ifdef CONFIG_POPCORN_HYPE
	if (pop_get_nid()) {
		POP_PRINTF("\t%s(): [<%d>/%d] SKIP.................\n",
				__func__, pop_get_nid(), popcorn_gettid());
		return 0;
	}

    POP_PRINTF("\t%s(): [<%d>/%d] JACK JACK JACK "
			"pci_config_mmio_access %p (frequent mmio->mmio_fn)\n\n",
			__func__, pop_get_nid(), popcorn_gettid(), pci_config_mmio_access);
#endif

	r = ioport__register(kvm, PCI_CONFIG_DATA + 0, &pci_config_data_ops, 4, NULL);
	if (r < 0)
		return r;

	r = ioport__register(kvm, PCI_CONFIG_ADDRESS + 0, &pci_config_address_ops, 4, NULL);
	if (r < 0)
		goto err_unregister_data;

	r = kvm__register_mmio(kvm, KVM_PCI_CFG_AREA, PCI_CFG_SIZE, false,
			       pci_config_mmio_access, kvm);
	if (r < 0)
		goto err_unregister_addr;

	return 0;

err_unregister_addr:
	ioport__unregister(kvm, PCI_CONFIG_ADDRESS);
err_unregister_data:
	ioport__unregister(kvm, PCI_CONFIG_DATA);
	return r;
}
dev_base_init(pci__init);

int pci__exit(struct kvm *kvm)
{
	ioport__unregister(kvm, PCI_CONFIG_DATA);
	ioport__unregister(kvm, PCI_CONFIG_ADDRESS);

	return 0;
}
dev_base_exit(pci__exit);
