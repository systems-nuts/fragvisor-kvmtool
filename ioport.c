#include "kvm/ioport.h"

#include "kvm/kvm.h"
#include "kvm/util.h"
#include "kvm/brlock.h"
#include "kvm/rbtree-interval.h"
#include "kvm/mutex.h"

#include <linux/kvm.h>	/* for KVM_EXIT_* */
#include <linux/types.h>

#include <stdbool.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

#include <popcorn/utils.h>

#define ioport_node(n) rb_entry(n, struct ioport, node)

DEFINE_MUTEX(ioport_mutex);

static u16			free_io_port_idx; /* protected by ioport_mutex */

static struct rb_root		ioport_tree = RB_ROOT;

static u16 ioport__find_free_port(void)
{
	u16 free_port;

	mutex_lock(&ioport_mutex);
	free_port = IOPORT_START + free_io_port_idx * IOPORT_SIZE;
	free_io_port_idx++;
	mutex_unlock(&ioport_mutex);

	return free_port;
}

static struct ioport *ioport_search(struct rb_root *root, u64 addr)
{
	struct rb_int_node *node;

	node = rb_int_search_single(root, addr);
	if (node == NULL)
		return NULL;

	return ioport_node(node);
}

static int ioport_insert(struct rb_root *root, struct ioport *data)
{
	return rb_int_insert(root, &data->node);
}

static void ioport_remove(struct rb_root *root, struct ioport *data)
{
	rb_int_erase(root, &data->node);
}

#ifdef CONFIG_HAS_LIBFDT
static void generate_ioport_fdt_node(void *fdt,
				     struct device_header *dev_hdr,
				     void (*generate_irq_prop)(void *fdt,
							       u8 irq,
							       enum irq_type))
{
	struct ioport *ioport = container_of(dev_hdr, struct ioport, dev_hdr);
	struct ioport_operations *ops = ioport->ops;

	if (ops->generate_fdt_node)
		ops->generate_fdt_node(ioport, fdt, generate_irq_prop);
}
#else
static void generate_ioport_fdt_node(void *fdt,
				     struct device_header *dev_hdr,
				     void (*generate_irq_prop)(void *fdt,
							       u8 irq,
							       enum irq_type))
{
	die("Unable to generate device tree nodes without libfdt\n");
}
#endif

int ioport__register(struct kvm *kvm, u16 port, struct ioport_operations *ops, int count, void *param)
{
	struct ioport *entry;
	int r;

#ifdef CONFIG_POPCORN_HYPE
	POP_DPRINTF(pop_get_nid(), "\t[<%d>/%d] %s: %s(): (general) register "
			"port 0x%x ops %p cnt %d device__register(){ (can skip) }\n",
			pop_get_nid(), popcorn_gettid(), __FILE__, __func__,
			port, ops, count);
	/* the most inportant thing is to register ops */
#endif

	br_write_lock(kvm);
	if (port == IOPORT_EMPTY)
		port = ioport__find_free_port();

	entry = ioport_search(&ioport_tree, port);
	if (entry) {
		pr_warning("ioport re-registered: 0x%x", port);
		rb_int_erase(&ioport_tree, &entry->node);
	}

	entry = malloc(sizeof(*entry));
	if (entry == NULL)
		return -ENOMEM;

	*entry = (struct ioport) {
		.node		= RB_INT_INIT(port, port + count),
		.ops		= ops,
		.priv		= param,
		.dev_hdr	= (struct device_header) {
			.bus_type	= DEVICE_BUS_IOPORT,
			.data		= generate_ioport_fdt_node,
		},
	};

	r = ioport_insert(&ioport_tree, entry);
	if (r < 0) {
		free(entry);
		br_write_unlock(kvm);
		return r;
	}

	device__register(&entry->dev_hdr);
	br_write_unlock(kvm);

	return port;
}

int ioport__unregister(struct kvm *kvm, u16 port)
{
	struct ioport *entry;
	int r;

	br_write_lock(kvm);

	r = -ENOENT;
	entry = ioport_search(&ioport_tree, port);
	if (!entry)
		goto done;

	device__unregister(&entry->dev_hdr);
	ioport_remove(&ioport_tree, entry);

	free(entry);

	r = 0;

done:
	br_write_unlock(kvm);

	return r;
}

static void ioport__unregister_all(void)
{
	struct ioport *entry;
	struct rb_node *rb;
	struct rb_int_node *rb_node;

	rb = rb_first(&ioport_tree);
	while (rb) {
		rb_node = rb_int(rb);
		entry = ioport_node(rb_node);
		device__unregister(&entry->dev_hdr);
		ioport_remove(&ioport_tree, entry);
		free(entry);
		rb = rb_first(&ioport_tree);
	}
}

static const char *to_direction(int direction)
{
	if (direction == KVM_EXIT_IO_IN)
		return "IN";
	else
		return "OUT";
}

static void ioport_error(u16 port, void *data, int direction, int size, u32 count)
{
	fprintf(stderr, "IO error: %s port=%x, size=%d, count=%u\n", to_direction(direction), port, size, count);
}

#if 0 /* It didn't work */
#include <linux/serial_reg.h>
#define FIFO_LEN        64
#define FIFO_MASK       (FIFO_LEN - 1)
#define UART_IIR_TYPE_BITS  0xc0
struct serial8250_device {
    struct mutex        mutex;
    u8          id;

    u16         iobase;
    u8          irq;
    u8          irq_state;
    int         txcnt;
    int         rxcnt;
    int         rxdone;
    char            txbuf[FIFO_LEN];
    char            rxbuf[FIFO_LEN];

    u8          dll;
    u8          dlm;
    u8          iir;
    u8          ier;
    u8          fcr;
    u8          lcr;
    u8          mcr;
    u8          lsr;
    u8          msr;
    u8          scr;
};
#endif

bool kvm__emulate_io(struct kvm_cpu *vcpu, u16 port, void *data, int direction, int size, u32 count)
{
	struct ioport_operations *ops;
	bool ret = false;
	struct ioport *entry;
	void *ptr = data;
	struct kvm *kvm = vcpu->kvm;

	br_read_lock();
	entry = ioport_search(&ioport_tree, port);
	if (!entry)
		goto out;

	ops	= entry->ops;

	while (count--) {
		if (direction == KVM_EXIT_IO_IN && ops->io_in) {
#ifdef CONFIG_POPCORN_HYPE
#if 0 /* It didn't work */
			/* From kvm-cpu.c VM EXIT_REASON == KVM_EXIT_IO */
			//if (entry->priv == )
			//struct serial8250_device *dev = (struct serial8250_device *)((struct ioport *)entry->priv);
			struct serial8250_device *dev = entry->priv;
			u16 offset = port - dev->iobase;
//			if (offset == UART_RX && !(dev->lcr & UART_LCR_DLAB)) {
//				CONSOLEPRINTF("\t[%d/%d] %s(): entry->priv %p (dev) here\n",
//					pop_get_nid(), popcorn_gettid(), __func__, entry->priv);
//			}
#endif
#endif
			ret = ops->io_in(entry, vcpu, port, ptr, size);
		} else if (direction == KVM_EXIT_IO_OUT && ops->io_out)
			ret = ops->io_out(entry, vcpu, port, ptr, size);

		ptr += size;
	}

out:
	br_read_unlock();

	if (ret)
		return true;

	if (kvm->cfg.ioport_debug)
		ioport_error(port, data, direction, size, count);

	return !kvm->cfg.ioport_debug;
}

int ioport__init(struct kvm *kvm)
{
#ifdef CONFIG_POPCORN_HYPE
	if (pop_get_nid()) { /* because of fd */
		POP_PRINTF("\t[%d/%d] %s(): SKIP ioport_init\n",
				pop_get_nid(), popcorn_gettid(), __func__);
		return 0;
	}
	POP_PRINTF("\t[*/%d] %s():\n", popcorn_gettid(), __func__);
#endif
	ioport__setup_arch(kvm);

	return 0;
}
dev_base_init(ioport__init);

int ioport__exit(struct kvm *kvm)
{
	ioport__unregister_all();
	return 0;
}
dev_base_exit(ioport__exit);
