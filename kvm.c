#include "kvm/kvm.h"
#include "kvm/read-write.h"
#include "kvm/util.h"
#include "kvm/strbuf.h"
#include "kvm/mutex.h"
#include "kvm/kvm-cpu.h"
#include "kvm/kvm-ipc.h"

#include <linux/kernel.h>
#include <linux/kvm.h>
#include <linux/list.h>
#include <linux/err.h>

#include <sys/un.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <sys/eventfd.h>
#include <asm/unistd.h>
#include <dirent.h>

#include <popcorn/utils.h>

#define DEFINE_KVM_EXIT_REASON(reason) [reason] = #reason

#if defined(CONFIG_POPCORN_HYPE)
extern volatile bool ckpt_done[];
extern volatile bool restart_done[];
extern volatile bool all_done[];
#endif

const char *kvm_exit_reasons[] = {
	DEFINE_KVM_EXIT_REASON(KVM_EXIT_UNKNOWN),
	DEFINE_KVM_EXIT_REASON(KVM_EXIT_EXCEPTION),
	DEFINE_KVM_EXIT_REASON(KVM_EXIT_IO),
	DEFINE_KVM_EXIT_REASON(KVM_EXIT_HYPERCALL),
	DEFINE_KVM_EXIT_REASON(KVM_EXIT_DEBUG),
	DEFINE_KVM_EXIT_REASON(KVM_EXIT_HLT),
	DEFINE_KVM_EXIT_REASON(KVM_EXIT_MMIO),
	DEFINE_KVM_EXIT_REASON(KVM_EXIT_IRQ_WINDOW_OPEN),
	DEFINE_KVM_EXIT_REASON(KVM_EXIT_SHUTDOWN),
	DEFINE_KVM_EXIT_REASON(KVM_EXIT_FAIL_ENTRY),
	DEFINE_KVM_EXIT_REASON(KVM_EXIT_INTR),
	DEFINE_KVM_EXIT_REASON(KVM_EXIT_SET_TPR),
	DEFINE_KVM_EXIT_REASON(KVM_EXIT_TPR_ACCESS),
	DEFINE_KVM_EXIT_REASON(KVM_EXIT_S390_SIEIC),
	DEFINE_KVM_EXIT_REASON(KVM_EXIT_S390_RESET),
	DEFINE_KVM_EXIT_REASON(KVM_EXIT_DCR),
	DEFINE_KVM_EXIT_REASON(KVM_EXIT_NMI),
	DEFINE_KVM_EXIT_REASON(KVM_EXIT_INTERNAL_ERROR),
#ifdef CONFIG_PPC64
	DEFINE_KVM_EXIT_REASON(KVM_EXIT_PAPR_HCALL),
#endif
};

static int pause_event;
static DEFINE_MUTEX(pause_lock);
extern struct kvm_ext kvm_req_ext[];

static char kvm_dir[PATH_MAX];

extern __thread struct kvm_cpu *current_kvm_cpu;

static int set_dir(const char *fmt, va_list args)
{
	char tmp[PATH_MAX];

	vsnprintf(tmp, sizeof(tmp), fmt, args);

	mkdir(tmp, 0777);

	if (!realpath(tmp, kvm_dir))
		return -errno;

	strcat(kvm_dir, "/");

	return 0;
}

void kvm__set_dir(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	set_dir(fmt, args);
	va_end(args);
}

const char *kvm__get_dir(void)
{
	return kvm_dir;
}

bool kvm__supports_vm_extension(struct kvm *kvm, unsigned int extension)
{
	static int supports_vm_ext_check = 0;
	int ret;

	switch (supports_vm_ext_check) {
	case 0:
		ret = ioctl(kvm->sys_fd, KVM_CHECK_EXTENSION,
			    KVM_CAP_CHECK_EXTENSION_VM);
		if (ret <= 0) {
			supports_vm_ext_check = -1;
			return false;
		}
		supports_vm_ext_check = 1;
		/* fall through */
	case 1:
		break;
	case -1:
		return false;
	}

	ret = ioctl(kvm->vm_fd, KVM_CHECK_EXTENSION, extension);
	if (ret < 0)
		return false;

	return ret;
}

bool kvm__supports_extension(struct kvm *kvm, unsigned int extension)
{
	int ret;

	ret = ioctl(kvm->sys_fd, KVM_CHECK_EXTENSION, extension);
	if (ret < 0)
		return false;

	return ret;
}

static int kvm__check_extensions(struct kvm *kvm)
{
	int i;

	for (i = 0; ; i++) {
		if (!kvm_req_ext[i].name)
			break;
		if (!kvm__supports_extension(kvm, kvm_req_ext[i].code)) {
			pr_err("Unsupported KVM extension detected: %s",
				kvm_req_ext[i].name);
			return -i;
		}
	}

	return 0;
}

struct kvm *kvm__new(void)
{
	struct kvm *kvm = calloc(1, sizeof(*kvm));
	if (!kvm)
		return ERR_PTR(-ENOMEM);

	kvm->sys_fd = -1;
	kvm->vm_fd = -1;

	return kvm;
}

int kvm__exit(struct kvm *kvm)
{
	struct kvm_mem_bank *bank, *tmp;

	kvm__arch_delete_ram(kvm);

	list_for_each_entry_safe(bank, tmp, &kvm->mem_banks, list) {
		list_del(&bank->list);
		free(bank);
	}

	free(kvm);
	return 0;
}
core_exit(kvm__exit);

// From different archs
int kvm__register_mem(struct kvm *kvm, u64 guest_phys, u64 size,
		      void *userspace_addr, enum kvm_mem_type type)
{
	struct kvm_userspace_memory_region mem;
	struct kvm_mem_bank *merged = NULL;
	struct kvm_mem_bank *bank;
	int ret;

	/* Check for overlap */
	list_for_each_entry(bank, &kvm->mem_banks, list) {
		u64 bank_end = bank->guest_phys_addr + bank->size - 1;
		u64 end = guest_phys + size - 1;
		if (guest_phys > bank_end || end < bank->guest_phys_addr)
			continue;

		/* Merge overlapping reserved regions */
		if (bank->type == KVM_MEM_TYPE_RESERVED &&
		    type == KVM_MEM_TYPE_RESERVED) {
			bank->guest_phys_addr = min(bank->guest_phys_addr, guest_phys);
			bank->size = max(bank_end, end) - bank->guest_phys_addr + 1;

			if (merged) {
				/*
				 * This is at least the second merge, remove
				 * previous result.
				 */
				list_del(&merged->list);
				free(merged);
			}

			guest_phys = bank->guest_phys_addr;
			size = bank->size;
			merged = bank;

			/* Keep checking that we don't overlap another region */
			continue;
		}

#ifdef CONFIG_POPCORN_HYPE
		POP_DPRINTF(pop_get_nid(), "%s region [%llx-%llx] would overlap %s region [%llx-%llx]",
				   kvm_mem_type_to_string(type), guest_phys, guest_phys + size - 1,
				   kvm_mem_type_to_string(bank->type), bank->guest_phys_addr,
				   bank->guest_phys_addr + bank->size - 1);
#else
		pr_err("%s region [%llx-%llx] would overlap %s region [%llx-%llx]",
		       kvm_mem_type_to_string(type), guest_phys, guest_phys + size - 1,
		       kvm_mem_type_to_string(bank->type), bank->guest_phys_addr,
		       bank->guest_phys_addr + bank->size - 1);
#endif
#ifdef CONFIG_POPCORN_HYPE
		POP_DPRINTF(pop_get_nid(), "\t<%d> %s(): only remote reaches here\n",
													pop_get_nid(), __func__);
		goto skip_listadd;
#endif
		BUG_ON(-1);
		return -EINVAL;
	}

	if (merged)
		return 0;

	bank = malloc(sizeof(*bank));
	if (!bank)
		return -ENOMEM;

	INIT_LIST_HEAD(&bank->list);
	bank->guest_phys_addr		= guest_phys;
	bank->host_addr			= userspace_addr;
	bank->size			= size;
	bank->type			= type;

#ifdef CONFIG_POPCORN_HYPE
	list_add(&bank->list, &kvm->mem_banks);
#endif

#ifdef CONFIG_POPCORN_HYPE
skip_listadd:
#endif
	if (type != KVM_MEM_TYPE_RESERVED) {
		/* both kernel need this info */
		mem = (struct kvm_userspace_memory_region) {
			.slot			= kvm->mem_slots++,
			.guest_phys_addr	= guest_phys,
			.memory_size		= size,
			.userspace_addr		= (unsigned long)userspace_addr,
		};

		ret = ioctl(kvm->vm_fd, KVM_SET_USER_MEMORY_REGION, &mem);
		if (ret < 0)
			return -errno;
	}
#ifdef CONFIG_POPCORN_HYPE
	/* Enforce to populate - already touched ouside kvm__init_ram()*/
	//POP_PRINTF("mem: guest_phys %lx\n", guest_phys);
#endif

#ifndef CONFIG_POPCORN_HYPE
	list_add(&bank->list, &kvm->mem_banks);
#endif

	return 0;
}

void *guest_flat_to_host(struct kvm *kvm, u64 offset)
{
	struct kvm_mem_bank *bank;

	list_for_each_entry(bank, &kvm->mem_banks, list) {
		u64 bank_start = bank->guest_phys_addr;
		u64 bank_end = bank_start + bank->size;

		if (offset >= bank_start && offset < bank_end) {
			POP_ADDR_PRINTF(pop_get_nid(), "\t\t<%d> %s(): guest offset 0x%lx -> "
														"host offset 0x%lx\n",
												pop_get_nid(), __func__, offset,
									bank->host_addr + (offset - bank_start));
			return bank->host_addr + (offset - bank_start);
		}
	}

	pr_warning("unable to translate guest address 0x%llx to host",
			(unsigned long long)offset);
	return NULL;
}

u64 host_to_guest_flat(struct kvm *kvm, void *ptr)
{
	struct kvm_mem_bank *bank;

	list_for_each_entry(bank, &kvm->mem_banks, list) {
		void *bank_start = bank->host_addr;
		void *bank_end = bank_start + bank->size;

		if (ptr >= bank_start && ptr < bank_end)
			return bank->guest_phys_addr + (ptr - bank_start);
	}

	pr_warning("unable to translate host address %p to guest", ptr);
	return 0;
}

/*
 * Iterate over each registered memory bank. Call @fun for each bank with @data
 * as argument. @type is a bitmask that allows to filter banks according to
 * their type.
 *
 * If one call to @fun returns a non-zero value, stop iterating and return the
 * value. Otherwise, return zero.
 */
int kvm__for_each_mem_bank(struct kvm *kvm, enum kvm_mem_type type,
			   int (*fun)(struct kvm *kvm, struct kvm_mem_bank *bank, void *data),
			   void *data)
{
	int ret;
	struct kvm_mem_bank *bank;

	list_for_each_entry(bank, &kvm->mem_banks, list) {
		if (type != KVM_MEM_TYPE_ALL && !(bank->type & type))
			continue;

		ret = fun(kvm, bank, data);
		if (ret)
			break;
	}

	return ret;
}

int kvm__recommended_cpus(struct kvm *kvm)
{
	int ret;

	ret = ioctl(kvm->sys_fd, KVM_CHECK_EXTENSION, KVM_CAP_NR_VCPUS);
	if (ret <= 0)
		/*
		 * api.txt states that if KVM_CAP_NR_VCPUS does not exist,
		 * assume 4.
		 */
		return 4;

	return ret;
}

int kvm__max_cpus(struct kvm *kvm)
{
	int ret;

	ret = ioctl(kvm->sys_fd, KVM_CHECK_EXTENSION, KVM_CAP_MAX_VCPUS);
	if (ret <= 0)
		ret = kvm__recommended_cpus(kvm);

	return ret;
}

extern pthread_barrier_t barrier_sys_open;
extern pthread_barrier_t barrier_vm_open;
extern pthread_barrier_t barrier_arch_init_done;
extern pthread_barrier_t barrier_mem_register_order;
extern pthread_barrier_t barrier_mem_register_done;
int kvm__init(struct kvm *kvm)
{
	int ret;

#ifdef CONFIG_POPCORN_HYPE
	//if (pop_get_nid()) { return 0; }
	POP_DPRINTF(pop_get_nid(), "\t<%d> %s(): main init\n",
				pop_get_nid(), __func__);
#endif

	if (!kvm__arch_cpu_supports_vm()) { // TODO Jack arch
		pr_err("Your CPU does not support hardware virtualization");
		ret = -ENOSYS;
		goto err;
	}

#ifndef CONFIG_POPCORN_HYPE
	kvm->sys_fd = open(kvm->cfg.dev, O_RDWR);
#else
	int __sys_fd = INT_MIN;
	if (!pop_get_nid())
		kvm->sys_fd = open(kvm->cfg.dev, O_RDWR);
	else
		__sys_fd = open(kvm->cfg.dev, O_RDWR);
    pthread_barrier_wait(&barrier_sys_open);
	if (pop_get_nid()) {
		if (__sys_fd != kvm->sys_fd) {
			POP_SANI_PRINTF(pop_get_nid(),
				"\n\n(BUG) kvm->sys_fd not matching %d %d\n",
									__sys_fd, kvm->sys_fd);
			BUG_ON(1);
		}
		POP_DPRINTF(pop_get_nid(), "\t<%d> %s(): kvm->sys_fd %d\n",
							pop_get_nid(), __func__, kvm->sys_fd);
	}
#endif

	if (kvm->sys_fd < 0) {
		if (errno == ENOENT)
			pr_err("'%s' not found. Please make sure your kernel has CONFIG_KVM "
			       "enabled and that the KVM modules are loaded.", kvm->cfg.dev);
		else if (errno == ENODEV)
			pr_err("'%s' KVM driver not available.\n  # (If the KVM "
			       "module is loaded then 'dmesg' may offer further clues "
			       "about the failure.)", kvm->cfg.dev);
		else
			pr_err("Could not open %s: ", kvm->cfg.dev);

		ret = -errno;
		goto err_free;
	}


	ret = ioctl(kvm->sys_fd, KVM_GET_API_VERSION, 0);
	if (ret != KVM_API_VERSION) {
		pr_err("KVM_API_VERSION ioctl");
		ret = -errno;
		goto err_sys_fd;
	}

#ifndef CONFIG_POPCORN_HYPE
	kvm->vm_fd = ioctl(kvm->sys_fd, KVM_CREATE_VM, KVM_VM_TYPE);
#else
	int __vm_fd = INT_MIN;
	if (!pop_get_nid())
		kvm->vm_fd = ioctl(kvm->sys_fd, KVM_CREATE_VM, KVM_VM_TYPE);
	else
		__vm_fd = ioctl(kvm->sys_fd, KVM_CREATE_VM, KVM_VM_TYPE);
    pthread_barrier_wait(&barrier_vm_open);
	if (pop_get_nid()) {
		if (__vm_fd != kvm->vm_fd) {
			POP_SANI_PRINTF(pop_get_nid(),
				"(BUG) kvm->sys_fd not matching %d %d\n",
										__vm_fd, kvm->vm_fd);
			BUG_ON(1);
		}
		POP_DPRINTF(pop_get_nid(), "\t<%d> %s(): kvm->vm_fd %d\n",
								pop_get_nid(), __func__, kvm->vm_fd);
	}
#endif
	if (kvm->vm_fd < 0) {
		pr_err("KVM_CREATE_VM ioctl");
		ret = kvm->vm_fd;
		goto err_sys_fd;
	}

#ifndef CONFIG_POPCORN_HYPE
	if (kvm__check_extensions(kvm)) {
		pr_err("A required KVM extension is not supported by OS");
		ret = -ENOSYS;
		goto err_vm_fd;
	}
#else
	//if (0) goto err_vm_fd; // for compiler error
	if (!pop_get_nid()) {
		if (kvm__check_extensions(kvm)) {
			pr_err("A required KVM extension is not supported by OS");
			ret = -ENOSYS;
			goto err_vm_fd;
		}
	} else {
		POP_DPRINTF(pop_get_nid(),
			"\t<%d> %s(): skip kvm__check_extensions(kvm)\n",
							pop_get_nid(), __func__, kvm->vm_fd);
	}
#endif

	kvm__arch_init(kvm, kvm->cfg.hugetlbfs_path, kvm->cfg.ram_size); // TODO Jack arch


#ifdef CONFIG_POPCORN_HYPE
#if RUN_REMOTE_MAIN_THREAD // ???
    pthread_barrier_wait(&barrier_arch_init_done); // ???
#endif
	POP_PRINTF("(debug) kvm->cfg.hugetlbfs_path = \"%s\"\n", kvm->cfg.hugetlbfs_path);
//	if (!pop_get_nid()) {
//		;
//	} else {
//		return 0;
//	}
#endif

#ifndef CONFIG_POPCORN_HYPE
	INIT_LIST_HEAD(&kvm->mem_banks);
	kvm__init_ram(kvm);
#else
#if 0 /* BUG() remote cannot */
	if (!pop_get_nid()) {
		printf("BUG: remote kernel has no info\n");
		INIT_LIST_HEAD(&kvm->mem_banks);
		kvm__init_ram(kvm);
	}
#else
	if (!pop_get_nid()) {
		POP_DPRINTF(pop_get_nid(),
			"\t<%d> %s(): register ram\n",
					pop_get_nid(), __func__);
		INIT_LIST_HEAD(&kvm->mem_banks);
		kvm__init_ram(kvm);
    	pthread_barrier_wait(&barrier_mem_register_order);
	} else {
    	pthread_barrier_wait(&barrier_mem_register_order);
		POP_DPRINTF(pop_get_nid(),
			"\t<%d> %s(): register ram\n",
					pop_get_nid(), __func__);
		//INIT_LIST_HEAD(&kvm->mem_banks);
		kvm__init_ram(kvm);
	}
    pthread_barrier_wait(&barrier_mem_register_done);
#endif
#endif

#ifdef CONFIG_POPCORN_HYPE
	if (!pop_get_nid()) {
#endif
	if (!kvm->cfg.firmware_filename) {
		//PRINTF("[*] kvm__load_kernel(kvm, kvm->cfg.kernel_filename, "
		//		"kvm->cfg.initrd_filename, kvm->cfg.real_cmdline))\n");
		if (!kvm__load_kernel(kvm, kvm->cfg.kernel_filename,
				kvm->cfg.initrd_filename, kvm->cfg.real_cmdline))
			die("unable to load kernel %s", kvm->cfg.kernel_filename);
	}

	if (kvm->cfg.firmware_filename) {
		//PRINTF("[] kvm__load_firmware(kvm, kvm->cfg.firmware_filename)\n");
		if (!kvm__load_firmware(kvm, kvm->cfg.firmware_filename))
			die("unable to load firmware image %s: %s", kvm->cfg.firmware_filename, strerror(errno));
	} else {
		//PRINTF("[*] kvm__arch_setup_firmware(kvm) BIOS\n");
		ret = kvm__arch_setup_firmware(kvm);
		/* The inited vma areas will be unmap at a certain point (if remote comes then origin cannot handle the vma fault)*/
		if (ret < 0)
			die("kvm__arch_setup_firmware() failed with error %d\n", ret);
	}
#ifdef CONFIG_POPCORN_HYPE
	}
#endif

#ifdef CONFIG_POPCORN_HYPE
#if RUN_REMOTE_MAIN_THREAD // ???
    pthread_barrier_wait(&barrier_arch_init_done); /* remote waits for list ready and init*/ // ???
#endif
#endif
	return 0;

err_vm_fd:
	close(kvm->vm_fd);
err_sys_fd:
	close(kvm->sys_fd);
err_free:
	free(kvm);
err:
	return ret;
}
core_init(kvm__init);

/* RFC 1952 */
#define GZIP_ID1		0x1f
#define GZIP_ID2		0x8b
#define CPIO_MAGIC		"0707"
/* initrd may be gzipped, or a plain cpio */
static bool initrd_check(int fd)
{
	unsigned char id[4];

	if (read_in_full(fd, id, ARRAY_SIZE(id)) < 0)
		return false;

	if (lseek(fd, 0, SEEK_SET) < 0)
		die_perror("lseek");

	return (id[0] == GZIP_ID1 && id[1] == GZIP_ID2) ||
		!memcmp(id, CPIO_MAGIC, 4);
}

bool kvm__load_kernel(struct kvm *kvm, const char *kernel_filename,
		const char *initrd_filename, const char *kernel_cmdline)
{
	bool ret;
	int fd_kernel = -1, fd_initrd = -1;

	fd_kernel = open(kernel_filename, O_RDONLY);
	if (fd_kernel < 0)
		die("Unable to open kernel %s", kernel_filename);

	if (initrd_filename) {
		fd_initrd = open(initrd_filename, O_RDONLY);
		if (fd_initrd < 0)
			die("Unable to open initrd %s", initrd_filename);

		if (!initrd_check(fd_initrd))
			die("%s is not an initrd", initrd_filename);
	}

	ret = kvm__arch_load_kernel_image(kvm, fd_kernel, fd_initrd,
					  kernel_cmdline);

	if (initrd_filename)
		close(fd_initrd);
	close(fd_kernel);

	if (!ret)
		die("%s is not a valid kernel image", kernel_filename);
	return ret;
}

void kvm__dump_mem(struct kvm *kvm, unsigned long addr, unsigned long size, int debug_fd)
{
	unsigned char *p;
	unsigned long n;

	size &= ~7; /* mod 8 */
	if (!size)
		return;

	p = guest_flat_to_host(kvm, addr);

	for (n = 0; n < size; n += 8) {
		if (!host_ptr_in_ram(kvm, p + n)) {
			dprintf(debug_fd, " 0x%08lx: <unknown>\n", addr + n);
			continue;
		}
		dprintf(debug_fd, " 0x%08lx: %02x %02x %02x %02x  %02x %02x %02x %02x\n",
			addr + n, p[n + 0], p[n + 1], p[n + 2], p[n + 3],
				  p[n + 4], p[n + 5], p[n + 6], p[n + 7]);
	}
}

void kvm__reboot(struct kvm *kvm)
{
	/* Check if the guest is running */
	if (!kvm->cpus[0] || kvm->cpus[0]->thread == 0)
		return;

	pthread_kill(kvm->cpus[0]->thread, SIGKVMEXIT);
}

void kvm__continue(struct kvm *kvm)
{
	POP_PRINTF("\t\t[%d/%d/%d] %s(): $UNLOCK$\n",
			pop_get_nid(), getpid(), popcorn_gettid(), __func__);
	mutex_unlock(&pause_lock);
}

#ifdef CONFIG_POPCORN_HYPE
void kvm__pause_vanilla(struct kvm *kvm)
{
    int i, paused_vcpus = 0;

	POP_PRINTF("<*> [%d/%d] %s(): start\n",
                pop_get_nid(), getpid(), __func__);
    //mutex_lock(&pause_lock);

    /* Check if the guest is running */
    if (!kvm->cpus || !kvm->cpus[0] || kvm->cpus[0]->thread == 0)
        return;

    pause_event = eventfd(0, 0);
    if (pause_event < 0)
        die("Failed creating pause notification event");
    for (i = 0; i < kvm->nrcpus; i++) {
        if (kvm->cpus[i]->is_running && kvm->cpus[i]->paused == 0)
            pthread_kill(kvm->cpus[i]->thread, SIGKVMPAUSEVANILLA);
        else
            paused_vcpus++;
    }

    while (paused_vcpus < kvm->nrcpus) {
        u64 cur_read;

        if (read(pause_event, &cur_read, sizeof(cur_read)) < 0)
            die("Failed reading pause event");
        paused_vcpus += cur_read;
    }
    close(pause_event);

	POP_PRINTF("<*> [%d/%d] %s(): done\n",
                pop_get_nid(), getpid(), __func__);
}
#endif

void kvm__pause(struct kvm *kvm)
{
	int i, paused_vcpus = 0;
#ifdef CONFIG_POPCORN_HYPE
	int _pause_event_hack = INT_MIN;
#endif

	POP_PRINTF("<*> [%d/%d/%d] %s(): $LOCK$ %s\n",
				pop_get_nid(), getpid(), popcorn_gettid(),
				__func__, __FILE__);
	mutex_lock(&pause_lock); // match for kvm_cpu_signal_handler 210212 // but release at KVM_IPC_FT_RESTART

	/* Check if the guest is running */
	if (!kvm->cpus || !kvm->cpus[0] || kvm->cpus[0]->thread == 0)
		return;

	pause_event = eventfd(0, 0);

#ifdef CONFIG_POPCORN_HYPE
	/* do a hack - all in userspace */
	// create fd at remote (repeat) // prepare to handle signals
    for (i = 1; i < kvm->nrcpus; i++) {
        migrate(i, NULL, NULL); // to remote

/******** hACK HACK HACK ***********/
// [1/2942] kvm__pause(): eventfd() _pause_event 23 pause_event fd 24 (?/SYSC_eventfd2)
/******** hACK HACK HACK done ***********/

        int _pause_event = INT_MIN;
        _pause_event_hack = eventfd(0, 0); /* received signal 'KVM_IPC_FT_CKPT' check fd 23 */
        _pause_event = eventfd(0, 0);
		POP_PRINTF("[%d/%d/%d] %s(): [dbg] "
				"_pause_event %d pause_event fd %d %d/%d\n",
				pop_get_nid(), getpid(),
				popcorn_gettid(), __func__,
				_pause_event, pause_event,
				i, kvm->nrcpus);
        if (_pause_event != pause_event) {
			POP_PRINTF("XXXXX WRONG XXXXX [%d/%d/%d] %s(): eventfd() "
				"_pause_event %d pause_event fd %d "
				"(?/SYSC_eventfd2)\n\n",
				pop_get_nid(), getpid(), popcorn_gettid(),
				__func__, _pause_event, pause_event);
            BUG_ON(-1);
        }
        migrate(0, NULL, NULL); // back to host
    }
    //migrate(0, NULL, NULL); // back to host
    // end of remotes


    POP_PRINTF("[%d/%d/%d] %s(): eventfd() pause_event fd %d "
				"(?/SYSC_eventfd2)\n\n",
				pop_get_nid(), getpid(), popcorn_gettid(),
				__func__, pause_event);
#endif

	if (pause_event < 0)
		die("Failed creating pause notification event");

	for (i = 0; i < kvm->nrcpus; i++) {
		if (kvm->cpus[i]->is_running && kvm->cpus[i]->paused == 0) {
#ifdef CONFIG_POPCORN_HYPE
			if (i > 0) { migrate(i, NULL, NULL); }
			POP_PRINTF("pophype: signal distribution "
				"now the target (popcorn process) "
				"is trying to signal [SIGKVMPAUSE %d] vcpu thread<%d> 0x%lx "
				"on differnet nodes (dbg SIGKVMCPUMIGRATE_BACK_W_STATE %d SIGRTMIN %d) "
				"%d/%d at %s %s\n",
				SIGKVMPAUSE, i, kvm->cpus[i]->thread, SIGKVMCPUMIGRATE_BACK_W_STATE,
				SIGRTMIN, i, kvm->nrcpus, i ? "REMOTE" : "ORIGIN", __FILE__);
			pthread_kill(kvm->cpus[i]->thread, SIGKVMPAUSE);
			if (i > 0) { migrate(0, NULL, NULL); }
#else
			pthread_kill(kvm->cpus[i]->thread, SIGKVMPAUSE);
#endif
		} else {
			paused_vcpus++;
		}
	}

	while (paused_vcpus < kvm->nrcpus) {
		u64 cur_read;
#ifdef CONFIG_POPCORN_HYPE
		/* read one by one [0] -> [1] -> [2] -> [3] */
		if (paused_vcpus > 0) {
			migrate(0, NULL, NULL); // maybe from remote to remote
        	migrate(paused_vcpus, NULL, NULL);
		}
		POP_PRINTF("[%d/%d/%d] %s(): wait read(pause_event %d) "
					"at paused_vcpus %d\n",
					pop_get_nid(), getpid(), popcorn_gettid(),
					__func__, pause_event, paused_vcpus);
#endif
		if (read(pause_event, &cur_read, sizeof(cur_read)) < 0)
			die("Failed reading pause event");
#ifdef CONFIG_POPCORN_HYPE
		POP_PRINTF("[%d/%d/%d] %s(): wait read(pause_event %d) "
					"at paused_vcpus # = %d done\n",
					pop_get_nid(), getpid(), popcorn_gettid(),
					__func__, pause_event, paused_vcpus);
#endif
		paused_vcpus += cur_read;
	}

#ifdef CONFIG_POPCORN_HYPE
	/* <*> at remote */
	migrate(0, NULL, NULL);
	/* <*> at origin */
	POP_PRINTF("[%d/%d/%d] %s(): close(pause_event %d) start\n",
			pop_get_nid(), getpid(), popcorn_gettid(), __func__, pause_event);
	close(pause_event);
	POP_PRINTF("[%d/%d/%d] %s(): close(pause_event %d) done\n",
			pop_get_nid(), getpid(), popcorn_gettid(), __func__, pause_event);

	// close fd at remote (repeat)
	for (i = 1; i < kvm->nrcpus; i++) {
		migrate(i, NULL, NULL); // to remote
		POP_PRINTF("[%d/%d/%d] %s(): close(_pause_event_hack %d "
				"pause_event %d) start\n",
				pop_get_nid(), getpid(), popcorn_gettid(),
				__func__, _pause_event_hack, pause_event);
		close(pause_event);
		close(_pause_event_hack);
		POP_PRINTF("[%d/%d/%d] %s(): close(_pause_event_hack %d "
					"pause_event %d) done\n",
					pop_get_nid(), getpid(), popcorn_gettid(),
					__func__, _pause_event_hack, pause_event);
		migrate(0, NULL, NULL); // back to host
	}
	// end of remotes
#else
	close(pause_event);
#endif
}

void kvm__notify_paused(void)
{
	u64 p = 1;

	POP_PRINTF("\t\t%s(): <%lu> [%d/%d/%d] %s(): "
				"write(pause_event %d) start %s ->\n",
				__func__, current_kvm_cpu->cpu_id, pop_get_nid(),
				getpid(), popcorn_gettid(), __func__, pause_event, __FILE__);
	if (write(pause_event, &p, sizeof(p)) < 0)
		die("Failed notifying of paused VCPU.");

	POP_PRINTF("\t\t%s(): <%lu> [%d/%d] "
			"write(pause_event %d) done - 1 "
			"(&current_kvm_cpu->paused %p) ->\n",
			__func__, current_kvm_cpu->cpu_id, pop_get_nid(), getpid(),
			pause_event, &current_kvm_cpu->paused);

	/* The pause_lock should be block till PAUSE is DONE
						(kvm_continue() by ipc_thread) */
#if !defined(CONFIG_POPCORN_HYPE)
	mutex_lock(&pause_lock);
	current_kvm_cpu->paused = 0;
	mutex_unlock(&pause_lock);
#else
	POP_PRINTF("\t\t%s(): <%lu> [%d/%d/%d] waiting for $LOCK$\n",
			   __func__, current_kvm_cpu->cpu_id, pop_get_nid(),
			   getpid(), popcorn_gettid());
    mutex_lock(&pause_lock);
	POP_PRINTF("\t\t%s(): <%lu> [%d/%d/%d] $LOCK$ed "
				"current_kvm_cpu->paused = 0 ->\n",
			   __func__, current_kvm_cpu->cpu_id, pop_get_nid(),
			   getpid(), popcorn_gettid());
	current_kvm_cpu->paused = 0;
	POP_PRINTF("\t\t%s(): <%lu> [%d/%d/%d] going to $UNLOCK$ ->\n",
			   __func__, current_kvm_cpu->cpu_id, pop_get_nid(),
			   getpid(), popcorn_gettid());
	mutex_unlock(&pause_lock);
#endif
	POP_PRINTF("\t\t%s(): <%lu> [%d/%d/%d] write done - 2 ->\n",
				__func__, current_kvm_cpu->cpu_id, pop_get_nid(),
				getpid(), popcorn_gettid());
}

#ifdef CONFIG_POPCORN_HYPE
/* ckpt: 1 = ckpt, 0 = restart */
/*
	vCPU threads all done ->
	kvm__notify_ft_vcpu_migrated (vcpu_thread) and
	kvm__pause_pophype_migrate (ipc_thread) procceed.

	ipc_thread releases lock by kvm__continue.
	vcpu_thread passes

 */
void kvm__notify_ft_vcpu_migrated(bool ckpt)
{
	u64 p = 1;
	int dst_nid = -1;

	POP_PRINTF("\t\t%s(): <%lu> [%d/%d/%d] write start %s\n",
			__func__, current_kvm_cpu->cpu_id, pop_get_nid(),
			getpid(), popcorn_gettid(), __FILE__);
//	if (write(pause_event, &p, sizeof(p)) < 0) {
//		POP_PRINTF("ERROR: <%lu> [%d/%d/%d]\n",
//				__func__, current_kvm_cpu->cpu_id, pop_get_nid(),
//				getpid(), popcorn_gettid());
//		die("Failed notifying of migrated VCPU.");
//	}
	POP_PRINTF("\t\t%s() <%lu> [%d/%d/%d] write done - "
			"(&current_kvm_cpu->paused %p)\n",
			__func__, current_kvm_cpu->cpu_id, pop_get_nid(),
			getpid(), popcorn_gettid(), &current_kvm_cpu->paused);

	if (current_kvm_cpu->cpu_id > 0) {
		if (ckpt) { // CKPT
#if 0 //testing210309local
			/* remote go back and wait origin releases lock */
			pophype_prepare_vcpu_migrate(current_kvm_cpu->cpu_id);
			/* HACK - vcpuid = nid */
			//dst_nid = current_kvm_cpu->cpu_id; // HACK.....this is supposed wrong but since pophype migration will exit after migrating back to remote......
			dst_nid = (MAX_POPCORN_VCPU * 2) + current_kvm_cpu->cpu_id; /* origin to remote */
#endif
#if 0
#if 1 // testing210309local
			POP_PRINTF("\t\t-> [%d/%d/%d] %s(): <%lu> [CKPT] "
						"[pophype migrate] - origin to %d <%lu>\n",
						pop_get_nid(), getpid(), popcorn_gettid(),
						__func__, current_kvm_cpu->cpu_id,
						dst_nid, current_kvm_cpu->cpu_id);
	//////////////////////////////////////////////
			migrate(dst_nid, NULL, NULL);
#endif
#else
	/* Hack in a hack !!!! 210225 */
		/* Hack in a hack !!!! 210225 */
			/* Hack in a hack !!!! 210225 */
				/* Hack in a hack !!!! 210225 */
					/* Hack in a hack !!!! 210225 */
		/* To fix this hack, properly deal with gvcpu_to_nid is needed.
			This is a hack because it doesn't update the
			gvcpu_to_nid table. pophype_migrate doesn but it cause some
			problems (bad back migration) */
#if 0
// testing210310 local
			POP_PRINTF("\t\t-> [%d/%d/%d] %s(): <%lu> [CKPT] "
						"[popcorn migrate] - origin to %d <%lu>\n",
						pop_get_nid(), getpid(), popcorn_gettid(),
						__func__, current_kvm_cpu->cpu_id,
						dst_nid, current_kvm_cpu->cpu_id);
			migrate(current_kvm_cpu->cpu_id, NULL, NULL);
// testing210310 local
#endif
#endif
	//////////////////////////////////////////////
	// testing 210224
//			POP_PRINTF("=> <%lu> [%d/%d/%d] %s(): [CKPT] "
//						"[[[[[I'm back from origin]]]]]\n",
//						current_kvm_cpu->cpu_id, pop_get_nid(), getpid(),
//						popcorn_gettid(), __func__);
		} else { // RESTART
#if 0 // pophype migrate
#if 1
			POP_PRINTF("\t\t-> <%lu> [%d/%d/%d] %s(): [RESTART] "
						"pophype save vcpu states start\n",
						current_kvm_cpu->cpu_id, pop_get_nid(), getpid(),
						popcorn_gettid(), __func__);
			pophype_prepare_vcpu_migrate(current_kvm_cpu->cpu_id);
			POP_PRINTF("\t\t-> <%lu> [%d/%d/%d] %s(): [RESTART] "
						"pophype save vcpu states done\n",
						current_kvm_cpu->cpu_id, pop_get_nid(), getpid(),
						popcorn_gettid(), __func__);
			/* HACK - vcpuid = nid */
			//dst_nid = current_kvm_cpu->cpu_id; // HACK.....this is supposed wrong but since pophype migration will exit after migrating back to remote......
			dst_nid = (MAX_POPCORN_VCPU * 2) + current_kvm_cpu->cpu_id; /* origin to remote */

			POP_PRINTF("\t\t-> <%lu> [%d/%d/%d] %s(): "
						"[RESTART] [pophype migrate] - origin to %d <%lu>\n",
						current_kvm_cpu->cpu_id, pop_get_nid(),
						getpid(), popcorn_gettid(), __func__,
						dst_nid, current_kvm_cpu->cpu_id);
			migrate(dst_nid, NULL, NULL);
#endif
#else // popcorn migrate
#if 1 // testing210309local
// testing210310
//			POP_PRINTF("\t\t-> [%d/%d/%d] %s(): "
//						"<%lu> [RESTART] [popcorn migrate] - origin to %d <%lu>\n",
//						pop_get_nid(), getpid(), popcorn_gettid(),
//						__func__, current_kvm_cpu->cpu_id,
//						dst_nid, current_kvm_cpu->cpu_id);
//			migrate(current_kvm_cpu->cpu_id, NULL, NULL);
#endif
////////////////////////////
////////////////////////////
////////////////////////////
//			POP_PRINTF("\t\t-> [%d/%d/%d] %s(): "
//						"<%lu> [RESTART] [popcorn migration]\n",
//						pop_get_nid(), getpid(), popcorn_gettid(),
//						__func__, current_kvm_cpu->cpu_id);
////////////////////////////
////////////////////////////
////////////////////////////
#endif
// testing210310
//			POP_PRINTF("=> <%lu> [%d/%d/%d] %s(): [RESTART] "
//						"[[[[[I'm back from origin]]]]]\n",
//						current_kvm_cpu->cpu_id, pop_get_nid(), getpid(),
//						popcorn_gettid(), __func__);
			// testing210305 KVM_SET_CPUID2
//			kvm_cpu__setup_cpuid(current_kvm_cpu); // reset

		}
	}

	POP_PRINTF("\t\t-> [%d/%d/%d] %s(): <%lu> GOOD - 1 %s "
				"waiting ipc_thread doing kvm_continue()\n",
				pop_get_nid(), getpid(), popcorn_gettid(),
				__func__, current_kvm_cpu->cpu_id,
				current_kvm_cpu->cpu_id ? "REMOTE" : "ORIGIN");
	mutex_lock(&pause_lock); // blocking here until ipc_thread does kvm_continue()
	current_kvm_cpu->paused = 0;
	mutex_unlock(&pause_lock);
	POP_PRINTF("\t\t-> [%d/%d/%d] %s(): <%lu> GOOD - 2 %s "
				"(This happens after ipc_thread done.)\n",
				pop_get_nid(), getpid(), popcorn_gettid(),
				__func__, current_kvm_cpu->cpu_id,
				current_kvm_cpu->cpu_id ? "REMOTE" : "ORIGIN");
}

/* Migrate w/o states transfer
	op:
		SIGKVMCPUMIGRATE - origin to remotes
		SIGKVMCPUMIGRATE_BACK - remotes to origin
		SIGKVMCPUMIGRATE_BACK_W_STATE - remotes to origin
 */
void kvm__pause_pophype_migrate(struct kvm *kvm, int op)
{
    int i, paused_vcpus = 0;
#if POPHYPE_USR_SANITY_CHECK
	BUG_ON(op != SIGKVMCPUMIGRATE_BACK_W_STATE &&
			op != SIGKVMCPUMIGRATE && op != SIGKVMCPUMIGRATE_BACK &&
			op != SIGKVMCPUMIGRATE_W_STATE &&
			op != SIGKVMFTCKPT && op != SIGKVMFTRESTART);
#endif
	POP_PRINTF("<*ipc> [%d/%d/%d] %s %s():\n"
				"\t\t$LOCK$\n",
				pop_get_nid(), getpid(), popcorn_gettid(),
				__FILE__, __func__);

	mutex_lock(&pause_lock);

	/* Check if the guest is running */
    if (!kvm->cpus || !kvm->cpus[0] || kvm->cpus[0]->thread == 0)
        return;

//	POP_PRINTF("\n\n<*ipc> [%d/%d/%d] %s %s(): "
//				"create ***pause_even*** for sync\n\n\n",
//				pop_get_nid(), getpid(), popcorn_gettid(),
//				__FILE__, __func__);
//    pause_event = eventfd(0, 0);
//    if (pause_event < 0)
//        die("Failed creating pause notification event");

	/* all vcpus are at origin */
    for (i = 0; i < kvm->nrcpus; i++) {
		/* kvm__notify_paused && signum == SIGKVMPAUSE =>
			 current_kvm_cpu->paused = 1;
			 but 02/15 this is wrong... pause should be 1*/
		POP_PRINTF("<*> [%d/%d/%d] %s %s(): "
				"i = <%d> running %d (expect 1) paused %d (expect 0)\n",
				pop_get_nid(), getpid(), popcorn_gettid(), __FILE__,
				__func__, i, kvm->cpus[i]->is_running, kvm->cpus[i]->paused);
        //if (kvm->cpus[i]->is_running && kvm->cpus[i]->paused == 0) {
		/* pophype SIGKVMMIGRATE unlike SIGKVMPAUSE -
								the status is running but pasued
			SIGKVMPAUSE -
					the status is running and not paused */
        //if (kvm->cpus[i]->is_running && kvm->cpus[i]->paused == 1) {
        if (kvm->cpus[i]->is_running && kvm->cpus[i]->paused == 0) {
			POP_PRINTF("<*> [%d/%d/%d] %s %s(): <%d> (migrate and) signal op %d "
						"SIGKVMFTCKPT %d SIGKVMFTRESTART %d "
						"SIGKVMCPUMIGRATE_BACK_W_STATE %d\n",
						pop_get_nid(), getpid(), popcorn_gettid(),
						__FILE__, __func__, i, op, SIGKVMFTCKPT, SIGKVMFTRESTART,
						SIGKVMCPUMIGRATE_BACK_W_STATE);

			if ((op == SIGKVMCPUMIGRATE_BACK_W_STATE
					|| op == SIGKVMCPUMIGRATE_BACK
					|| op == SIGKVMFTCKPT || op == SIGKVMFTRESTART)
					&& i > 0) { migrate(i, NULL, NULL); }

			POP_PRINTF("[%d] pophype: inject [[signal %d]] to "
						"vcpu thread<%d> 0x%lx "
						"on different nodes "
						"(dbg SIGKVMCPUMIGRATE_BACK_W_STATE %d "
						"SIGKVMFTCKPT %d SIGKVMFTRESTART %d "
						"SIGKVMCPUMIGRATE_BACK %d SIGRTMIN %d) "
						"%d/%d at %s %s\n",
						popcorn_gettid(), op, i, kvm->cpus[i]->thread,
						SIGKVMCPUMIGRATE_BACK_W_STATE,
						SIGKVMFTCKPT, SIGKVMFTRESTART,
						SIGKVMCPUMIGRATE_BACK, SIGRTMIN, i, kvm->nrcpus,
						i ? "REMOTE" : "ORIGIN", __FILE__);
            pthread_kill(kvm->cpus[i]->thread, op);
			POP_PRINTF("[%d] <%d> %s(): kill done op %d\n",
						popcorn_gettid(), i, __func__, op);

			if ((op == SIGKVMCPUMIGRATE_BACK_W_STATE
					|| op == SIGKVMCPUMIGRATE_BACK
					|| op == SIGKVMFTCKPT || op == SIGKVMFTRESTART)
					&& i > 0) { migrate(0, NULL, NULL); }

			POP_PRINTF("[%d] <%d> %s(): kill done op %d migrate back\n",
						popcorn_gettid(), i, __func__, op);
        } else {
            paused_vcpus++;
		}
    }

//    while (paused_vcpus < kvm->nrcpus) {
//        u64 cur_read;
//        if (read(pause_event, &cur_read, sizeof(cur_read)) < 0)
//            die("Failed reading pause event");
//
//        paused_vcpus += cur_read;
//	}
//	POP_PRINTF("[%d] <*> %s(): paused_vcpus got %d writes - "
//				"goin to close pause_event\n",
//				popcorn_gettid(), __func__, paused_vcpus);
//	close(pause_event);


	POP_PRINTF("[%d] <*> %s(): paused_vcpus got %d writes - "
				"goin to close pause_event\n",
				popcorn_gettid(), __func__, paused_vcpus);
	for (i = 0; i < kvm->nrcpus; i++) {
		if (op == SIGKVMFTCKPT ||
			op == SIGKVMCPUMIGRATE_BACK_W_STATE) {
			while (!ckpt_done[i]) { sched_yield(); }
		} else if (op == SIGKVMFTRESTART) {
			while (!restart_done[i]) { sched_yield(); }
		}
		//while (!all_done[i]) { sched_yield(); }
	}

//	for (i = 0; i < kvm->nrcpus; i++) {
//		all_done[i] = false;
//	}

}
#endif
