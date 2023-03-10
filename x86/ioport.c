#include "kvm/ioport.h"

#include <stdlib.h>
#include <stdio.h>

#include <popcorn/utils.h>

static bool debug_io_out(struct ioport *ioport, struct kvm_cpu *vcpu, u16 port, void *data, int size)
{
	return 0;
}

static struct ioport_operations debug_ops = {
	.io_out		= debug_io_out,
};

static bool seabios_debug_io_out(struct ioport *ioport, struct kvm_cpu *vcpu, u16 port, void *data, int size)
{
	char ch;

	ch = ioport__read8(data);

	putchar(ch);

	return true;
}

static struct ioport_operations seabios_debug_ops = {
	.io_out		= seabios_debug_io_out,
};

static bool dummy_io_in(struct ioport *ioport, struct kvm_cpu *vcpu, u16 port, void *data, int size)
{
	return true;
}

static bool dummy_io_out(struct ioport *ioport, struct kvm_cpu *vcpu, u16 port, void *data, int size)
{
	return true;
}

static struct ioport_operations dummy_read_write_ioport_ops = {
	.io_in		= dummy_io_in,
	.io_out		= dummy_io_out,
};

static struct ioport_operations dummy_write_only_ioport_ops = {
	.io_out		= dummy_io_out,
};

/*
 * The "fast A20 gate"
 */

static bool ps2_control_a_io_in(struct ioport *ioport, struct kvm_cpu *vcpu, u16 port, void *data, int size)
{
	/*
	 * A20 is always enabled.
	 */
	ioport__write8(data, 0x02);

	return true;
}

static struct ioport_operations ps2_control_a_ops = {
	.io_in		= ps2_control_a_io_in,
	.io_out		= dummy_io_out,
};

void ioport__map_irq(u8 *irq)
{
}

void ioport__setup_arch(struct kvm *kvm)
{
	/* Legacy ioport setup */
#ifdef CONFIG_POPCORN_HYPE
	POP_PRINTF("\t[%d/%d] %s(): Legacy ioport setup\n",
			pop_get_nid(), popcorn_gettid(), __func__);
#endif

	/* 0000 - 001F - DMA1 controller */
	ioport__register(kvm, 0x0000, &dummy_read_write_ioport_ops, 32, NULL);

	/* 0x0020 - 0x003F - 8259A PIC 1 */
	ioport__register(kvm, 0x0020, &dummy_read_write_ioport_ops, 2, NULL);

	/* PORT 0040-005F - PIT - PROGRAMMABLE INTERVAL TIMER (8253, 8254) */
	ioport__register(kvm, 0x0040, &dummy_read_write_ioport_ops, 4, NULL);

	/* 0092 - PS/2 system control port A */
	ioport__register(kvm, 0x0092, &ps2_control_a_ops, 1, NULL);

	/* 0x00A0 - 0x00AF - 8259A PIC 2 */
	ioport__register(kvm, 0x00A0, &dummy_read_write_ioport_ops, 2, NULL);

	/* 00C0 - 001F - DMA2 controller */
	ioport__register(kvm, 0x00C0, &dummy_read_write_ioport_ops, 32, NULL);

	/* PORT 00E0-00EF are 'motherboard specific' so we use them for our
	   internal debugging purposes.  */
	ioport__register(kvm, IOPORT_DBG, &debug_ops, 1, NULL);

	/* PORT 00ED - DUMMY PORT FOR DELAY??? */
	ioport__register(kvm, 0x00ED, &dummy_write_only_ioport_ops, 1, NULL);

	/* 0x00F0 - 0x00FF - Math co-processor */
	ioport__register(kvm, 0x00F0, &dummy_write_only_ioport_ops, 2, NULL);

	/* PORT 0278-027A - PARALLEL PRINTER PORT (usually LPT1, sometimes LPT2) */
	ioport__register(kvm, 0x0278, &dummy_read_write_ioport_ops, 3, NULL);

	/* PORT 0378-037A - PARALLEL PRINTER PORT (usually LPT2, sometimes LPT3) */
	ioport__register(kvm, 0x0378, &dummy_read_write_ioport_ops, 3, NULL);

	/* PORT 03D4-03D5 - COLOR VIDEO - CRT CONTROL REGISTERS */
	ioport__register(kvm, 0x03D4, &dummy_read_write_ioport_ops, 1, NULL);
	ioport__register(kvm, 0x03D5, &dummy_write_only_ioport_ops, 1, NULL);

	ioport__register(kvm, 0x402, &seabios_debug_ops, 1, NULL);

	/* 0510 - QEMU BIOS configuration register */
	ioport__register(kvm, 0x510, &dummy_read_write_ioport_ops, 2, NULL);
}
