#include "kvm/builtin-run.h"

#include "kvm/builtin-setup.h"
#include "kvm/virtio-balloon.h"
#include "kvm/virtio-console.h"
#include "kvm/parse-options.h"
#include "kvm/8250-serial.h"
#include "kvm/framebuffer.h"
#include "kvm/disk-image.h"
#include "kvm/threadpool.h"
#include "kvm/virtio-scsi.h"
#include "kvm/virtio-blk.h"
#include "kvm/virtio-net.h"
#include "kvm/virtio-rng.h"
#include "kvm/ioeventfd.h"
#include "kvm/virtio-9p.h"
#include "kvm/barrier.h"
#include "kvm/kvm-cpu.h"
#include "kvm/ioport.h"
#include "kvm/symbol.h"
#include "kvm/i8042.h"
#include "kvm/mutex.h"
#include "kvm/term.h"
#include "kvm/util.h"
#include "kvm/strbuf.h"
#include "kvm/vesa.h"
#include "kvm/irq.h"
#include "kvm/kvm.h"
#include "kvm/pci.h"
#include "kvm/rtc.h"
#include "kvm/sdl.h"
#include "kvm/vnc.h"
#include "kvm/guest_compat.h"
#include "kvm/pci-shmem.h"
#include "kvm/kvm-ipc.h"
#include "kvm/builtin-debug.h"

#include <linux/types.h>
#include <linux/err.h>

#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>

#include <popcorn/utils.h>
int gcpus[MAX_POPCORN_VCPU];
int popcorn_nodes;

#define MB_SHIFT		(20)
#define KB_SHIFT		(10)
#define GB_SHIFT		(30)

extern pthread_barrier_t barrier_cmd_init;

__thread struct kvm_cpu *current_kvm_cpu;

static int  kvm_run_wrapper;

bool do_debug_print = false;

static const char * const run_usage[] = {
	"lkvm run [<options>] [<kernel image>]",
	NULL
};

enum {
	KVM_RUN_DEFAULT,
	KVM_RUN_SANDBOX,
};

static int img_name_parser(const struct option *opt, const char *arg, int unset)
{
	char path[PATH_MAX];
	struct stat st;

	snprintf(path, PATH_MAX, "%s%s", kvm__get_dir(), arg);

	if ((stat(arg, &st) == 0 && S_ISDIR(st.st_mode)) ||
	   (stat(path, &st) == 0 && S_ISDIR(st.st_mode)))
		return virtio_9p_img_name_parser(opt, arg, unset);
	return disk_img_name_parser(opt, arg, unset);
}

void kvm_run_set_wrapper_sandbox(void)
{
	kvm_run_wrapper = KVM_RUN_SANDBOX;
}

#ifndef OPT_ARCH_RUN
#define OPT_ARCH_RUN(...)
#endif

/* pophype - args */
#define BUILD_OPTIONS(name, cfg, kvm)					\
	struct option name[] = {					\
	OPT_GROUP("Basic options:"),					\
	OPT_STRING('\0', "name", &(cfg)->guest_name, "guest name",	\
			"A name for the guest"),			\
	OPT_INTEGER('a', "node0", &(cfg)->nodes[0], "Node0 CPUs"),	\
	OPT_INTEGER('b', "node1", &(cfg)->nodes[1], "Node1 CPUs"),	\
	OPT_INTEGER('x', "node2", &(cfg)->nodes[2], "Node2 CPUs"),	\
	OPT_INTEGER('y', "node3", &(cfg)->nodes[3], "Node3 CPUs"),	\
	OPT_INTEGER('z', "nodd4", &(cfg)->nodes[4], "Node4 CPUs"),	\
	OPT_INTEGER('q', "node5", &(cfg)->nodes[5], "Node5 CPUs"),	\
	OPT_INTEGER('r', "node6", &(cfg)->nodes[6], "Node6 CPUs"),	\
	OPT_INTEGER('s', "node7", &(cfg)->nodes[7], "Node7 CPUs"),	\
	OPT_INTEGER('t', "node8", &(cfg)->nodes[8], "Node8 CPUs"),	\
	OPT_INTEGER('w', "nodes", &(cfg)->total_nodes, "# of nodes"),	\
	OPT_INTEGER('c', "cpus", &(cfg)->nrcpus, "Number of CPUs"),	\
	OPT_U64('m', "mem", &(cfg)->ram_size, "Virtual machine memory"	\
		" size in MiB."),					\
	OPT_CALLBACK('\0', "shmem", NULL,				\
		     "[pci:]<addr>:<size>[:handle=<handle>][:create]",	\
		     "Share host shmem with guest via pci device",	\
		     shmem_parser, NULL),				\
	OPT_CALLBACK('d', "disk", kvm, "image or rootfs_dir", "Disk "	\
			" image or rootfs directory", img_name_parser,	\
			kvm),						\
	OPT_BOOLEAN('\0', "balloon", &(cfg)->balloon, "Enable virtio"	\
			" balloon"),					\
	OPT_BOOLEAN('\0', "vnc", &(cfg)->vnc, "Enable VNC framebuffer"),\
	OPT_BOOLEAN('\0', "gtk", &(cfg)->gtk, "Enable GTK framebuffer"),\
	OPT_BOOLEAN('\0', "sdl", &(cfg)->sdl, "Enable SDL framebuffer"),\
	OPT_BOOLEAN('\0', "rng", &(cfg)->virtio_rng, "Enable virtio"	\
			" Random Number Generator"),			\
	OPT_CALLBACK('\0', "9p", NULL, "dir_to_share,tag_name",		\
		     "Enable virtio 9p to share files between host and"	\
		     " guest", virtio_9p_rootdir_parser, kvm),		\
	OPT_STRING('\0', "console", &(cfg)->console, "serial, virtio or"\
			" hv", "Console to use"),			\
	OPT_STRING('\0', "dev", &(cfg)->dev, "device_file",		\
			"KVM device file"),				\
	OPT_CALLBACK('\0', "tty", NULL, "tty id",			\
		     "Remap guest TTY into a pty on the host",		\
		     tty_parser, NULL),					\
	OPT_STRING('\0', "sandbox", &(cfg)->sandbox, "script",		\
			"Run this script when booting into custom"	\
			" rootfs"),					\
	OPT_STRING('\0', "hugetlbfs", &(cfg)->hugetlbfs_path, "path",	\
			"Hugetlbfs path"),				\
									\
	OPT_GROUP("Kernel options:"),					\
	OPT_STRING('k', "kernel", &(cfg)->kernel_filename, "kernel",	\
			"Kernel to boot in virtual machine"),		\
	OPT_STRING('i', "initrd", &(cfg)->initrd_filename, "initrd",	\
			"Initial RAM disk image"),			\
	OPT_STRING('p', "params", &(cfg)->kernel_cmdline, "params",	\
			"Kernel command line arguments"),		\
	OPT_STRING('f', "firmware", &(cfg)->firmware_filename, "firmware",\
			"Firmware image to boot in virtual machine"),	\
									\
	OPT_GROUP("Networking options:"),				\
	OPT_CALLBACK_DEFAULT('n', "network", NULL, "network params",	\
		     "Create a new guest NIC",				\
		     netdev_parser, NULL, kvm),				\
	OPT_BOOLEAN('\0', "no-dhcp", &(cfg)->no_dhcp, "Disable kernel"	\
			" DHCP in rootfs mode"),			\
									\
	OPT_GROUP("VFIO options:"),					\
	OPT_CALLBACK('\0', "vfio-pci", NULL, "[domain:]bus:dev.fn",	\
		     "Assign a PCI device to the virtual machine",	\
		     vfio_device_parser, kvm),				\
									\
	OPT_GROUP("Debug options:"),					\
	OPT_BOOLEAN('\0', "debug", &do_debug_print,			\
			"Enable debug messages"),			\
	OPT_BOOLEAN('\0', "debug-single-step", &(cfg)->single_step,	\
			"Enable single stepping"),			\
	OPT_BOOLEAN('\0', "debug-ioport", &(cfg)->ioport_debug,		\
			"Enable ioport debugging"),			\
	OPT_BOOLEAN('\0', "debug-mmio", &(cfg)->mmio_debug,		\
			"Enable MMIO debugging"),			\
	OPT_INTEGER('\0', "debug-iodelay", &(cfg)->debug_iodelay,	\
			"Delay IO by millisecond"),			\
									\
	OPT_ARCH(RUN, cfg)						\
	OPT_END()							\
	};

// <*> -> <2345...>
static void *kvm_cpu_thread(void *arg)
{
	char name[16];
//	POP_DBG_PRINTF(pop_get_nid(), "\t<%d> %s(): 000\n", pop_get_nid(), __func__);

	current_kvm_cpu = arg;

#ifdef CONFIG_POPCORN_HYPE
	/* func here is serialized */
	int nid = vcpuid_to_nid(current_kvm_cpu->cpu_id);

	POP_DBG_PRINTF(pop_get_nid(),
		"\t[<*>/%d/%d] %s(): thread <%lu> starts on node %d (all threads start on origin)\n",
		getpid(), popcorn_gettid(), __func__, current_kvm_cpu->cpu_id, nid);

	if (nid) {
		migrate(nid, NULL, NULL);
		POP_PRINTF("\t[%d/*%d/%d] %s(): thread<%lu> got migrated to remote. remote start******\n",
				pop_get_nid(), getpid(), popcorn_gettid(), __func__, current_kvm_cpu->cpu_id);

		if (nid != pop_get_nid()) {
			POP_PRINTF("\t\t\tBUG nid %d != pop_get_nid() %d\n",
											nid, pop_get_nid());
		}
		/* BUG: pop_get_nid = 11 somehow.... */

		/* Will migrate back to origin again for doing
				kvm_cpu__reset_vcpu() */
	}
	/*****************
	   distributed - <0>: at origin <123...>: at remote
	 *****************/
#endif

	sprintf(name, "kvm-vcpu-%lu", current_kvm_cpu->cpu_id);

#ifdef CONFIG_POPCORN_HYPE
	POP_PRINTF("\t[%d/*%d/%d] %s(): thread <%lu> \"%s\" set name\n",
			pop_get_nid(), getpid(), popcorn_gettid(),
			__func__, current_kvm_cpu->cpu_id, name);
#endif
	kvm__set_thread_name(name);

#ifdef CONFIG_POPCORN_HYPE
	popcorn_setcpuaffinity(nid); // TODO: THIS IS A HACK. USE MORE STANDARD WAY
#endif

	if (kvm_cpu__start(current_kvm_cpu))
		goto panic_kvm;

	POP_PRINTF("\t[%d/%d/*%d] %s(): vcpu thread <%lu> done "
			"(if remote, migrate back to origin)\n",
			pop_get_nid(), getpid(), popcorn_gettid(),
			__func__, current_kvm_cpu->cpu_id);

	if (pop_get_nid())
		migrate(0, NULL, NULL);

	/* ==============
		Serialized
	   ============== */
	POP_PRINTF("\t[%d/*%d] <!> %s(): vcpu thread <%lu> good done\n",
				getpid(), popcorn_gettid(), __func__, current_kvm_cpu->cpu_id);
	return (void *) (intptr_t) 0;

panic_kvm:
#ifndef CONFIG_POPCORN_HYPE
	fprintf(stderr, "\tKVM exit reason: %u (\"%s\")\n",
		current_kvm_cpu->kvm_run->exit_reason,
		kvm_exit_reasons[current_kvm_cpu->kvm_run->exit_reason]);
#else
	{
		int nid = pop_get_nid();
		if (pop_get_nid())
			migrate(0, NULL, NULL);

		POP_PRINTF("\t[%d] <%lu> KVM exit reason: %u (\"%s\")"
				"(not true vcpu is !distributed shared now)\n",
			nid, current_kvm_cpu->cpu_id, current_kvm_cpu->kvm_run->exit_reason,
						kvm_exit_reasons[current_kvm_cpu->kvm_run->exit_reason]);
//		POP_DPRINTF(pop_get_nid(), "\t<%d> KVM exit reason: %u (\"%s\")\n",
//						pop_get_nid(), current_kvm_cpu->kvm_run->exit_reason,
//					kvm_exit_reasons[current_kvm_cpu->kvm_run->exit_reason]);
	}
#endif
	if (current_kvm_cpu->kvm_run->exit_reason == KVM_EXIT_UNKNOWN)
		fprintf(stderr, "\tKVM exit code: 0x%llu\n",
			(unsigned long long)current_kvm_cpu->kvm_run->hw.hardware_exit_reason);

	kvm_cpu__set_debug_fd(STDOUT_FILENO);
	kvm_cpu__show_registers(current_kvm_cpu);
	kvm_cpu__show_code(current_kvm_cpu);
	kvm_cpu__show_page_tables(current_kvm_cpu);

	return (void *) (intptr_t) 1;
}

static char kernel[PATH_MAX];

static const char *host_kernels[] = {
	"/boot/vmlinuz",
	"/boot/bzImage",
	NULL
};

static const char *default_kernels[] = {
	"./bzImage",
	"arch/" BUILD_ARCH "/boot/bzImage",
	"../../arch/" BUILD_ARCH "/boot/bzImage",
	NULL
};

static const char *default_vmlinux[] = {
	"vmlinux",
	"../../../vmlinux",
	"../../vmlinux",
	NULL
};

static void kernel_usage_with_options(void)
{
	const char **k;
	struct utsname uts;

	fprintf(stderr, "Fatal: could not find default kernel image in:\n");
	k = &default_kernels[0];
	while (*k) {
		fprintf(stderr, "\t%s\n", *k);
		k++;
	}

	if (uname(&uts) < 0)
		return;

	k = &host_kernels[0];
	while (*k) {
		if (snprintf(kernel, PATH_MAX, "%s-%s", *k, uts.release) < 0)
			return;
		fprintf(stderr, "\t%s\n", kernel);
		k++;
	}
	fprintf(stderr, "\nPlease see '%s run --help' for more options.\n\n",
		KVM_BINARY_NAME);
}

static u64 host_ram_size(void)
{
	long page_size;
	long nr_pages;

	nr_pages	= sysconf(_SC_PHYS_PAGES);
	if (nr_pages < 0) {
		pr_warning("sysconf(_SC_PHYS_PAGES) failed");
		return 0;
	}

	page_size	= sysconf(_SC_PAGE_SIZE);
	if (page_size < 0) {
		pr_warning("sysconf(_SC_PAGE_SIZE) failed");
		return 0;
	}

	return (nr_pages * page_size) >> MB_SHIFT;
}

/*
 * If user didn't specify how much memory it wants to allocate for the guest,
 * avoid filling the whole host RAM.
 */
#define RAM_SIZE_RATIO		0.8

static u64 get_ram_size(int nr_cpus)
{
	u64 available;
	u64 ram_size;

	ram_size	= 64 * (nr_cpus + 3);

	available	= host_ram_size() * RAM_SIZE_RATIO;
	if (!available)
		available = MIN_RAM_SIZE_MB;

	if (ram_size > available)
		ram_size	= available;

	return ram_size;
}

static const char *find_kernel(void)
{
	const char **k;
	struct stat st;
	struct utsname uts;

	k = &default_kernels[0];
	while (*k) {
		if (stat(*k, &st) < 0 || !S_ISREG(st.st_mode)) {
			k++;
			continue;
		}
		strlcpy(kernel, *k, PATH_MAX);
		return kernel;
	}

	if (uname(&uts) < 0)
		return NULL;

	k = &host_kernels[0];
	while (*k) {
		if (snprintf(kernel, PATH_MAX, "%s-%s", *k, uts.release) < 0)
			return NULL;

		if (stat(kernel, &st) < 0 || !S_ISREG(st.st_mode)) {
			k++;
			continue;
		}
		return kernel;

	}
	return NULL;
}

static const char *find_vmlinux(void)
{
	const char **vmlinux;

	vmlinux = &default_vmlinux[0];
	while (*vmlinux) {
		struct stat st;

		if (stat(*vmlinux, &st) < 0 || !S_ISREG(st.st_mode)) {
			vmlinux++;
			continue;
		}
		return *vmlinux;
	}
	return NULL;
}

void kvm_run_help(void)
{
	struct kvm *kvm = NULL;

	BUILD_OPTIONS(options, &kvm->cfg, kvm);
	usage_with_options(run_usage, options);
}

static int kvm_run_set_sandbox(struct kvm *kvm)
{
	const char *guestfs_name = kvm->cfg.custom_rootfs_name;
	char path[PATH_MAX], script[PATH_MAX], *tmp;

	snprintf(path, PATH_MAX, "%s%s/virt/sandbox.sh", kvm__get_dir(), guestfs_name);

	remove(path);

	if (kvm->cfg.sandbox == NULL)
		return 0;

	tmp = realpath(kvm->cfg.sandbox, NULL);
	if (tmp == NULL)
		return -ENOMEM;

	snprintf(script, PATH_MAX, "/host/%s", tmp);
	free(tmp);

	return symlink(script, path);
}

static void kvm_write_sandbox_cmd_exactly(int fd, const char *arg)
{
	const char *single_quote;

	if (!*arg) { /* zero length string */
		if (write(fd, "''", 2) <= 0)
			die("Failed writing sandbox script");
		return;
	}

	while (*arg) {
		single_quote = strchrnul(arg, '\'');

		/* write non-single-quote string as #('string') */
		if (arg != single_quote) {
			if (write(fd, "'", 1) <= 0 ||
			    write(fd, arg, single_quote - arg) <= 0 ||
			    write(fd, "'", 1) <= 0)
				die("Failed writing sandbox script");
		}

		/* write single quote as #("'") */
		if (*single_quote) {
			if (write(fd, "\"'\"", 3) <= 0)
				die("Failed writing sandbox script");
		} else
			break;

		arg = single_quote + 1;
	}
}

static void resolve_program(const char *src, char *dst, size_t len)
{
	struct stat st;
	int err;

	err = stat(src, &st);

	if (!err && S_ISREG(st.st_mode)) {
		char resolved_path[PATH_MAX];

		if (!realpath(src, resolved_path))
			die("Unable to resolve program %s: %s\n", src, strerror(errno));

		if (snprintf(dst, len, "/host%s", resolved_path) >= (int)len)
			die("Pathname too long: %s -> %s\n", src, resolved_path);

	} else
		strlcpy(dst, src, len);
}

static void kvm_run_write_sandbox_cmd(struct kvm *kvm, const char **argv, int argc)
{
	const char script_hdr[] = "#! /bin/bash\n\n";
	char program[PATH_MAX];
	int fd;

	remove(kvm->cfg.sandbox);

	fd = open(kvm->cfg.sandbox, O_RDWR | O_CREAT, 0777);
	if (fd < 0)
		die("Failed creating sandbox script");

	if (write(fd, script_hdr, sizeof(script_hdr) - 1) <= 0)
		die("Failed writing sandbox script");

	resolve_program(argv[0], program, PATH_MAX);
	kvm_write_sandbox_cmd_exactly(fd, program);

	argv++;
	argc--;

	while (argc) {
		if (write(fd, " ", 1) <= 0)
			die("Failed writing sandbox script");

		kvm_write_sandbox_cmd_exactly(fd, argv[0]);
		argv++;
		argc--;
	}
	if (write(fd, "\n", 1) <= 0)
		die("Failed writing sandbox script");

	close(fd);
}

//extern int __init_thread_params(void);
static struct kvm *kvm_cmd_run_init(int argc, const char **argv)
{
	static char real_cmdline[2048], default_name[20];
	//unsigned int nr_online_cpus;
	struct kvm *kvm = kvm__new();
	bool video;
	int i, j;

	if (IS_ERR(kvm))
		return kvm;

    POP_PRINTF("===========================\n");
    POP_PRINTF("%s(): pophype - init\n", __func__);
    POP_PRINTF("===========================\n");
    POP_PRINTF("init - vcpu info on each node\n");
    for (i = 0; i < MAX_POPCORN_NODES; i++) {
		kvm->cfg.nodes[i] = 0;
    }

	POP_DPRINTF(pop_get_nid(), "<%d> %s(): kvm %p argc %d argv %p\n",
								pop_get_nid(), __func__, kvm, argc, argv);
	POP_DPRINTF(pop_get_nid(), "<%d> %s(): argv[0] %p argv[1] %p\n",
								pop_get_nid(), __func__, argv[0], argv[1]);

	//nr_online_cpus = sysconf(_SC_NPROCESSORS_ONLN);
	kvm->cfg.custom_rootfs_name = "default";

	while (argc != 0) {
		/* construct HELP "./lkvm help run" */
		BUILD_OPTIONS(options, &kvm->cfg, kvm);
		argc = parse_options(argc, argv, options, run_usage,
				PARSE_OPT_STOP_AT_NON_OPTION |
				PARSE_OPT_KEEP_DASHDASH);
		/* ptr* got changed so don't invoke this function again on other node or cpu  */

		if (argc != 0) {
			/* Custom options, should have been handled elsewhere */
			if (strcmp(argv[0], "--") == 0) {
				if (kvm_run_wrapper == KVM_RUN_SANDBOX) {
					kvm->cfg.sandbox = DEFAULT_SANDBOX_FILENAME;
					kvm_run_write_sandbox_cmd(kvm, argv+1, argc-1);
					break;
				}
			}

			if ((kvm_run_wrapper == KVM_RUN_DEFAULT && kvm->cfg.kernel_filename) ||
				(kvm_run_wrapper == KVM_RUN_SANDBOX && kvm->cfg.sandbox)) {
				fprintf(stderr, "Cannot handle parameter: "
						"%s\n", argv[0]);
				usage_with_options(run_usage, options);
				free(kvm);
				return ERR_PTR(-EINVAL);
			}
			if (kvm_run_wrapper == KVM_RUN_SANDBOX) {
				/*
				 * first unhandled parameter is treated as
				 * sandbox command
				 */
				kvm->cfg.sandbox = DEFAULT_SANDBOX_FILENAME;
				kvm_run_write_sandbox_cmd(kvm, argv, argc);
			} else {
				/*
				 * first unhandled parameter is treated as a kernel
				 * image
				 */
				kvm->cfg.kernel_filename = argv[0];
			}
			argv++;
			argc--;
		}

	}

	kvm->nr_disks = kvm->cfg.image_count;

	if (!kvm->cfg.kernel_filename && !kvm->cfg.firmware_filename) {
		kvm->cfg.kernel_filename = find_kernel();

		if (!kvm->cfg.kernel_filename) {
			kernel_usage_with_options();
			return ERR_PTR(-EINVAL);
		}
	}

	kvm->cfg.vmlinux_filename = find_vmlinux();
	kvm->vmlinux = kvm->cfg.vmlinux_filename;

#ifdef CONFIG_POPCORN_HYPE
	// TODO333
    /* Popcorn hype before load the arg fron cfg manipulate it.
        We need nothing except cpu_on_node info */
	//kvm->nrcpus = kvm->cfg.nodes[0] + kvm->cfg.nodes[1];
	//kvm->cfg.nrcpus = kvm->nrcpus = kvm->cfg.nodes[0] + kvm->cfg.nodes[1];

	kvm->cfg.nrcpus = kvm->nrcpus = kvm->cfg.nodes[0] + kvm->cfg.nodes[1] + kvm->cfg.nodes[2] + kvm->cfg.nodes[3];

	//kvm->cfg.nrcpus = kvm->cfg.nodes[0] + kvm->cfg.nodes[1] + kvm->cfg.nodes[2];
    //kvm->cfg.nrcpus = kvm->cfg.nodes[0] + kvm->cfg.nodes[1] + kvm->cfg.nodes[2] + kvm->cfg.nodes[3];
    //vm->cfg.nrcpus = kvm->cfg.nodes[0] + kvm->cfg.nodes[1];
	nthreads = kvm->cfg.nrcpus;
	nodes = kvm->cfg.total_nodes;
    POP_ARG_PRINTF("\n**********************<*>**********************\n");
    POP_ARG_PRINTF("- Popcorn interested argvs & syncing\n");
    //POP_ARG_PRINTF("\tnodes %d\n", nodes);
    POP_ARG_PRINTF("\tkvm->cfg.total_nodes () %d\n", kvm->cfg.total_nodes);
    POP_ARG_PRINTF("<*> %s(): nthreads %d =  kvm->nrcpus %d = nodes[0] %d + nodes[1] %d "
					//"\n", __func__, nthreads, kvm->cfg.nrcpus,
					//kvm->cfg.nodes[0], kvm->cfg.nodes[1]);

					//"+ nodes[2] %d\n", __func__, nthreads, kvm->cfg.nrcpus,
					//kvm->cfg.nodes[0], kvm->cfg.nodes[1], kvm->cfg.nodes[2]);

					"+ nodes[2] %d + nodes[3] %d\n", __func__, nthreads, kvm->cfg.nrcpus,
					kvm->cfg.nodes[0], kvm->cfg.nodes[1], kvm->cfg.nodes[2], kvm->cfg.nodes[3]);
    POP_ARG_PRINTF("\tnext non-vcpu fd is %d\n", VCPU_FD_BASE + nthreads);
	// TODO DON'T hardcode
	/* TODO for max = online nodes (kern:get_popcorn_nodes) */
	//BUG_ON(!kvm->cfg.nodes[0]);
	//BUG_ON(!kvm->cfg.nodes[0] || !kvm->cfg.nodes[1]);
	//BUG_ON(!kvm->cfg.nodes[0] || !kvm->cfg.nodes[1] || !kvm->cfg.nodes[2]);

	// TODO make init 0 (i should support it eventually)
	//BUG_ON(!kvm->cfg.nodes[0] || !kvm->cfg.nodes[1] || !kvm->cfg.nodes[2] || !kvm->cfg.nodes[3]);




	/* Calculate nthreads by checking -a -b -x -y */
	int cal_nodes = 0;
	int vcpu_id = 0;
	int node_id = 0;
	nthreads = 0;
	for (i = 0; i < MAX_POPCORN_VCPU; i++) {
		gcpus[i] = -1;
	}
	for (i = 0; i < MAX_POPCORN_NODES; i++) { /* This will trigger pophype migration code ....*/ /* testing nid 4~max-1 now */
	//for (i = 0; i < pop_msg_nodes; i++) {
		if (kvm->cfg.nodes[i] > 0) {
			cal_nodes++; /* # of nodes */
			nthreads += kvm->cfg.nodes[i]; /* # of threads */
			/* convert cfg.nodes[] to vcpu_to_nid[] */
			for (j = 1; j <= kvm->cfg.nodes[i]; j++) { /* e.g. */
				POP_PRINTF("vcpu <%d> is on node [%d]\n", vcpu_id, node_id);
				gcpus[vcpu_id] = node_id;
				vcpu_id++;
			}
			node_id++;
		}

		/* create Popcorn remote meta data */
		POP_PRINTF("<*> %s(): make this process distributed to [%d]\n", __func__, i);
		migrate(i, NULL, NULL);
		migrate(0, NULL, NULL);
	}
    POP_ARG_PRINTF("\t<*> cal_nodes = %d, node_id = %d, vcpu_id = %d\n",
										cal_nodes, node_id, vcpu_id);

	/* <*>: broadcast vcpu_to_nid[] */
	//popcorn_broadcast_cpu_table(kvm->cfg.nodes); /* This is a table.  <*> cal_nodes[] = cpus[nid].
	POP_PRINTF("<*> %s(): call "
			"popcorn_broadcast_cpu_table(&gcpus[])\n", __func__);
	popcorn_broadcast_cpu_table(gcpus);
	/* This is a table.  <*> cal_nodes[] = cpus[nid].
	Make sure you have migrated before (remote metadata required) */

    POP_ARG_PRINTF("\tAuto calculated nodes = %d\n", cal_nodes);
    POP_ARG_PRINTF("\tUser specify nodes is not for nodes but vcpus = %d\n", nodes);
	nodes = cal_nodes;
    POP_ARG_PRINTF("\tnodes overwritten = %d\n", nodes);
    POP_ARG_PRINTF("\tUser specify nrcpuss = %d\n", kvm->cfg.nrcpus);
	kvm->cfg.nrcpus = nthreads;
	POP_ARG_PRINTF("- nrthreads/nrcpus OVERWRITTEN = %d\n", kvm->cfg.nrcpus);

	POP_ARG_PRINTF("- Popcorn broadcasting cpu&node info\n");
    POP_ARG_PRINTF("**********************<*>**********************\n\n");
	BUG_ON(!kvm->cfg.nrcpus);
	BUG_ON(kvm->cfg.nrcpus < cal_nodes);
	//BUG_ON(cal_nodes != kvm->cfg.total_nodes);
#endif

	//if (kvm->cfg.nrcpus == 0)
	//	kvm->cfg.nrcpus = nr_online_cpus; /* default use all host cpu */

	if (!kvm->cfg.ram_size)
		kvm->cfg.ram_size = get_ram_size(kvm->cfg.nrcpus * 2); /* default ram size */

	if (kvm->cfg.ram_size > host_ram_size())
		pr_warning("Guest memory size %lluMB exceeds host physical RAM size %lluMB",
			(unsigned long long)kvm->cfg.ram_size,
			(unsigned long long)host_ram_size());

	POP_CPU_PRINTF(pop_get_nid(), "<%d> %s(): kvm->cfg.nrcpus %d "
								"kvm->cfg.ram_size %lu\n",
								pop_get_nid(), __func__,
								kvm->cfg.nrcpus, kvm->cfg.ram_size);
	kvm->cfg.ram_size <<= MB_SHIFT;

	if (!kvm->cfg.dev)
		kvm->cfg.dev = DEFAULT_KVM_DEV;

	if (!kvm->cfg.console)
		kvm->cfg.console = DEFAULT_CONSOLE;

	video = kvm->cfg.vnc || kvm->cfg.sdl || kvm->cfg.gtk;

	if (!strncmp(kvm->cfg.console, "virtio", 6))
		kvm->cfg.active_console  = CONSOLE_VIRTIO;
	else if (!strncmp(kvm->cfg.console, "serial", 6))
		kvm->cfg.active_console  = CONSOLE_8250;
	else if (!strncmp(kvm->cfg.console, "hv", 2))
		kvm->cfg.active_console = CONSOLE_HV;
	else
		pr_warning("No console!");

	if (!kvm->cfg.host_ip)
		kvm->cfg.host_ip = DEFAULT_HOST_ADDR;

	if (!kvm->cfg.guest_ip)
		kvm->cfg.guest_ip = DEFAULT_GUEST_ADDR;

	if (!kvm->cfg.guest_mac)
		kvm->cfg.guest_mac = DEFAULT_GUEST_MAC;

	if (!kvm->cfg.host_mac)
		kvm->cfg.host_mac = DEFAULT_HOST_MAC;

	if (!kvm->cfg.script)
		kvm->cfg.script = DEFAULT_SCRIPT;

	if (!kvm->cfg.network)
		kvm->cfg.network = DEFAULT_NETWORK;

	memset(real_cmdline, 0, sizeof(real_cmdline));
	kvm__arch_set_cmdline(real_cmdline, video); // TODO Jack arch

	if (video) {
		strcat(real_cmdline, " console=tty0");
	} else {
		switch (kvm->cfg.active_console) {
		case CONSOLE_HV:
			/* Fallthrough */
		case CONSOLE_VIRTIO:
			strcat(real_cmdline, " console=hvc0");
			break;
		case CONSOLE_8250:
			strcat(real_cmdline, " console=ttyS0");
			break;
		}
	}

	if (!kvm->cfg.guest_name) {
		if (kvm->cfg.custom_rootfs) {
			kvm->cfg.guest_name = kvm->cfg.custom_rootfs_name;
		} else {
			sprintf(default_name, "guest-%u", getpid());
			kvm->cfg.guest_name = default_name;
		}
	}

	if (!kvm->cfg.using_rootfs && !kvm->cfg.disk_image[0].filename && !kvm->cfg.initrd_filename) {
		char tmp[PATH_MAX];

		kvm_setup_create_new(kvm->cfg.custom_rootfs_name);
		kvm_setup_resolv(kvm->cfg.custom_rootfs_name);

		snprintf(tmp, PATH_MAX, "%s%s", kvm__get_dir(), "default");
		if (virtio_9p__register(kvm, tmp, "/dev/root") < 0)
			die("Unable to initialize virtio 9p");
		if (virtio_9p__register(kvm, "/", "hostfs") < 0)
			die("Unable to initialize virtio 9p");
		kvm->cfg.using_rootfs = kvm->cfg.custom_rootfs = 1;
	}

	if (kvm->cfg.using_rootfs) {
		strcat(real_cmdline, " rw rootflags=trans=virtio,version=9p2000.L,cache=loose rootfstype=9p");
		if (kvm->cfg.custom_rootfs) {
			kvm_run_set_sandbox(kvm);

#ifdef CONFIG_GUEST_PRE_INIT
			strcat(real_cmdline, " init=/virt/pre_init");
#else
			strcat(real_cmdline, " init=/virt/init");
#endif

			if (!kvm->cfg.no_dhcp)
				strcat(real_cmdline, "  ip=dhcp");
			if (kvm_setup_guest_init(kvm->cfg.custom_rootfs_name))
				die("Failed to setup init for guest.");
		}
	} else if (!kvm->cfg.kernel_cmdline || !strstr(kvm->cfg.kernel_cmdline, "root=")) {
		strlcat(real_cmdline, " root=/dev/vda rw ", sizeof(real_cmdline));
	}

	if (kvm->cfg.kernel_cmdline) {
		strcat(real_cmdline, " ");
		strlcat(real_cmdline, kvm->cfg.kernel_cmdline, sizeof(real_cmdline));
	}

	kvm->cfg.real_cmdline = real_cmdline; /* TODO Jack arch */

	if (kvm->cfg.kernel_filename) {
		//POP_PRINTF("  # %s run -k %s -m %Lu -c %d --name %s\n", KVM_BINARY_NAME,
		printf("  # %s run -k %s -m %Lu -c %d --name %s\n\n", KVM_BINARY_NAME,
		       kvm->cfg.kernel_filename,
		       (unsigned long long)kvm->cfg.ram_size / 1024 / 1024,
		       kvm->cfg.nrcpus, kvm->cfg.guest_name);
	} else if (kvm->cfg.firmware_filename) {
		//POP_PRINTF("  # %s run --firmware %s -m %Lu -c %d --name %s\n", KVM_BINARY_NAME,
		printf("  # %s run --firmware %s -m %Lu -c %d --name %s\n\n", KVM_BINARY_NAME,
		       kvm->cfg.firmware_filename,
		       (unsigned long long)kvm->cfg.ram_size / 1024 / 1024,
		       kvm->cfg.nrcpus, kvm->cfg.guest_name);
	}

	POP_PRINTF("%s(): real_cmdline: %s\n\n", __func__,  real_cmdline);

//	__init_thread_params();

//  moved to outside
//	if (init_list__init(kvm) < 0)
//		die ("Initialisation failed");

	return kvm;
}

// <*>
static int kvm_cmd_run_work(struct kvm *kvm)
{
	int i;

	POP_PRINTF("\n==============<*>===============\n\n");
	POP_DBG_PRINTF(pop_get_nid(),
			"\t[<*>/%d] %s(): kvm->nrcpus %d total threads (TODO seperate) "
			"going to run kvm_run()\n", popcorn_gettid(), __func__, kvm->nrcpus);

	// TODO remote version kvm remote ID
	for (i = 0; i < kvm->nrcpus; i++) {
		kvm->cpus[i]->cpu_id = i;
		POP_PRINTF("\t\tDEBUG: JACK UNINIT DATA? %lu\n", kvm->cpus[i]->cpu_id);
		POP_DBG_PRINTF(pop_get_nid(),
			"\t[<*>/%d] %s(): vcpu thread[%d] arg %p vcpuid %lu\n",
			popcorn_gettid(), __func__, i,
			kvm->cpus[i], kvm->cpus[i]->cpu_id);

		if (pthread_create(&kvm->cpus[i]->thread, NULL,
							kvm_cpu_thread, kvm->cpus[i]) != 0) {
			die("unable to create KVM VCPU thread");
		}
	}

#ifndef CONFIG_POPCORN_HYPE
	/* Only VCPU #0 is going to exit by itself when shutting down */
	if (pthread_join(kvm->cpus[0]->thread, NULL) != 0)
		die("unable to join with vcpu 0");
#else
	///* since some pthreads are on the remote nodes, wait until they are back (barrier will not work, use pjoin()) */
	POP_PRINTF("\t<*> %s(): forget about remote threads now\n", __func__);
	POP_PRINTF("\t<*> %s(): dbg kvm->nrcpus %d\n", __func__, kvm->nrcpus);
	//for (i = kvm->nrcpus - 1; i >= 0; i--) { // exit path - TODO why?
	for (i = 0; i == 0; i++) { // exit path - TODO why?
		POP_PRINTF("\t<*> %s(): pjoint vcpu <%d>\n", __func__, i);
		if (pthread_join(kvm->cpus[i]->thread, NULL) != 0)
			die("unable to join with vcpu %d", i);
	}
#endif

	POP_PRINTF("\n\n\n===============\n"
			"\t<*> %s(): vcpu thread[] do vcpu exit\n"
			"=======================================\n\n\n", __func__);

	return kvm_cpu__exit(kvm);
}

static void kvm_cmd_run_exit(struct kvm *kvm, int guest_ret)
{
#if RUN_GUEST_KERNEL
	compat__print_all_messages();
#endif

	init_list__exit(kvm);

	if (guest_ret == 0)
		POP_PRINTF("\n  # KVM session ended normally.\n");
}

struct kvm *g_kvm = NULL; /* kvm on heap synced by dsm */
extern pthread_barrier_t barrier_dbg_last_sync;
int kvm_cmd_run(int argc, const char **argv, const char *prefix)
{
	int ret = -EFAULT;
	POP_PRINTF("[%d/%d] %s():\n",
			pop_get_nid(), popcorn_gettid(), __func__);
	if (!pop_get_nid())
		g_kvm = kvm_cmd_run_init(argc, argv);

	//POP_PRINTF("<*> [%d/%d] %s(): [WRONG] "
	//		"after I support argv, remote threads no longer run to here\n",
	//							pop_get_nid(), popcorn_gettid(), __func__);
	/* remotes wait for kvm */
#if RUN_REMOTE_MAIN_THREAD
	pthread_barrier_wait(&barrier_cmd_init);
#endif
	POP_DPRINTF(pop_get_nid(), "[%d/%d] %s(): returned g_kvm %p\n",
					pop_get_nid(), popcorn_gettid(), __func__, g_kvm);

	printf("[%d/%d] %s(): dbg 2\n",
			pop_get_nid(), popcorn_gettid(), __func__);

	/* REAL MAIN FOR ALL TREADS */
	/* <all> main init classes - for a serial of *_init() */
	if (init_list__init(g_kvm) < 0)
		die ("Initialisation failed");

	printf("[%d/%d] %s(): dbg 3\n",
			pop_get_nid(), popcorn_gettid(), __func__);

#if RUN_REMOTE_MAIN_THREAD
	POP_DPRINTF(pop_get_nid(), "======================\n"
				"<%d> [%d] %s(): all class init done going to kvm_run\n"
				"=====================\n",
				pop_get_nid(), popcorn_gettid(), __func__);

	pthread_barrier_wait(&barrier_dbg_last_sync);
#endif

	printf("[%d/%d] %s(): dbg 4\n",
			pop_get_nid(), popcorn_gettid(), __func__);

	if (IS_ERR(g_kvm))
		return PTR_ERR(g_kvm);

	/* Both */
	POP_PRINTF("<%d> [%d] both %s(): kvm_cmd_run_work():\n",
				pop_get_nid(), popcorn_gettid(), __func__);

	/** <*>: serialized at origin  **/
	if (!pop_get_nid()) {
		ret = kvm_cmd_run_work(g_kvm); // sync inside
	} else {
		POP_PRINTF("[%d/%d/%d] I'm an remote worker thread and "
					"will cause futex 120s blocking\n",
					pop_get_nid(), getpid(), popcorn_gettid());
	}
	/** DONE **/

	POP_PRINTF("[%d/%d/%d] %s(): "
				"remote main thread waits to finish lkvm together\n",
				pop_get_nid(), getpid(), popcorn_gettid(), __func__);
	/* reote thread skipped kvm_cmd_run_work() and
					waits for origin at tihs barrier */
	pthread_barrier_wait(&barrier_run_destory); //  single noe


	/***************
		Exit path
	****************/
	if (pop_get_nid()) {
		POP_DPRINTF(pop_get_nid(),
				"<%d> %s(): should remote redo kvm_cpu__exit() again?\n",
												pop_get_nid(), __func__);
//		kvm_cpu__exit(g_kvm); // from kvm_cmd_run_work() // TODO....more sync.
		POP_DPRINTF(pop_get_nid(),
				"<%d> %s(): remote safe waiting for destorying\n",
											pop_get_nid(), __func__);
		sleep(5); //TODO list_kill sync
	}
	if (!pop_get_nid())
		kvm_cmd_run_exit(g_kvm, ret);

	//TODO list_kill sync
	POP_DPRINTF(pop_get_nid(), "<%d> %s(): kvm_cmd_run(): done\n",
										pop_get_nid(), __func__);

	return ret;
}
