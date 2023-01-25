#include "kvm/kvm.h"
#include "kvm/kvm-cpu.h"
#include "kvm/rbtree-interval.h"
#include "kvm/brlock.h"

#include <stdio.h>
#include <stdlib.h>

#include <sys/ioctl.h>
#include <linux/kvm.h>
#include <linux/types.h>
#include <linux/rbtree.h>
#include <linux/err.h>
#include <errno.h>

#include <popcorn/debug.h>

#define mmio_node(n) rb_entry(n, struct mmio_mapping, node)

struct mmio_mapping {
	struct rb_int_node	node;
	void			(*mmio_fn)(struct kvm_cpu *vcpu, u64 addr, u8 *data, u32 len, u8 is_write, void *ptr);
	void			*ptr;
};

static struct rb_root mmio_tree = RB_ROOT;

static struct mmio_mapping *mmio_search(struct rb_root *root, u64 addr, u64 len)
{
	struct rb_int_node *node;

	node = rb_int_search_range(root, addr, addr + len);
	if (node == NULL)
		return NULL;

	return mmio_node(node);
}

/* Find lowest match, Check for overlap */
static struct mmio_mapping *mmio_search_single(struct rb_root *root, u64 addr)
{
	struct rb_int_node *node;

	node = rb_int_search_single(root, addr);
	if (node == NULL)
		return NULL;

	return mmio_node(node);
}

static int mmio_insert(struct rb_root *root, struct mmio_mapping *data)
{
	return rb_int_insert(root, &data->node);
}

static const char *to_direction(u8 is_write)
{
	if (is_write)
		return "write";

	return "read";
}

#include <popcorn/utils.h>
/* from virtio_pci__init() */
int kvm__register_mmio(struct kvm *kvm, u64 phys_addr, u64 phys_addr_len, bool coalesce,
		       void (*mmio_fn)(struct kvm_cpu *vcpu, u64 addr, u8 *data, u32 len, u8 is_write, void *ptr),
			void *ptr)
{
	struct mmio_mapping *mmio;
	struct kvm_coalesced_mmio_zone zone;
	int ret;

#ifdef CONFIG_POPCORN_HYPE
	POP_PRINTF("\t\t<%d> %s: %s(): mmio_fn %p paddr %llx coalesce (%c)\n",
									pop_get_nid(), __FILE__, __func__,
									mmio_fn, phys_addr, coalesce ? 'O' : 'X');
#endif

	mmio = malloc(sizeof(*mmio));
	if (mmio == NULL)
		return -ENOMEM;

	*mmio = (struct mmio_mapping) {
		.node = RB_INT_INIT(phys_addr, phys_addr + phys_addr_len),
		.mmio_fn = mmio_fn,
		.ptr	= ptr,
	};

	if (coalesce) {
		zone = (struct kvm_coalesced_mmio_zone) {
			.addr	= phys_addr,
			.size	= phys_addr_len,
		};
		ret = ioctl(kvm->vm_fd, KVM_REGISTER_COALESCED_MMIO, &zone);
		if (ret < 0) {
			free(mmio);
			return -errno;
		}
	}
	br_write_lock(kvm);
	ret = mmio_insert(&mmio_tree, mmio); /* usr rb_treee insert mmio */
	br_write_unlock(kvm);
#ifdef CONFIG_POPCORN_HYPE
	POP_PRINTF("\t\t<%d> %s: %s(): 1 malloc and 1 mmio_insert()\n",
							pop_get_nid(), __FILE__, __func__);
#endif

	return ret;
}

bool kvm__deregister_mmio(struct kvm *kvm, u64 phys_addr)
{
	struct mmio_mapping *mmio;
	struct kvm_coalesced_mmio_zone zone;

	br_write_lock(kvm);
	mmio = mmio_search_single(&mmio_tree, phys_addr);
	if (mmio == NULL) {
		br_write_unlock(kvm);
		return false;
	}

	zone = (struct kvm_coalesced_mmio_zone) {
		.addr	= phys_addr,
		.size	= 1,
	};
	ioctl(kvm->vm_fd, KVM_UNREGISTER_COALESCED_MMIO, &zone);

	rb_int_erase(&mmio_tree, &mmio->node);
	br_write_unlock(kvm);

	free(mmio);
	return true;
}

bool kvm__emulate_mmio(struct kvm_cpu *vcpu, u64 phys_addr, u8 *data, u32 len, u8 is_write)
{
	struct mmio_mapping *mmio;

	br_read_lock();
	mmio = mmio_search(&mmio_tree, phys_addr, len);

#ifdef CONFIG_POPCORN_HYPE
	if (mmio) { 
#if 0
		//if (pop_get_nid()) {
			static int cnt = 0;
			cnt++;
			if (cnt <= 3) {
				kvm_cpu__show_registers(vcpu);
				kvm_cpu__show_code(vcpu);
			}
		//}
#endif
		static u64 cnt = 0;
		struct kvm_regs regs;
		if (ioctl(vcpu->vcpu_fd, KVM_GET_REGS, &regs) < 0)
		        die("KVM_GET_REGS failed");
		//unsigned long rip = regs.rip;

		cnt++;
		MMIOPF("\t\t<%d> %s(): mmio->mmio_fn %p "
				"paddr 0x%llx rip 0x%lx %c (HARM PERF) #%llu\n",
				pop_get_nid(), __func__, mmio->mmio_fn,
				phys_addr, rip, is_write? 'W' : 'R', cnt);
	} else {
		MMIOPF("\t\t<%d> %s(): WRONG EXIT 6 !mmio, paddr 0x%llx\n",
				pop_get_nid(), __func__, phys_addr);
	}
#endif
	if (mmio)
		mmio->mmio_fn(vcpu, phys_addr, data, len, is_write, mmio->ptr);
	else {
		if (vcpu->kvm->cfg.mmio_debug)
			fprintf(stderr,	"Warning: Ignoring MMIO %s at %016llx (length %u)\n",
				to_direction(is_write),
				(unsigned long long)phys_addr, len);
	}
	br_read_unlock();

	return true;
}
