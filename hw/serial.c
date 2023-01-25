#include "kvm/8250-serial.h"

#include "kvm/read-write.h"
#include "kvm/ioport.h"
#include "kvm/mutex.h"
#include "kvm/util.h"
#include "kvm/term.h"
#include "kvm/kvm.h"
#include "kvm/fdt.h"

#include <linux/types.h>
#include <linux/serial_reg.h>

#include <pthread.h>

#include <popcorn/utils.h>

/*
 * This fakes a U6_16550A. The fifo len needs to be 64 as the kernel
 * expects that for autodetection.
 */
#define FIFO_LEN		64
#define FIFO_MASK		(FIFO_LEN - 1)

#define UART_IIR_TYPE_BITS	0xc0

struct serial8250_device {
	struct mutex		mutex;
	u8			id;

	u16			iobase;
	u8			irq;
	u8			irq_state;
	int			txcnt;
	int			rxcnt;
	int			rxdone;
	char			txbuf[FIFO_LEN];
	char			rxbuf[FIFO_LEN];

	u8			dll;
	u8			dlm;
	u8			iir;
	u8			ier;
	u8			fcr;
	u8			lcr;
	u8			mcr;
	u8			lsr;
	u8			msr;
	u8			scr;
};

#define SERIAL_REGS_SETTING \
	.iir			= UART_IIR_NO_INT, \
	.lsr			= UART_LSR_TEMT | UART_LSR_THRE, \
	.msr			= UART_MSR_DCD | UART_MSR_DSR | UART_MSR_CTS, \
	.mcr			= UART_MCR_OUT2,

static struct serial8250_device devices[] = {
	/* ttyS0 */
	[0]	= {
		.mutex			= MUTEX_INITIALIZER,

		.id			= 0,
		.iobase			= 0x3f8,
		.irq			= 4,

		SERIAL_REGS_SETTING
	},
	/* ttyS1 */
	[1]	= {
		.mutex			= MUTEX_INITIALIZER,

		.id			= 1,
		.iobase			= 0x2f8,
		.irq			= 3,

		SERIAL_REGS_SETTING
	},
	/* ttyS2 */
	[2]	= {
		.mutex			= MUTEX_INITIALIZER,

		.id			= 2,
		.iobase			= 0x3e8,
		.irq			= 4,

		SERIAL_REGS_SETTING
	},
	/* ttyS3 */
	[3]	= {
		.mutex			= MUTEX_INITIALIZER,

		.id			= 3,
		.iobase			= 0x2e8,
		.irq			= 3,

		SERIAL_REGS_SETTING
	},
};

static void serial8250_flush_tx(struct kvm *kvm, struct serial8250_device *dev)
{
	dev->lsr |= UART_LSR_TEMT | UART_LSR_THRE;

	if (dev->txcnt) {
		term_putc(dev->txbuf, dev->txcnt, dev->id);
		dev->txcnt = 0;
	}
}

/* from _in/_out
 * Host mimics console device to (inject a irq) pull up/down irq level to kvm
 * (inject a pull-up/down votage level to irq pin in kvm)
 * LSR: line status register
 * IIR: interrupt ID register
 *
 * UART_LSR_TEMT   0x40            // Transmitter serial register empty
 * UART_LSR_THRE   0x20            // Transmitter holding register empty
 * UART_LSR_DR     0x01            // Receiver data ready
 *
 * buf:          // R/W: Rx & Tx buffer when DLAB=0
 * ier:          // R/W: Interrupt Enable Register
 * iir:          // R: Interrupt ID Register
 * lcr:          // R/W: Line Control Register
 * mcr:          // W: Modem Control Register
 * lsr:          // R: Line Status Register
 * msr:          // R: Modem Status Register
 * scr:          // R/W: Scratch Register
 */
static void serial8250_update_irq(struct kvm *kvm, struct serial8250_device *dev)
{
	u8 iir = 0;

	/* Handle clear rx */
	if (dev->lcr & UART_FCR_CLEAR_RCVR) {
		dev->lcr &= ~UART_FCR_CLEAR_RCVR;
		dev->rxcnt = dev->rxdone = 0;
		dev->lsr &= ~UART_LSR_DR;
#if DEBUG_SERIAL
#ifdef CONFIG_POPCORN_HYPE
		CONSOLEPRINTF("\t\t[%d/%d] %s(): <%d> clear rx (IN BEGINING)\n",
					pop_get_nid(), popcorn_gettid(), __func__, dev->id);
#endif
#endif
	}

	/* Handle clear tx */
	if (dev->lcr & UART_FCR_CLEAR_XMIT) {
		dev->lcr &= ~UART_FCR_CLEAR_XMIT;
		dev->txcnt = 0;
		dev->lsr |= UART_LSR_TEMT | UART_LSR_THRE;
#if DEBUG_SERIAL
#ifdef CONFIG_POPCORN_HYPE
		CONSOLEPRINTF("\t\t[%d/%d] %s(): <%d> clear tx\n",
					pop_get_nid(), popcorn_gettid(), __func__, dev->id);
#endif
#endif
	}

	/* IIR (Interrupt Identification Register) */
	/* Data ready and rcv interrupt enabled ? */
	if ((dev->ier & UART_IER_RDI) && (dev->lsr & UART_LSR_DR)) {
		iir |= UART_IIR_RDI; // 0x04
#if DEBUG_SERIAL
#ifdef CONFIG_POPCORN_HYPE
		CONSOLEPRINTF("\t\t[%d/%d] %s(): <%d> recved data available\n",
					pop_get_nid(), popcorn_gettid(), __func__, dev->id);
#endif
#endif
	}

	/* Transmitter empty and interrupt enabled ? */
	if ((dev->ier & UART_IER_THRI) && (dev->lsr & UART_LSR_TEMT)) {
#ifdef CONFIG_POPCORN_HYPE
		/* - start_tx() -> sets UART_IER_THRI.
				This will generate an interrupt once the FIFO is empty. */
#if DEBUG_SERIAL
		if (!pop_get_nid()) {
			CONSOLEPRINTF("\t\t[%d/%d] %s(): <%d> dev %p iir = tx empty 0x02\n",
						pop_get_nid(), popcorn_gettid(), __func__, dev->id, dev);
		} else { /* BUG */
			static unsigned long cnt3 = 0;
			cnt3++;
			POP_PRINTF("\t\t[%d/%d] %s(): dev %d iir = tx empty 0x02 (problem!!) "
					"#%lu\n",
					pop_get_nid(), popcorn_gettid(), __func__, dev->id, cnt3);
			//BUG_ON(pop_get_nid()); /* intercepted by remote (attention)*/
			//migrate(0, NULL, NULL);
		}
		/* pophype cannot do "iir |= UART_IIR_THRI;". I don't why but I guess
			this is the problem of my terminal bug. But if I use it, I cannot
			event do anything */
		iir |= UART_IIR_THRI; // 0x02 /* found until 11/09 */
#endif
		/* I just realized I remove iir |= UART_IIR_THRI;....... on 11/09 */
#else
		iir |= UART_IIR_THRI;
#endif
	}

#if 0
#define UART_LCR_DLAB	0x80	/* Divisor latch access bit */

#define UART_IER_MSI	0x08	/* Enable Modem status interrupt */
#define UART_IER_RLSI	0x04	/* Enable receiver line status interrupt */
#define UART_IER_THRI	0x02	/* Enable Transmitter holding register int. */
#define UART_IER_RDI	0x01	/* Enable receiver data interrupt */

#define UART_IIR_NO_INT	0x01	/* No interrupts pending */
#define UART_IIR_ID	0x06	/* Mask for the interrupt ID */

#define UART_IIR_MSI	0x00	/* Modem status interrupt */
#define UART_IIR_THRI	0x02	/* Transmitter holding register empty */
#define UART_IIR_RDI	0x04	/* Receiver data interrupt */
#define UART_IIR_RLSI	0x06	/* Receiver line status interrupt */
#endif

	/* Now update the irq line, if necessary */
	if (!iir) {
		dev->iir = UART_IIR_NO_INT; /* no int pending */
		if (dev->irq_state) {
#ifdef CONFIG_POPCORN_HYPE
#if DEBUG_SERIAL
			static unsigned long cnt = 0;
			cnt++;
			CONSOLEPRINTF("\t\t[%d/%d] %s(): <%d> read last irq state 0x%x "
						"(now !iir so next dev irq state = 0x%x) "
						"inject kvm_irq_line LOW #%lu\n",
						pop_get_nid(), popcorn_gettid(), __func__,
						dev->id, dev->irq_state, iir, cnt);
			//if (cnt >= 100)
			//	dump_stack(); // see who reset dev->irq_state
#endif
#endif
			kvm__irq_line(kvm, dev->irq, 0); /* LOW */
		}
	} else {
		static unsigned long cnt = 0;
		cnt++;
		dev->iir = iir; // install
#ifdef CONFIG_POPCORN_HYPE
#if DEBUG_SERIAL
		CONSOLEPRINTF("\t\t[%d/%d] %s(): dev %d read last irq state 0x%x "
						"(now !!iir so next dev irq state = 0x%x) #%lu\n",
						pop_get_nid(), popcorn_gettid(), __func__,
						dev->id, (int)dev->irq_state, iir, cnt);

		// debug
		static unsigned long cnt_kill_me = 0;
		//if (cnt_kill_me < 2000 && dev->irq_state == 0x2) {
		if (cnt_kill_me < 2000 && dev->irq_state == 0x0) {
			cnt_kill_me++;
			//dump_stack();
		}
#endif
#endif
		if (!dev->irq_state) {
			kvm__irq_line(kvm, dev->irq, 1); // level 1: high
#ifdef CONFIG_POPCORN_HYPE
#if DEBUG_SERIAL
			if (!pop_get_nid()) {
				CONSOLEPRINTF("\t\t[%d/%d] %s(): <%d> !!iir & state 0x%x (!dev->irq_state) "
								"inject kvm_irq_line HIGH #%lu\n",
								pop_get_nid(), popcorn_gettid(), __func__, dev->id, dev->irq_state, cnt);
			} else { /* BUG() */
				static unsigned long cnt2 = 0;
				cnt2++;
				POP_PRINTF("\t\t[%d/%d] %s(): <%d> state 0x%x inject kvm_irq_line HIGH (problem!!) #%lu\n",
									pop_get_nid(), popcorn_gettid(), __func__, dev->id, dev->irq_state, cnt2);
				//BUG_ON(pop_get_nid());
				//if (migrate_back || !pop_get_nid())
				//	migrate(migrate_back, NULL, NULL);
				//BUG_ON(pop_get_nid()); /* intercepted by remote (attention)*/
				//die();
			}
#endif
#endif
		}
	}
	dev->irq_state = iir; // install
#ifdef CONFIG_POPCORN_HYPE
//	tx/rx too many
//	static unsigned long cnt = 0;
//	cnt++;
//	CONSOLEPRINTF("\t\tt[%d/%d] %s(): <%d> set irq state 0x%x #%lu\n",
//		pop_get_nid(), popcorn_gettid(), __func__, dev->id, (int)dev->irq_state, cnt);
#endif

	/*
	 * If the kernel disabled the tx interrupt, we know that there
	 * is nothing more to transmit, so we can reset our tx logic
	 * here.
	 */
	if (!(dev->ier & UART_IER_THRI))
		serial8250_flush_tx(kvm, dev);
}

#define SYSRQ_PENDING_NONE		0

static int sysrq_pending;

static void serial8250__sysrq(struct kvm *kvm, struct serial8250_device *dev)
{
	dev->lsr |= UART_LSR_DR | UART_LSR_BI;
	dev->rxbuf[dev->rxcnt++] = sysrq_pending;
	sysrq_pending	= SYSRQ_PENDING_NONE;
#ifdef CONFIG_POPCORN_HYPE
	CONSOLEPRINTF("\t\t%s(): set UART_LSR_DR (save to rx buf)\n", __func__);
#endif
}

static void serial8250__receive(struct kvm *kvm, struct serial8250_device *dev,
				bool handle_sysrq)
{
	int c;

	if (dev->mcr & UART_MCR_LOOP) {
		CONSOLEPRINTF("\t\tt[%d/%d] %s(): exit return 1 (NEVER KILL ME) !!!\n",
						pop_get_nid(), popcorn_gettid(), __func__);
		return;
	}

	/* data ready 				||		rx data	*/
	if ((dev->lsr & UART_LSR_DR) || dev->rxcnt) {
#ifdef CONFIG_POPCORN_HYPE
#if DEBUG_SERIAL
		static unsigned int cnt_bug = 0;
		cnt_bug++;
		CONSOLEPRINTF("\t\tt[%d/%d] %s(): EXIT RETURN 2 "
			"(rx data ready: %c) *** BUG *** <%d> "
			//"(lead to inject - transmitter int enabled) "
			"dev->rxcnt %d dev->rxdone %d #%u\n",
			pop_get_nid(), popcorn_gettid(), __func__,
			(dev->lsr & UART_LSR_DR)? 'O': 'X', dev->id,
			dev->rxcnt, dev->rxdone, cnt_bug);
#endif
#endif
		return;
	}

	if (handle_sysrq && sysrq_pending) {
		serial8250__sysrq(kvm, dev);
		CONSOLEPRINTF("\t\tt[%d/%d] %s(): exit return 3 (NEVER KILL ME) !!!\n",
						pop_get_nid(), popcorn_gettid(), __func__);
		return;
	}

	if (kvm->cfg.active_console != CONSOLE_8250) {
		CONSOLEPRINTF("\t\tt[%d/%d] %s(): exit return 4 (NEVER KILL ME) !!!\n",
						pop_get_nid(), popcorn_gettid(), __func__);
		return;
	}

#ifdef CONFIG_POPCORN_HYPE
#if DEBUG_SERIAL
	/* bug */
	static unsigned long cnt = 0;
	cnt++;
	if (cnt < 4000 || !(cnt % 4000))
		CONSOLEPRINTF("%s(): got str #%lu\n", __func__, cnt);
	else if (cnt > 40001) {
		//die("lalalalala");
	}
#endif
#endif

	while (term_readable(dev->id) &&
	       dev->rxcnt < FIFO_LEN) {

		c = term_getc(kvm, dev->id);
#ifdef CONFIG_POPCORN_HYPE
#if DEBUG_SERIAL
		CONSOLEPRINTF("\t\t%s(): got char '%c'\n", __func__, (char)c);
#endif
#endif

		if (c < 0) {
#ifdef CONFIG_POPCORN_HYPE
#if DEBUG_SERIAL
			CONSOLEPRINTF("\t\t%s(): BAD exit\n", __func__);
#endif
#endif
			break;
		}
		dev->rxbuf[dev->rxcnt++] = c;
		dev->lsr |= UART_LSR_DR; /* vdev status changed - new data received */
#ifdef CONFIG_POPCORN_HYPE
#if DEBUG_SERIAL
		CONSOLEPRINTF("\t%s(): set UART_LSR_DR (new data received)\n",
																__func__);
#endif
#endif
	}
}

void serial8250__update_consoles(struct kvm *kvm)
{
	unsigned int i;

#if DEBUG_SERIAL
#ifdef CONFIG_POPCORN_HYPE
	/* bug */
	static unsigned long cnt = 0;
	cnt++;
	if (cnt < 100 || !(cnt % 100))
		CONSOLEPRINTF("\tt[%d/%d] %s(): got #%lu\n",
			pop_get_nid(), popcorn_gettid(), __func__, cnt);
	else if (cnt > 1001) {
		//die("lalalalala");
	}
#endif
#endif

	for (i = 0; i < ARRAY_SIZE(devices); i++) {
		struct serial8250_device *dev = &devices[i];

#if DEBUG_SERIAL
#ifdef CONFIG_POPCORN_HYPE
		// bug
		if (cnt < 100 || !(cnt % 100))
			CONSOLEPRINTF("\tt[%d/%d] %s(): got %u/%lu dev %d #%lu\n",
							pop_get_nid(), popcorn_gettid(), __func__,
							i, ARRAY_SIZE(devices) - 1, dev->id, cnt);
		else if (cnt > 1001) {
			//die("lalalalala");
		}
#endif
#endif

		mutex_lock(&dev->mutex);

		/* Restrict sysrq injection to the first port */
		serial8250__receive(kvm, dev, i == 0);

		serial8250_update_irq(kvm, dev);

		mutex_unlock(&dev->mutex);
	}
}

void serial8250__inject_sysrq(struct kvm *kvm, char sysrq)
{
	sysrq_pending = sysrq;
}

static bool serial8250_out(struct ioport *ioport, struct kvm_cpu *vcpu, u16 port,
			   void *data, int size)
{
	struct serial8250_device *dev = ioport->priv;
	u16 offset;
	bool ret = true;
	char *addr = data;

#ifdef CONFIG_POPCORN_HYPE
	// many
	static unsigned long cnt = 0;
	cnt++;
	offset = port - dev->iobase;
	if (offset == UART_TX &&
		!(dev->lcr & UART_LCR_DLAB) &&
		!(dev->mcr & UART_MCR_LOOP) &&
		(dev->txcnt < FIFO_LEN)) {
		/* From kvm-cpu.c VM EXIT_REASON == KVM_EXIT_IO */
		// guest printk word by word
		//CONSOLEPRINTF("\t[%d/%d] %s(): from VM_EXIT_IO kvm__emulate_io() dev %p #%lu\n",
		//				pop_get_nid(), popcorn_gettid(), __func__, dev, cnt);
	}
#endif

	mutex_lock(&dev->mutex);

	offset = port - dev->iobase;

	switch (offset) {
	case UART_TX:
		if (dev->lcr & UART_LCR_DLAB) {
#ifdef CONFIG_POPCORN_HYPE
			CONSOLEPRINTF("\t\t%s(): UART_LCR_DLAB ioport__read8 NEVER HAPPEN\n", __func__);
#endif
			dev->dll = ioport__read8(data);
			break;
		}

		/* Loopback mode */ /* pophype - loopback implemented here */
		if (dev->mcr & UART_MCR_LOOP) {
			if (dev->rxcnt < FIFO_LEN) {
				dev->rxbuf[dev->rxcnt++] = *addr;
				dev->lsr |= UART_LSR_DR; /* Receiver data ready */
#ifdef CONFIG_POPCORN_HYPE
				CONSOLEPRINTF("\t\t%s(): set UART_LSR_DR "
					"(recver data ready) (under loopback mode)\n", __func__);
#endif
			}
			break;
		}

		if (dev->txcnt < FIFO_LEN) {
			dev->txbuf[dev->txcnt++] = *addr;
			dev->lsr &= ~UART_LSR_TEMT;
			if (dev->txcnt == FIFO_LEN / 2)
				dev->lsr &= ~UART_LSR_THRE;
			serial8250_flush_tx(vcpu->kvm, dev);
		} else {
			/* Should never happpen */
			dev->lsr &= ~(UART_LSR_TEMT | UART_LSR_THRE);
		}
		break;
	case UART_IER:
		if (!(dev->lcr & UART_LCR_DLAB))
			dev->ier = ioport__read8(data) & 0x0f;
		else
			dev->dlm = ioport__read8(data);
		break;
	case UART_FCR:
		dev->fcr = ioport__read8(data);
		break;
	case UART_LCR:
		dev->lcr = ioport__read8(data);
		break;
	case UART_MCR:
		dev->mcr = ioport__read8(data);
		break;
	case UART_LSR:
		/* Factory test */
		break;
	case UART_MSR:
		/* Not used */
		break;
	case UART_SCR:
		dev->scr = ioport__read8(data);
		break;
	default:
		ret = false;
		break;
	}

	serial8250_update_irq(vcpu->kvm, dev);

	mutex_unlock(&dev->mutex);

	return ret;
}

static void serial8250_rx(struct serial8250_device *dev, void *data)
{
#ifdef CONFIG_POPCORN_HYPE
	//dump_stack();
	CONSOLEPRINTF("\t\texit[%d/%d] %s(): I'm a check after "
		"serial8250__receive()\n", pop_get_nid(), popcorn_gettid(), __func__);
#endif
	if (dev->rxdone == dev->rxcnt)
		return;

	/* Break issued ? */
	if (dev->lsr & UART_LSR_BI) {
		dev->lsr &= ~UART_LSR_BI;
		ioport__write8(data, 0);
#ifdef CONFIG_POPCORN_HYPE
		CONSOLEPRINTF("\t%s(): write 0 and RETURN !!!!@@@@@@@@@@@\n", __func__);
#endif
		return;
	}

	ioport__write8(data, dev->rxbuf[dev->rxdone++]);
#ifdef CONFIG_POPCORN_HYPE
	CONSOLEPRINTF("\t\texit[%d/%d] %s(): VM EXIT move "
				"*dev->rxbuf*[dev->rxdone++] to data '%c'\n",
				pop_get_nid(), popcorn_gettid(), __func__, *(char*)data);
#endif
	if (dev->rxcnt == dev->rxdone) {
		dev->lsr &= ~UART_LSR_DR;
		dev->rxcnt = dev->rxdone = 0;
#ifdef CONFIG_POPCORN_HYPE
		CONSOLEPRINTF("\t\texit[%d/%d] %s(): rxcnt == rxdone (GOOD clear rx)\n",
					pop_get_nid(), popcorn_gettid(), __func__);
#endif
	}
}

static bool serial8250_in(struct ioport *ioport, struct kvm_cpu *vcpu, u16 port, void *data, int size)
{
	struct serial8250_device *dev = ioport->priv;
	u16 offset;
	bool ret = true;

#ifdef CONFIG_POPCORN_HYPE
	offset = port - dev->iobase;
	if (offset == UART_RX && !(dev->lcr & UART_LCR_DLAB)) {
		/* From kvm-cpu.c VM EXIT_REASON == KVM_EXIT_IO */
		CONSOLEPRINTF("\texit[%d/%d] %s(): dev %d from "
					"VM_EXIT_IO kvm__emulate_io()\n",
					pop_get_nid(), popcorn_gettid(), __func__, dev->id);
	}
#endif

	mutex_lock(&dev->mutex);

	offset = port - dev->iobase;

	switch (offset) {
	case UART_RX:
		if (dev->lcr & UART_LCR_DLAB)
			ioport__write8(data, dev->dll);
		else
			serial8250_rx(dev, data);
		break;
	case UART_IER:
		if (dev->lcr & UART_LCR_DLAB)
			ioport__write8(data, dev->dlm);
		else
			ioport__write8(data, dev->ier);
		break;
	case UART_IIR:
		ioport__write8(data, dev->iir | UART_IIR_TYPE_BITS);
		break;
	case UART_LCR:
		ioport__write8(data, dev->lcr);
		break;
	case UART_MCR:
		ioport__write8(data, dev->mcr);
		break;
	case UART_LSR:
		ioport__write8(data, dev->lsr);
		break;
	case UART_MSR:
		ioport__write8(data, dev->msr);
		break;
	case UART_SCR:
		ioport__write8(data, dev->scr);
		break;
	default:
		ret = false;
		break;
	}

	serial8250_update_irq(vcpu->kvm, dev);

	mutex_unlock(&dev->mutex);

	return ret;
}

#ifdef CONFIG_HAS_LIBFDT

char *fdt_stdout_path = NULL;

#define DEVICE_NAME_MAX_LEN 32
static
void serial8250_generate_fdt_node(struct ioport *ioport, void *fdt,
				  void (*generate_irq_prop)(void *fdt,
							    u8 irq,
							    enum irq_type))
{
	char dev_name[DEVICE_NAME_MAX_LEN];
	struct serial8250_device *dev = ioport->priv;
	u64 addr = KVM_IOPORT_AREA + dev->iobase;
	u64 reg_prop[] = {
		cpu_to_fdt64(addr),
		cpu_to_fdt64(8),
	};

	snprintf(dev_name, DEVICE_NAME_MAX_LEN, "U6_16550A@%llx", addr);

	if (!fdt_stdout_path) {
		fdt_stdout_path = malloc(strlen(dev_name) + 2);
		/* Assumes that this node is a child of the root node. */
		sprintf(fdt_stdout_path, "/%s", dev_name);
	}

	_FDT(fdt_begin_node(fdt, dev_name));
	_FDT(fdt_property_string(fdt, "compatible", "ns16550a"));
	_FDT(fdt_property(fdt, "reg", reg_prop, sizeof(reg_prop)));
	generate_irq_prop(fdt, dev->irq, IRQ_TYPE_LEVEL_HIGH);
	_FDT(fdt_property_cell(fdt, "clock-frequency", 1843200));
	_FDT(fdt_end_node(fdt));
}
#else
#define serial8250_generate_fdt_node	NULL
#endif

static struct ioport_operations serial8250_ops = {
	.io_in			= serial8250_in,
	.io_out			= serial8250_out,
	.generate_fdt_node	= serial8250_generate_fdt_node,
};

static int serial8250__device_init(struct kvm *kvm, struct serial8250_device *dev)
{
	int r;

	ioport__map_irq(&dev->irq); // arch: x86 - null, arm - !null
	r = ioport__register(kvm, dev->iobase, &serial8250_ops, 8, dev);

	return r;
}

int serial8250__init(struct kvm *kvm)
{
	unsigned int i, j;
	int r = 0;

#ifdef CONFIG_POPCORN_HYPE
    if (pop_get_nid()) {
        POP_PRINTF("\t[%d/%d] %s(): SKIP serial8250..............\n",
                        pop_get_nid(), popcorn_gettid(), __func__);
        return 0;
    } else {
        POP_PRINTF("\t%s(): <*> serial8250\n", __func__);
    }
#endif

	for (i = 0; i < ARRAY_SIZE(devices); i++) {
		struct serial8250_device *dev = &devices[i];

#ifdef CONFIG_POPCORN_HYPE
        POP_PRINTF("\t%s(): <*> serial8250 devices[%d] base 0x%x "
				"init (-> ioport__register)\n", __func__, i, dev->iobase);
#endif
		r = serial8250__device_init(kvm, dev);
		if (r < 0)
			goto cleanup;
	}

	return r;
cleanup:
	for (j = 0; j <= i; j++) {
		struct serial8250_device *dev = &devices[j];

		ioport__unregister(kvm, dev->iobase);
	}

	return r;
}
dev_init(serial8250__init);

int serial8250__exit(struct kvm *kvm)
{
	unsigned int i;
	int r;

	for (i = 0; i < ARRAY_SIZE(devices); i++) {
		struct serial8250_device *dev = &devices[i];

		r = ioport__unregister(kvm, dev->iobase);
		if (r < 0)
			return r;
	}

	return 0;
}
dev_exit(serial8250__exit);
