#include "kvm/irq.h"
#include "kvm/kvm.h"
#include "kvm/util.h"

#include <linux/types.h>
#include <linux/rbtree.h>
#include <linux/list.h>
#include <linux/kvm.h>
#include <sys/ioctl.h>

#include <stddef.h>
#include <stdlib.h>

#define IRQCHIP_MASTER			0
#define IRQCHIP_SLAVE			1
#define IRQCHIP_IOAPIC			2

static int irq__add_routing(u32 gsi, u32 type, u32 irqchip, u32 pin)
{
	int r = irq__allocate_routing_entry();
	if (r)
		return r;

	irq_routing->entries[irq_routing->nr++] =
		(struct kvm_irq_routing_entry) {
			.gsi = gsi,
			.type = type,
			.u.irqchip.irqchip = irqchip,
			.u.irqchip.pin = pin,
		};

	return 0;
}

#include <popcorn/utils.h>
/***
 * irq_routing is on heap
 */
extern pthread_barrier_t barrier_irq_setup;
int irq__init(struct kvm *kvm)
{
	int i, r;

	POP_DBG_PRINTF(pop_get_nid(), "\t[<%d>/%d] %s(): per system\n",
						pop_get_nid(), popcorn_gettid(), __func__);

#ifdef CONFIG_POPCORN_HYPE
	if (pop_get_nid())
		goto skip; /* remote do it after origin */
	//return 0;
#endif

	/* Hook first 8 GSIs to master IRQCHIP */
	for (i = 0; i < 8; i++)
		if (i != 2)
			irq__add_routing(i, KVM_IRQ_ROUTING_IRQCHIP, IRQCHIP_MASTER, i);

	/* Hook next 8 GSIs to slave IRQCHIP */
	for (i = 8; i < 16; i++)
		irq__add_routing(i, KVM_IRQ_ROUTING_IRQCHIP, IRQCHIP_SLAVE, i - 8);

	/* Last but not least, IOAPIC */
	for (i = 0; i < 24; i++) {
		if (i == 0)
			irq__add_routing(i, KVM_IRQ_ROUTING_IRQCHIP, IRQCHIP_IOAPIC, 2);
		else if (i != 2)
			irq__add_routing(i, KVM_IRQ_ROUTING_IRQCHIP, IRQCHIP_IOAPIC, i);
	}

#ifndef CONFIG_POPCORN_HYPE
	r = ioctl(kvm->vm_fd, KVM_SET_GSI_ROUTING, irq_routing);
#else
skip:
	pthread_barrier_wait(&barrier_irq_setup);
// testing

	POP_DBG_PRINTF(pop_get_nid(),
			"\t[<%d>/%d] %s(): per system - after <*> "
			"BUT still TESTING (TODO) IF ON, BUT();.\n",
									pop_get_nid(), popcorn_gettid(), __func__);
//	/* Hook first 8 GSIs to master IRQCHIP */
//	for (i = 0; i < 8; i++)
//		if (i != 2)
//			irq__add_routing(i, KVM_IRQ_ROUTING_IRQCHIP, IRQCHIP_MASTER, i);
//
//	/* Hook next 8 GSIs to slave IRQCHIP */
//	for (i = 8; i < 16; i++)
//		irq__add_routing(i, KVM_IRQ_ROUTING_IRQCHIP, IRQCHIP_SLAVE, i - 8);
//
//	/* Last but not least, IOAPIC */
//	for (i = 0; i < 24; i++) {
//		if (i == 0)
//			irq__add_routing(i, KVM_IRQ_ROUTING_IRQCHIP, IRQCHIP_IOAPIC, 2);
//		else if (i != 2)
//			irq__add_routing(i, KVM_IRQ_ROUTING_IRQCHIP, IRQCHIP_IOAPIC, i);
//	}

// testing
	r = ioctl(kvm->vm_fd, KVM_SET_GSI_ROUTING, irq_routing);
#endif
	if (r) {
		BUG_ON(1);
		free(irq_routing);
		return errno;
	}

	next_gsi = i;

	return 0;
}
dev_base_init(irq__init);
