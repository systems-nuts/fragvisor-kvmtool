#include "kvm/kvm-cpu.h"

#include "kvm/kvm.h"
#include "kvm/util.h"

#include <sys/ioctl.h>
#include <stdlib.h>

#include <popcorn/utils.h>
#include <popcorn/debug.h>

#define	MAX_KVM_CPUID_ENTRIES		100

static void filter_cpuid(struct kvm_cpuid2 *kvm_cpuid)
{
	unsigned int signature[3];
	unsigned int i;

	/*
	 * Filter CPUID functions that are not supported by the hypervisor.
	 */
	for (i = 0; i < kvm_cpuid->nent; i++) {
		struct kvm_cpuid_entry2 *entry = &kvm_cpuid->entries[i];

		switch (entry->function) {
		case 0:
			/* Vendor name */
			memcpy(signature, "LKVMLKVMLKVM", 12);
			entry->ebx = signature[0];
			entry->ecx = signature[1];
			entry->edx = signature[2];
			break;
		case 1:
			/* Set X86_FEATURE_HYPERVISOR */
			if (entry->index == 0)
				entry->ecx |= (1 << 31);
			break;
		case 6:
			/* Clear X86_FEATURE_EPB */
			entry->ecx = entry->ecx & ~(1 << 3);
			break;
		case 10: { /* Architectural Performance Monitoring */
			union cpuid10_eax {
				struct {
					unsigned int version_id		:8;
					unsigned int num_counters	:8;
					unsigned int bit_width		:8;
					unsigned int mask_length	:8;
				} split;
				unsigned int full;
			} eax;

			/*
			 * If the host has perf system running,
			 * but no architectural events available
			 * through kvm pmu -- disable perf support,
			 * thus guest won't even try to access msr
			 * registers.
			 */
			if (entry->eax) {
				eax.full = entry->eax;
				if (eax.split.version_id != 2 ||
				    !eax.split.num_counters)
					entry->eax = 0;
			}
			break;
		}
		default:
			/* Keep the CPUID function as -is */
			break;
		};
	}
}

void kvm_cpu__setup_cpuid(struct kvm_cpu *vcpu)
{
	struct kvm_cpuid2 *kvm_cpuid = calloc(1, sizeof(*kvm_cpuid) +
				MAX_KVM_CPUID_ENTRIES * sizeof(*kvm_cpuid->entries));

	//POP_DPRINTF(pop_get_nid(), "\n\t\t[%d/%d] %s(): vcpu->vcpu_fd fd %d\n",
	//						pop_get_nid(), popcorn_gettid(), __func__, vcpu->vcpu_fd);
	POP_PRINTF("<%lu>=<fd %d>: KVM_GET_SUPPORTED_CPUID\n", vcpu->cpu_id, vcpu->vcpu_fd);
	kvm_cpuid->nent = MAX_KVM_CPUID_ENTRIES;
	if (ioctl(vcpu->kvm->sys_fd, KVM_GET_SUPPORTED_CPUID, kvm_cpuid) < 0)
		die_perror("KVM_GET_SUPPORTED_CPUID failed");

	filter_cpuid(kvm_cpuid);

	POP_PRINTF("<%lu>=<fd %d>: KVM_SET_CPUID2\n", vcpu->cpu_id, vcpu->vcpu_fd);
	if (ioctl(vcpu->vcpu_fd, KVM_SET_CPUID2, kvm_cpuid) < 0)
		die_perror("KVM_SET_CPUID2 failed");

	free(kvm_cpuid);
}
