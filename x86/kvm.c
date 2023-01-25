#include "kvm/kvm.h"
#include "kvm/boot-protocol.h"
#include "kvm/cpufeature.h"
#include "kvm/interrupt.h"
#include "kvm/mptable.h"
#include "kvm/util.h"
#include "kvm/8250-serial.h"
#include "kvm/virtio-console.h"

#include <asm/bootparam.h>
#include <linux/kvm.h>
#include <linux/kernel.h>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

#include <popcorn/utils.h>

struct kvm_ext kvm_req_ext[] = {
	{ DEFINE_KVM_EXT(KVM_CAP_COALESCED_MMIO) },
	{ DEFINE_KVM_EXT(KVM_CAP_SET_TSS_ADDR) },
	{ DEFINE_KVM_EXT(KVM_CAP_PIT2) },
	{ DEFINE_KVM_EXT(KVM_CAP_USER_MEMORY) },
	{ DEFINE_KVM_EXT(KVM_CAP_IRQ_ROUTING) },
	{ DEFINE_KVM_EXT(KVM_CAP_IRQCHIP) },
	{ DEFINE_KVM_EXT(KVM_CAP_HLT) },
	{ DEFINE_KVM_EXT(KVM_CAP_IRQ_INJECT_STATUS) },
	{ DEFINE_KVM_EXT(KVM_CAP_EXT_CPUID) },
	{ 0, 0 }
};

bool kvm__arch_cpu_supports_vm(void)
{
	struct cpuid_regs regs;
	u32 eax_base;
	int feature;

	regs	= (struct cpuid_regs) {
		.eax		= 0x00,
	};
	host_cpuid(&regs);

	switch (regs.ebx) {
	case CPUID_VENDOR_INTEL_1:
		eax_base	= 0x00;
		feature		= KVM__X86_FEATURE_VMX;
		break;

	case CPUID_VENDOR_AMD_1:
		eax_base	= 0x80000000;
		feature		= KVM__X86_FEATURE_SVM;
		break;

	default:
		return false;
	}

	regs	= (struct cpuid_regs) {
		.eax		= eax_base,
	};
	host_cpuid(&regs);

	if (regs.eax < eax_base + 0x01)
		return false;

	regs	= (struct cpuid_regs) {
		.eax		= eax_base + 0x01
	};
	host_cpuid(&regs);

	return regs.ecx & (1 << feature);
}

/*
 * Allocating RAM size bigger than 4GB requires us to leave a gap
 * in the RAM which is used for PCI MMIO, hotplug, and unconfigured
 * devices (see documentation of e820_setup_gap() for details).
 *
 * If we're required to initialize RAM bigger than 4GB, we will create
 * a gap between 0xe0000000 and 0x100000000 in the guest virtual mem space.
 */
char *first_mem = NULL;
char *second_mem = NULL;
u64 first_mem_size = 0;
u64 second_mem_size = 0;
void kvm__init_ram(struct kvm *kvm)
{
	u64	phys_start, phys_size;
	void	*host_mem;

	if (kvm->ram_size < KVM_32BIT_GAP_START) {
		/* Use a single block of RAM for 32bit RAM */

		phys_start = 0;
		phys_size  = kvm->ram_size;
		host_mem   = kvm->ram_start;

		kvm__register_ram(kvm, phys_start, phys_size, host_mem);
#if POPHYPE_DEBUG
        printf("Not called\n");
#endif
	} else {
		/* First RAM range from zero to the PCI gap: */

		phys_start = 0;
		phys_size  = KVM_32BIT_GAP_START;
		host_mem   = kvm->ram_start;

#ifdef CONFIG_POPCORN_HYPE
		//POP_MEM_PRINTF(pop_get_nid(), "\n\t[%d][mem] - 1 ram phys_start(0) 0x%llx "
		//			"phys_size(KVM_32BIT_GAP_START) 0x%llx host_mem %p (kvm->ram_start)\n\n",
		//			pop_get_nid(), phys_start, phys_size, host_mem);
		POP_PRINTF("\n\t[mem] - 1 ram phys_start(0) 0x%llx "
                "\n\t\tphys_size(KVM_32BIT_GAP_START) 0x%llx (%llu MB)"
                "\n\t\thost_mem %p (kvm->ram_start)\n\n",
                phys_start, phys_size, phys_size / 1024 / 1024, host_mem);
#endif
		kvm__register_ram(kvm, phys_start, phys_size, host_mem);

#if defined(CONFIG_POPCORN_HYPE)
        first_mem = host_mem;
        first_mem_size = phys_size;
        printf("\n\t[mem] [first_mem %p] [first_mem_size 0x%llx %lld MB]\n",
                        first_mem, first_mem_size, first_mem_size / 1024 / 1024);
#endif
#if POPHYPE_DEBUG
		POP_PRINTF("\n\t\t\t[mem]: "
                "phys_start(guest_phys) %llx - %llx\n"
                "\t\t\thost_mem(host_vaddr)(saved) %p - %p size 0x%llx (%llx pgs)\n",
                //"\t\t\thost_mem(host_vaddr)(saved) %p - %p  %llx pgs\n",
                phys_start, phys_start + phys_size,
                //host_mem, host_mem + phys_size, phys_size / PAGE_SIZE);
                host_mem, host_mem + phys_size, phys_size, phys_size / PAGE_SIZE);
#endif

#ifdef CONFIG_POPCORN_HYPE
#if POPHYPE_FORCE_TOUCH_MEM
		/* kvm__register_ram <1> will fail */
		unsigned long i;
		volatile char a;
		/* Enforce to populate */
		POP_PRINTF("\n\n\t\tmem: done by [*]. [%d] enforce populate "
				"phys_start(guest_phys) %llx - %llx\n\n\n"
				"\t\t host_mem %p - %p  %llx pgs\n",
					pop_get_nid(), phys_start, phys_start + phys_size,
					host_mem, host_mem + phys_size, phys_size / PAGE_SIZE);
		for (i = 0; i < phys_size; i += 4096) {
			a += *((char *)host_mem + i);
		}
#else
		POP_PRINTF("\n\n\t\tskip enforce populate mem\n\n\n");
#endif
#endif

		/* Second RAM range from 4GB to the end of RAM: */
		phys_start = KVM_32BIT_MAX_MEM_SIZE;
		phys_size  = kvm->ram_size - phys_start;
		host_mem   = kvm->ram_start + phys_start;

#ifdef CONFIG_POPCORN_HYPE
		//POP_MEM_PRINTF(pop_get_nid(), "\n\t[*X%d] [mem] - 2 ram phys_start(KVM_32BIT_MAX_MEM_SIZE) 0x%llx "
		//			"phys_size(kvm->ram_size-KVM_32BIT_MAX_MEM_SIZE) 0x%llx "
		//			"host_mem %p (kvm->ram_start + phys_start)\n\n", pop_get_nid(),
		//				phys_start, phys_size, host_mem);
		POP_PRINTF("\n\t[mem] - 2 ram phys_start(KVM_32BIT_MAX_MEM_SIZE) 0x%llx "
                "\n\t\tphys_size(kvm->ram_size 0x%llx - KVM_32BIT_MAX_MEM_SIZE 0x%llx) "
                "0x%llx (%llu MB) "
                "\n\t\thost_mem %p (kvm->ram_start + phys_start)\n\n",
                phys_start, kvm->ram_size, KVM_32BIT_MAX_MEM_SIZE,
                phys_size, phys_size / 1024 / 1024, host_mem);
#endif

		kvm__register_ram(kvm, phys_start, phys_size, host_mem);
#if defined(CONFIG_POPCORN_HYPE)
        second_mem = host_mem;
        second_mem_size = phys_size;
		POP_PRINTF("\n\t[mem] [second_mem %p] [second_mem_size 0x%llx %llu MB]\n",
                    second_mem, second_mem_size, second_mem_size / 1024 / 1024);
#endif
#if POPHYPE_DEBUG
		POP_PRINTF("\n\t\t\t[mem]: "
                "phys_start(guest_phys) %llx - %llx\n"
                "\t\t\thost_mem(host_vaddr)(saved) %p - %p size 0x%llx (%llx pgs)\n",
                phys_start, phys_start + phys_size,
                host_mem, host_mem + phys_size, phys_size, phys_size / PAGE_SIZE);
#endif

#ifdef CONFIG_POPCORN_HYPE
#if POPHYPE_FORCE_TOUCH_MEM
		/* Enforce to populate */
		POP_PRINTF("\n\n\t\tmem: done by [*]. [%d] enforce populate "
				"phys_start(guest_phys) %llx - %llx \n"
				"\t\thost_mem %p - %p  %x pgs\n",
				pop_get_nid(), phys_start, phys_start + phys_size,
				host_mem, host_mem + phys_size, (unsigned int)(phys_size / PAGE_SIZE));
		for (i = 0; i < phys_size; i += 4096) {
			a += *((char *)host_mem + i);
		}
		POP_PRINTF("\t\ta = %d (for preventing from optimization)\n\n\n", a);
#else
		POP_PRINTF("\n\n\t\tskip enforce populate mem\n\n\n");
#endif
#endif
	}
}

/* Arch-specific commandline setup */
void kvm__arch_set_cmdline(char *cmdline, bool video)
{
	strcpy(cmdline, "noapic noacpi pci=conf1 reboot=k panic=1 i8042.direct=1 "
				"i8042.dumbkbd=1 i8042.nopnp=1");
	if (video)
		strcat(cmdline, " video=vesafb");
	else
		strcat(cmdline, " earlyprintk=serial i8042.noaux=1");
}

/* Architecture-specific KVM init */
extern pthread_barrier_t barrier_arch_init;
extern pthread_barrier_t barrier_arch_init_2;
extern pthread_barrier_t barrier_arch_init_3;
// TODO: Jack arch
void kvm__arch_init(struct kvm *kvm, const char *hugetlbfs_path, u64 ram_size)
{
	struct kvm_pit_config pit_config = { .flags = 0, };
	int ret;

#ifdef CONFIG_POPCORN_HYPE
#if RUN_REMOTE_MAIN_THREAD
    pthread_barrier_wait(&barrier_arch_init);
//	if (pop_get_nid()) {
//		POP_DPRINTF(pop_get_nid(),
//			"\t<%d> %s(): wait until origin done (SLEEPING 3s. TODO FIX IT) -> TSS\n",
//													pop_get_nid(), __func__);
//		sleep(pop_get_nid() * 3); // TODO don't remote!!! hacking works
//	}
#endif
#endif

#ifdef CONFIG_POPCORN_HYPE
	popcorn_serial_threads_start(); /* still buggy */
#endif

	/* __x86_set_memory_region vm_mmap */
	ret = ioctl(kvm->vm_fd, KVM_SET_TSS_ADDR, 0xfffbd000); // set limit 4G kvm_vm_ioctl_set_tss_addr arch/x86/kvm/vmx.c
	POP_DPRINTF(pop_get_nid(), "\t<%d> %s(): %s ret %d\n",
				pop_get_nid(), __func__, "KVM_SET_TSS_ADDR", ret);
	if (ret < 0)
		die_perror("KVM_SET_TSS_ADDR ioctl");

	ret = ioctl(kvm->vm_fd, KVM_CREATE_PIT2, &pit_config); // kern kvm_create_pit arch/x86/kvm/i8254.c
	POP_DPRINTF(pop_get_nid(), "\t<%d> %s(): %s ret %d\n",
				pop_get_nid(), __func__, "KVM_CREATE_PIT2", ret);
	if (ret < 0)
		die_perror("KVM_CREATE_PIT2 ioctl");

#ifdef CONFIG_POPCORN_HYPE
	popcorn_serial_threads_end();
#if RUN_REMOTE_MAIN_THREAD
    pthread_barrier_wait(&barrier_arch_init_2);
#endif
#endif


#ifdef CONFIG_POPCORN_HYPE
	if (pop_get_nid()) { goto skip_mem; }
#endif
	if (ram_size < KVM_32BIT_GAP_START) {
		MEMPRINTF("\t<*>[mem] ram_size < KVM_32BIT_GAP_START %llu\n", KVM_32BIT_GAP_START);
		kvm->ram_size = ram_size;
		kvm->ram_start = mmap_anon_or_hugetlbfs(kvm, hugetlbfs_path, ram_size);
	} else {
//#ifdef CONFIG_POPCORN_HYPE
//		if (pop_get_nid()) { goto skip_mem; }
//#endif
		kvm->ram_start = mmap_anon_or_hugetlbfs(kvm, hugetlbfs_path, ram_size + KVM_32BIT_GAP_SIZE);
		kvm->ram_size = ram_size + KVM_32BIT_GAP_SIZE;
#ifdef CONFIG_POPCORN_HYPE
		/* 0x7ffec0000000 ~ 0x7ffff0000000 size = 0x130000000 */
		MEMPRINTF("\t<*>[mem] kvm->ram_start [[[[[ %p ~ %llx ]]]]]\n", // 0x7ffec0000000 ~ xxx
						kvm->ram_start, (unsigned long)kvm->ram_start + kvm->ram_size);
		MEMPRINTF("\t<*>[mem] KVM_32BIT_GAP_START %llx\n", KVM_32BIT_GAP_START);
		MEMPRINTF("\t<*>[mem] KVM_32BIT_GAP_SIZE %d\n", KVM_32BIT_GAP_SIZE);
		MEMPRINTF("\t<*>[mem] KVM_32BIT_MAX_MEM_SIZE %llx\n", KVM_32BIT_MAX_MEM_SIZE);
		//POP_MEM_PRINTF(pop_get_nid(), "\t<*>[mem] kvm->ram_start touching/reading (%d)\n",
		//								*(int*)kvm->ram_start);
#endif

#ifdef CONFIG_POPCORN_HYPE
skip_mem:
#if RUN_REMOTE_MAIN_THREAD
		pthread_barrier_wait(&barrier_arch_init_3);
#endif
#endif
		if (kvm->ram_start != MAP_FAILED) {
			/*
			 * We mprotect the gap (see kvm__init_ram() for details) PROT_NONE so that
			 * if we accidently write to it, we will know.
			 */
			mprotect(kvm->ram_start + KVM_32BIT_GAP_START, KVM_32BIT_GAP_SIZE, PROT_NONE);
#ifdef CONFIG_POPCORN_HYPE
			/* TODO make sure*/
#endif
		}
	}
	POP_DPRINTF(pop_get_nid(), "\t<%d> %s(): kvm__arch_init: "
					"(mmap_anon_or_hugetlbfs) kvm->ram_size %llu\n",
							pop_get_nid(), __func__, kvm->ram_size);
	if (kvm->ram_start == MAP_FAILED)
		die("out of memory");

#ifdef CONFIG_POPCORN_HYPE
	/* TODO make sure*/
#endif
	madvise(kvm->ram_start, kvm->ram_size, MADV_MERGEABLE);

//#ifdef CONFIG_POPCORN_HYPE
//skip_mem:
//#if RUN_REMOTE_MAIN_THREAD
//    pthread_barrier_wait(&barrier_arch_init);
//#endif
//#endif

	ret = ioctl(kvm->vm_fd, KVM_CREATE_IRQCHIP); // vpic kvm_create_pic
	if (ret < 0)
		die_perror("KVM_CREATE_IRQCHIP ioctl");
}

void kvm__arch_delete_ram(struct kvm *kvm)
{
	munmap(kvm->ram_start, kvm->ram_size);
}

void kvm__irq_line(struct kvm *kvm, int irq, int level)
{
	struct kvm_irq_level irq_level;

	irq_level	= (struct kvm_irq_level) {
		{
			.irq		= irq,
		},
		.level		= level,
	};

	if (ioctl(kvm->vm_fd, KVM_IRQ_LINE, &irq_level) < 0)
		die_perror("KVM_IRQ_LINE failed");
}

void kvm__irq_trigger(struct kvm *kvm, int irq)
{
	kvm__irq_line(kvm, irq, 1);
	kvm__irq_line(kvm, irq, 0);
}

#define BOOT_LOADER_SELECTOR	0x1000
#define BOOT_LOADER_IP		0x0000
#define BOOT_LOADER_SP		0x8000
#define BOOT_CMDLINE_OFFSET	0x20000

#define BOOT_PROTOCOL_REQUIRED	0x206
#define LOAD_HIGH		0x01

static inline void *guest_real_to_host(struct kvm *kvm, u16 selector, u16 offset)
{
	unsigned long flat = ((u32)selector << 4) + offset;

	return guest_flat_to_host(kvm, flat);
}

static bool load_flat_binary(struct kvm *kvm, int fd_kernel)
{
	void *p;

	if (lseek(fd_kernel, 0, SEEK_SET) < 0)
		die_perror("lseek");

	p = guest_real_to_host(kvm, BOOT_LOADER_SELECTOR, BOOT_LOADER_IP);

	if (read_file(fd_kernel, p, kvm->cfg.ram_size) < 0)
		die_perror("read");

	kvm->arch.boot_selector	= BOOT_LOADER_SELECTOR;
	kvm->arch.boot_ip	= BOOT_LOADER_IP;
	kvm->arch.boot_sp	= BOOT_LOADER_SP;

	return true;
}

static const char *BZIMAGE_MAGIC = "HdrS";

static bool load_bzimage(struct kvm *kvm, int fd_kernel, int fd_initrd,
			 const char *kernel_cmdline)
{
	struct boot_params *kern_boot;
	struct boot_params boot;
	size_t cmdline_size;
	ssize_t file_size;
	void *p;
	u16 vidmode;

	/*
	 * See Documentation/x86/boot.txt for details no bzImage on-disk and
	 * memory layout.
	 */

	if (read_in_full(fd_kernel, &boot, sizeof(boot)) != sizeof(boot))
		return false;

	if (memcmp(&boot.hdr.header, BZIMAGE_MAGIC, strlen(BZIMAGE_MAGIC)))
		return false;

	if (boot.hdr.version < BOOT_PROTOCOL_REQUIRED)
		die("Too old kernel");

	if (lseek(fd_kernel, 0, SEEK_SET) < 0)
		die_perror("lseek");

	if (!boot.hdr.setup_sects)
		boot.hdr.setup_sects = BZ_DEFAULT_SETUP_SECTS;
	file_size = (boot.hdr.setup_sects + 1) << 9;
	p = guest_real_to_host(kvm, BOOT_LOADER_SELECTOR, BOOT_LOADER_IP);
	if (read_in_full(fd_kernel, p, file_size) != file_size)
		die_perror("kernel setup read");

	/* read actual kernel image (vmlinux.bin) to BZ_KERNEL_START */
	p = guest_flat_to_host(kvm, BZ_KERNEL_START);
	file_size = read_file(fd_kernel, p,
			      kvm->cfg.ram_size - BZ_KERNEL_START);
	if (file_size < 0)
		die_perror("kernel read");

	p = guest_flat_to_host(kvm, BOOT_CMDLINE_OFFSET);
	if (kernel_cmdline) {
		cmdline_size = strlen(kernel_cmdline) + 1;
		if (cmdline_size > boot.hdr.cmdline_size)
			cmdline_size = boot.hdr.cmdline_size;

		memset(p, 0, boot.hdr.cmdline_size);
		memcpy(p, kernel_cmdline, cmdline_size - 1);
	}

	/* vidmode should be either specified or set by default */
	if (kvm->cfg.vnc || kvm->cfg.sdl || kvm->cfg.gtk) {
		if (!kvm->cfg.arch.vidmode)
			vidmode = 0x312;
		else
			vidmode = kvm->cfg.arch.vidmode;
	} else {
		vidmode = 0;
	}

	kern_boot	= guest_real_to_host(kvm, BOOT_LOADER_SELECTOR, 0x00);

	kern_boot->hdr.cmd_line_ptr	= BOOT_CMDLINE_OFFSET;
	kern_boot->hdr.type_of_loader	= 0xff;
	kern_boot->hdr.heap_end_ptr	= 0xfe00;
	kern_boot->hdr.loadflags	|= CAN_USE_HEAP;
	kern_boot->hdr.vid_mode		= vidmode;

	/*
	 * Read initrd image into guest memory
	 */
	if (fd_initrd >= 0) {
		struct stat initrd_stat;
		unsigned long addr;

		if (fstat(fd_initrd, &initrd_stat))
			die_perror("fstat");

		addr = boot.hdr.initrd_addr_max & ~0xfffff;
		for (;;) {
			if (addr < BZ_KERNEL_START)
				die("Not enough memory for initrd");
			else if (addr < (kvm->ram_size - initrd_stat.st_size))
				break;
			addr -= 0x100000;
		}

		p = guest_flat_to_host(kvm, addr);
		if (read_in_full(fd_initrd, p, initrd_stat.st_size) < 0)
			die("Failed to read initrd");

		kern_boot->hdr.ramdisk_image	= addr;
		kern_boot->hdr.ramdisk_size	= initrd_stat.st_size;
	}

	kvm->arch.boot_selector = BOOT_LOADER_SELECTOR;
	/*
	 * The real-mode setup code starts at offset 0x200 of a bzImage. See
	 * Documentation/x86/boot.txt for details.
	 */
	kvm->arch.boot_ip = BOOT_LOADER_IP + 0x200;
	kvm->arch.boot_sp = BOOT_LOADER_SP;

	return true;
}

bool kvm__arch_load_kernel_image(struct kvm *kvm, int fd_kernel, int fd_initrd,
				 const char *kernel_cmdline)
{
	if (load_bzimage(kvm, fd_kernel, fd_initrd, kernel_cmdline))
		return true;
	pr_warning("Kernel image is not a bzImage.");
	pr_warning("Trying to load it as a flat binary (no cmdline support)");

	if (fd_initrd != -1)
		pr_warning("Loading initrd with flat binary not supported.");

	return load_flat_binary(kvm, fd_kernel);
}

/**
 * kvm__arch_setup_firmware - inject BIOS into guest system memory
 * @kvm - guest system descriptor
 *
 * This function is a main routine where we poke guest memory
 * and install BIOS there.
 */
int kvm__arch_setup_firmware(struct kvm *kvm)
{
	/* standart minimal configuration */
	setup_bios(kvm);

	/* FIXME: SMP, ACPI and friends here */

	return 0;
}

int kvm__arch_free_firmware(struct kvm *kvm)
{
	return 0;
}

void kvm__arch_read_term(struct kvm *kvm)
{
	serial8250__update_consoles(kvm); /* hw/serial.c */
	virtio_console__inject_interrupt(kvm); /* virtio/console.c */
}
