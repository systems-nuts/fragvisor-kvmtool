#include "kvm/kvm-cpu.h"

#include "kvm/symbol.h"
#include "kvm/util.h"
#include "kvm/kvm.h"
#include "kvm/virtio.h"
#include "kvm/mutex.h"
#include "kvm/barrier.h"

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/eventfd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#include <popcorn/utils.h>
#include <popcorn/debug.h>
extern pthread_barrier_t barrier_atomic_op_test_start;
#define REMOTE_NEVER_BOOT_BSP_ID 999

extern __thread struct kvm_cpu *current_kvm_cpu;



#if defined(CONFIG_POPCORN_HYPE)
volatile bool all_done[MAX_POPCORN_VCPU]; // ckpt done sync
//bool ckpt_done[MAX_POPCORN_VCPU]; // ckpt done sync
volatile bool ckpt_done[MAX_POPCORN_VCPU]; // ckpt done sync
volatile bool restart_done[MAX_POPCORN_VCPU]; // ckpt done sync
// remote - vcpu states transffered
// orogin - ckpt done
volatile bool ipc_thread_ckpt_done;

extern void *first_mem;
extern void *second_mem;
extern u64 first_mem_size;
extern u64 second_mem_size;
#define MAX_FNAME   256
#define MAX_MSR_ENTRIES 25

#define kvm_ioctl(fd, cmd, arg) ({ \
    const int ret = ioctl(fd, cmd, arg); \
    if(ret == -1) \
        printf("KVM: ioctl " #cmd " failed\n"); \
    ret; \
    })

/* from ./arch/x86/include/asm/msr-index.h 4.4.137
    CPU model specific register (MSR) numbers
    x86-64 specific MSRs */
#define MSR_EFER        0xc0000080 /* extended feature register */
#define MSR_STAR        0xc0000081 /* legacy mode SYSCALL target */
#define MSR_LSTAR       0xc0000082 /* long mode SYSCALL target */
#define MSR_CSTAR       0xc0000083 /* compat mode SYSCALL target */
#define MSR_SYSCALL_MASK    0xc0000084 /* EFLAGS mask for syscall */
#define MSR_FS_BASE     0xc0000100 /* 64bit FS base */
#define MSR_GS_BASE     0xc0000101 /* 64bit GS base */
#define MSR_KERNEL_GS_BASE  0xc0000102 /* SwapGS GS shadow */
#define MSR_TSC_AUX     0xc0000103 /* Auxiliary TSC */

/* Intel MSRs. Some also available on other CPUs */
#define MSR_IA32_PERFCTR0       0x000000c1
#define MSR_IA32_PERFCTR1       0x000000c2
#define MSR_FSB_FREQ            0x000000cd
#define MSR_PLATFORM_INFO       0x000000ce

#define MSR_IA32_APICBASE       0x0000001b
#define MSR_IA32_APICBASE_BSP       (1<<8)
#define MSR_IA32_APICBASE_ENABLE    (1<<11) /* The CPU then receives its interrupts directly from a 8259-compatible PIC */
#define MSR_IA32_APICBASE_BASE      (0xfffff<<12)

#define MSR_IA32_SYSENTER_CS        0x00000174
#define MSR_IA32_SYSENTER_ESP       0x00000175
#define MSR_IA32_SYSENTER_EIP       0x00000176

/* Intel defined MSRs. */
#define MSR_IA32_P5_MC_ADDR     0x00000000
#define MSR_IA32_P5_MC_TYPE     0x00000001
#define MSR_IA32_TSC            0x00000010
#define MSR_IA32_PLATFORM_ID        0x00000017
#define MSR_IA32_EBL_CR_POWERON     0x0000002a
#define MSR_EBC_FREQUENCY_ID        0x0000002c
#define MSR_SMI_COUNT           0x00000034
#define MSR_IA32_FEATURE_CONTROL        0x0000003a
#define MSR_IA32_TSC_ADJUST             0x0000003b
#define MSR_IA32_BNDCFGS        0x00000d90

#define MSR_IA32_CR_PAT         0x00000277

#define MSR_IA32_MISC_ENABLE        0x000001a0

static void save_vcpu_state(struct kvm_cpu *vcpu)
{
    struct {
        struct kvm_msrs info;
        struct kvm_msr_entry entries[MAX_MSR_ENTRIES];
    } msr_data;
    struct kvm_msr_entry *msrs = msr_data.entries;
    struct kvm_regs regs;
    struct kvm_sregs sregs;
    struct kvm_fpu fpu;
    struct kvm_lapic_state lapic;
    struct kvm_xsave xsave;
    struct kvm_xcrs xcrs;
    struct kvm_vcpu_events events;
    struct kvm_mp_state mp_state;
    char fname[MAX_FNAME];
    int n = 0;

	BUG_ON(!vcpu);

    /* define the list of required MSRs */
    msrs[n++].index = MSR_IA32_APICBASE;
    msrs[n++].index = MSR_IA32_SYSENTER_CS;
    msrs[n++].index = MSR_IA32_SYSENTER_ESP;
    msrs[n++].index = MSR_IA32_SYSENTER_EIP;
    msrs[n++].index = MSR_IA32_CR_PAT;
    msrs[n++].index = MSR_IA32_MISC_ENABLE;
    msrs[n++].index = MSR_IA32_TSC;
    msrs[n++].index = MSR_CSTAR;
    msrs[n++].index = MSR_STAR;
    msrs[n++].index = MSR_EFER;
    msrs[n++].index = MSR_LSTAR;
    msrs[n++].index = MSR_GS_BASE;
    msrs[n++].index = MSR_FS_BASE;
    msrs[n++].index = MSR_KERNEL_GS_BASE;
    //msrs[n++].index = MSR_IA32_FEATURE_CONTROL;
    msr_data.info.nmsrs = n;

    POP_DBGPRINTF("\t[dbg] collect vcpu[%lu] fd %d "
					"kernel states\n",
					vcpu->cpu_id, vcpu->vcpu_fd);

#if 0
    kvm_ioctl(vcpu->vcpu_fd, KVM_GET_MP_STATE, &mp_state); // qemu ./target/i386/kvm.c
//#if !POPHYPE_FT_HACK_SKIP_SAVING_VCPU
    kvm_ioctl(vcpu->vcpu_fd, KVM_GET_SREGS, &sregs); // ./virt/kvm/kvm_main.c
    kvm_ioctl(vcpu->vcpu_fd, KVM_GET_REGS, &regs);
    kvm_ioctl(vcpu->vcpu_fd, KVM_GET_MSRS, &msr_data);
    kvm_ioctl(vcpu->vcpu_fd, KVM_GET_XCRS, &xcrs);
    kvm_ioctl(vcpu->vcpu_fd, KVM_GET_LAPIC, &lapic);
    kvm_ioctl(vcpu->vcpu_fd, KVM_GET_FPU, &fpu);
    kvm_ioctl(vcpu->vcpu_fd, KVM_GET_XSAVE, &xsave); // kernel: arch/x86/kvm/x86.c
    kvm_ioctl(vcpu->vcpu_fd, KVM_GET_VCPU_EVENTS, &events);
    //kvm_ioctl(vcpu->vcpu_fd, KVM_GET_MP_STATE, &mp_state);
//#else
//	POP_DBGPRINTF("\t[dbg] FT HACK SKIP SAVING VCPU<%lu>\n", vcpu->cpu_id);
//#endif
#else // qemu order
    kvm_ioctl(vcpu->vcpu_fd, KVM_GET_VCPU_EVENTS, &events);
    kvm_ioctl(vcpu->vcpu_fd, KVM_GET_MP_STATE, &mp_state);
    kvm_ioctl(vcpu->vcpu_fd, KVM_GET_REGS, &regs);
    // if no xsave, do fpu (we have xsave in kernel)
    //kvm_ioctl(vcpu->vcpu_fd, KVM_GET_FPU, &fpu);
    kvm_ioctl(vcpu->vcpu_fd, KVM_GET_XSAVE, &xsave);
    kvm_ioctl(vcpu->vcpu_fd, KVM_GET_XCRS, &xcrs);
    kvm_ioctl(vcpu->vcpu_fd, KVM_GET_SREGS, &sregs);
    kvm_ioctl(vcpu->vcpu_fd, KVM_GET_MSRS, &msr_data);
    kvm_ioctl(vcpu->vcpu_fd, KVM_GET_LAPIC, &lapic);

#endif


    POP_DBGPRINTF("\t[dbg] save vcpu<%lu> to file\n", vcpu->cpu_id);
    /* ckpt write */
    int ofs = 0;
#if POPHYPE_FT_TO_MEM
    ofs = snprintf(fname + ofs, MAX_FNAME, "/tmp/");
#endif
    //ofs = snprintf(fname + ofs, MAX_FNAME, "vcpu%lu.dat", vcpu->cpu_id);
    //ofs = snprintf(fname + ofs, MAX_FNAME, "/home/jackchuang/c/vcpu%lu.dat", vcpu->cpu_id);
    ofs = snprintf(fname + ofs, MAX_FNAME, "/vcpu%lu.dat", vcpu->cpu_id);
    POP_DBGPRINTF("%s(): debug \"%s\"\n", __func__, fname);
    FILE* f = fopen(fname, "w+");
    if (f == NULL) {
        printf("%s(): fopen: unable to open file", __func__);
        exit(-1);
    }

    if (fwrite(&sregs, sizeof(sregs), 1, f) != 1)
        perror("fwrite failed\n");
    if (fwrite(&regs, sizeof(regs), 1, f) != 1)
        perror("fwrite failed\n");
    if (fwrite(&fpu, sizeof(fpu), 1, f) != 1)
        perror("fwrite failed\n");
    if (fwrite(&msr_data, sizeof(msr_data), 1, f) != 1)
        perror("fwrite failed\n");
    if (fwrite(&lapic, sizeof(lapic), 1, f) != 1)
        perror("fwrite failed\n");
    if (fwrite(&xsave, sizeof(xsave), 1, f) != 1)
        perror("fwrite failed\n");
    if (fwrite(&xcrs, sizeof(xcrs), 1, f) != 1)
        perror("fwrite failed\n");
    if (fwrite(&events, sizeof(events), 1, f) != 1)
        perror("fwrite failed\n");
    if (fwrite(&mp_state, sizeof(mp_state), 1, f) != 1)
        perror("fwrite failed\n");


    POP_DBGPRINTF("[dbg] %s(): <%lu> regs.rip 0x%llx = "
				"vcpu->kvm->arch.boot_ip 0x%x\n",
				__func__, vcpu->cpu_id, regs.rip,
				vcpu->kvm->arch.boot_ip);
    POP_DBGPRINTF("[dbg] %s(): <%lu> sregs.cr3 0x%llx\n",
						__func__, vcpu->cpu_id, sregs.cr3);
    POP_DBGPRINTF("[dbg] %s(): <%lu> sregs.apic_base 0x%llx\n",
						__func__, vcpu->cpu_id, sregs.apic_base);

    POP_DBGPRINTF("<%ld> ckpt apic_base 0x%llx\n",
					vcpu->cpu_id, vcpu->kvm_run->apic_base);
    POP_DBGPRINTF("<%ld> ckpt mp_state 0x%x\n",
					vcpu->cpu_id, mp_state.mp_state);
    POP_DBGPRINTF("<%ld> ckpt regs.rip 0x%llx\n",
							vcpu->cpu_id, regs.rip);
    POP_DBGPRINTF("<%ld> ckpt regs.rflags 0x%llx\n",
							vcpu->cpu_id, regs.rflags);

    fclose(f);
}


/* Store at local */
static void load_vcpu_state(struct kvm_cpu *vcpu)
{
    struct kvm_mp_state mp_state = { KVM_MP_STATE_RUNNABLE };
	//struct kvm_mp_state mp_state = { KVM_MP_STATE_HALTED };
    struct kvm_regs regs = {
		/* These are fron init_cpu_state() in HermitCore.
			We should NOT use these. */
        //.rip = elf_entry, // entry point to HermitCore
//        .rip = vcpu->kvm->arch.boot_ip, // entry point to kernel (pophype)
//        .rflags = 0x2,	// POR value required by x86 architecture (pophype)
    };
	kvm_cpu__setup_cpuid(vcpu);
    char fname[MAX_FNAME];
    struct kvm_sregs sregs;
    struct kvm_fpu fpu;
    struct {
        struct kvm_msrs info;
        struct kvm_msr_entry entries[MAX_MSR_ENTRIES];
    } msr_data;
    struct kvm_lapic_state lapic;
    struct kvm_xsave xsave;
    struct kvm_xcrs xcrs;
    struct kvm_vcpu_events events;

    int ofs = 0;
	BUG_ON(!vcpu);
#if POPHYPE_FT_TO_MEM
    ofs = snprintf(fname + ofs, MAX_FNAME, "/tmp/");
#endif
//testing210305 start
    //ofs = snprintf(fname + ofs, MAX_FNAME, "vcpu%lu.dat", vcpu->cpu_id);
//testing210305 end
    ofs = snprintf(fname + ofs, MAX_FNAME, "/vcpu%lu.dat", vcpu->cpu_id);
    POP_DBGPRINTF("%s(): debug r \"%s\"\n", __func__, fname);

    FILE* f = fopen(fname, "r");
    if (f == NULL) {
        printf("%s(): fopen: unable to open file", __func__);
        exit(-1);
    }

    if (fread(&sregs, sizeof(sregs), 1, f) != 1)
        perror("fread failed\n");
    if (fread(&regs, sizeof(regs), 1, f) != 1)
        perror("fread failed\n");
    if (fread(&fpu, sizeof(fpu), 1, f) != 1)
        perror("fread failed\n");
    if (fread(&msr_data, sizeof(msr_data), 1, f) != 1)
        perror("fread failed\n");
    if (fread(&lapic, sizeof(lapic), 1, f) != 1)
        perror("fread failed\n");
    if (fread(&xsave, sizeof(xsave), 1, f) != 1)
        perror("fread failed\n");
    if (fread(&xcrs, sizeof(xcrs), 1, f) != 1)
        perror("fread failed\n");
    if (fread(&events, sizeof(events), 1, f) != 1)
        perror("fread failed\n");
    if (fread(&mp_state, sizeof(mp_state), 1, f) != 1)
        perror("fread failed\n");

    fclose(f);
#if 0
    kvm_ioctl(vcpu->vcpu_fd, KVM_SET_MP_STATE, &mp_state); // qemu ./target/i386/kvm.c
    kvm_ioctl(vcpu->vcpu_fd, KVM_SET_SREGS, &sregs);
    kvm_ioctl(vcpu->vcpu_fd, KVM_SET_REGS, &regs);
    kvm_ioctl(vcpu->vcpu_fd, KVM_SET_MSRS, &msr_data);
    kvm_ioctl(vcpu->vcpu_fd, KVM_SET_XCRS, &xcrs);
    //kvm_ioctl(vcpu->vcpu_fd, KVM_SET_MP_STATE, &mp_state);
    kvm_ioctl(vcpu->vcpu_fd, KVM_SET_LAPIC, &lapic);
    kvm_ioctl(vcpu->vcpu_fd, KVM_SET_FPU, &fpu);
    kvm_ioctl(vcpu->vcpu_fd, KVM_SET_XSAVE, &xsave);
    kvm_ioctl(vcpu->vcpu_fd, KVM_SET_VCPU_EVENTS, &events);
#else // qemu order
    kvm_ioctl(vcpu->vcpu_fd, KVM_SET_REGS, &regs);
    // qemu only use fpu if there is no xsave (we have xsave in kernel)
    //kvm_ioctl(vcpu->vcpu_fd, KVM_SET_FPU, &fpu);
    kvm_ioctl(vcpu->vcpu_fd, KVM_SET_XSAVE, &xsave);
    kvm_ioctl(vcpu->vcpu_fd, KVM_SET_XCRS, &xcrs);
    kvm_ioctl(vcpu->vcpu_fd, KVM_SET_SREGS, &sregs);

    kvm_ioctl(vcpu->vcpu_fd, KVM_SET_MSRS, &msr_data);
    kvm_ioctl(vcpu->vcpu_fd, KVM_SET_LAPIC, &lapic);
    kvm_ioctl(vcpu->vcpu_fd, KVM_SET_VCPU_EVENTS, &events);
    kvm_ioctl(vcpu->vcpu_fd, KVM_SET_MP_STATE, &mp_state);
#endif

    POP_DBGPRINTF("[dbg] %s(): <%lu> regs.rip 0x%llx = "
					"vcpu->kvm->arch.boot_ip 0x%x\n",
					__func__, vcpu->cpu_id, regs.rip,
					vcpu->kvm->arch.boot_ip);
    POP_DBGPRINTF("[dbg] %s(): <%lu> sregs.cr3 0x%llx\n",
						__func__, vcpu->cpu_id, sregs.cr3);
    POP_DBGPRINTF("[dbg] %s(): <%lu> sregs.apic_base 0x%llx\n",
						__func__, vcpu->cpu_id, sregs.apic_base);
    POP_DBGPRINTF("<%ld> vcpu->kvm->arch.boot_ip (rip?) - 0x%x \n",
							vcpu->cpu_id, vcpu->kvm->arch.boot_ip);
    POP_DBGPRINTF("<%ld> apic_base kvm_run 0x%llx sregs 0x%llx\n",
			vcpu->cpu_id, vcpu->kvm_run->apic_base, sregs.apic_base);
	POP_DBGPRINTF("<%ld> restart regs.rip 0x%llx\n",
								vcpu->cpu_id, regs.rip);
    POP_DBGPRINTF("<%ld> restart regs.rflags 0x%llx\n",
							vcpu->cpu_id, regs.rflags);
    return;
}
#endif

int __attribute__((weak)) kvm_cpu__get_endianness(struct kvm_cpu *vcpu)
{
	return VIRTIO_ENDIAN_HOST;
}

void kvm_cpu__enable_singlestep(struct kvm_cpu *vcpu)
{
	struct kvm_guest_debug debug = {
		.control	= KVM_GUESTDBG_ENABLE | KVM_GUESTDBG_SINGLESTEP,
	};

	if (ioctl(vcpu->vcpu_fd, KVM_SET_GUEST_DEBUG, &debug) < 0)
		pr_warning("KVM_SET_GUEST_DEBUG failed");
}

unsigned long retry[32] = { 0 };
void kvm_cpu__run(struct kvm_cpu *vcpu)
{
	int err;

	if (!vcpu->is_running)
		return;

	err = ioctl(vcpu->vcpu_fd, KVM_RUN, 0);
	if (err < 0 && (errno != EINTR && errno != EAGAIN)
		&& vcpu->kvm_run->exit_reason != KVM_EXIT_POPHYPE_MIGRATE) {
		POP_PRINTF(" ========================================\n");
		POP_PRINTF(" ========= [BAD] USR DIE ERR %d =========\n", err);
		POP_PRINTF(" ========================================\n");
		if (++retry[pop_get_nid()] >= 3) {
			POP_PRINTF("TODO: >=3 handle migration case. Others BUG_ON(-1)\n");
			//BUG_ON(-1);
		} else { POP_PRINTF("TODO: <3 handle migration case. Others BUG_ON(-1)\n"); }
		//die_perror("KVM_RUN failed");
	}
}

#if defined(CONFIG_POPCORN_HYPE)
static void kvm_cpu_signal_handler_vanilla(int signum)
{
    if (signum == SIGKVMEXIT) {
        if (current_kvm_cpu && current_kvm_cpu->is_running)
            current_kvm_cpu->is_running = false;
		BUG_ON(-1);
    } else if (signum == SIGKVMPAUSEVANILLA) {
        if (current_kvm_cpu->paused)
            die("Pause signaled for already paused CPU\n");

        /* pause_lock is held by kvm__pause() */
        current_kvm_cpu->paused = 1;

        /*
         * This is a blocking function and uses locks. It is safe
         * to call it for this signal as a second pause event should
         * not be send to this thread until it acquires and releases
         * the pause_lock.
         */
        kvm__notify_paused();
    }

    /* For SIGKVMTASK cpu->task is already set */
}

void do_ckpt_at_origin(struct kvm *kvm);
void do_ckpt_at_origin(struct kvm *kvm)
{
#if 1
	FILE *fptr;
	int i, name_ofs = 0;
	char fname[MAX_FNAME];
	long unsigned long ret = 0;

	/* save_vcpu */
#if SHOW_POPHYPE_FT_TIME // finer breakdown
//	struct timeval tstart, tstop;
//	int tusecs;
//	gettimeofday(&tstart, NULL);
#endif
	POP_PRINTF("[%d] [[CKPT]] save %d vcpus\n",
				popcorn_gettid(), kvm->cfg.nrcpus);

#if 0
	for (i = 0; i < kvm->cfg.nrcpus; i++) {
#else
	/* Only save/restotre original vCPU */
	for (i = 0; i < 1; i++) {
#endif
	//	int hack_i = 0;
		POP_DBGPRINTF("[%d] [dbg] write vcpu%lu.dat / %d %p\n",
					popcorn_gettid(), gvcpu[i]->cpu_id,
					kvm->cfg.nrcpus, gvcpu[i]);
		save_vcpu_state(gvcpu[i]); // testing2019
	//	save_vcpu_state(gvcpu[hack_i]);
	}
	//int hack_i = 0;
	//save_vcpu_state(gvcpu[hack_i]);

#if SHOW_POPHYPE_FT_TIME // finer breakdown
//	gettimeofday(&tstop, NULL);
//	tusecs = ((tstop.tv_sec - tstart.tv_sec) * 1000000) +
//				(tstop.tv_usec - tstart.tv_usec);
//	printf("pophype: lkvm ft \"save vcpu\" "
//			"[time] = %d ms [[[%d s]]] gettimeofday()\n",
//			tusecs / 1000, tusecs / 1000 / 1000);
#endif

#if 1
	/* mem1 */
	POP_PRINTF("[CKPT] - write mem1.dat\n");
#if POPHYPE_FT_TO_MEM
	name_ofs = snprintf(fname + name_ofs, MAX_FNAME, "/tmp/");
#endif
	name_ofs = snprintf(fname + name_ofs, MAX_FNAME, "/mem1.dat");
	//name_ofs = snprintf(fname + name_ofs, MAX_FNAME, "/ft/mem1.dat");
	POP_DBGPRINTF("%s(): debug w+ \"%s\"\n", __func__, fname);
	fptr = fopen(fname, "w+");
	if (fptr == NULL) { // exiting program
		printf("File open Error!");
		exit(1);
	}
#if 0
////////////////////////////////////////////////////////////
{// testing write dummy chars
//int i;
int divider = 100;
u64 oft = 0;
for (i=0;i<10;i++)
	POP_PRINTF("[CKPT] - [TEST] write DUMMY jack_mem mem1.dat sizeof(char) %lu\n", sizeof(char));
//char *jack_mem = malloc(first_mem_size/divider);
char *jack_mem = malloc(first_mem_size);
if (!jack_mem) {
	POP_PRINTF("[CKPT] - [TEST] allocate jack_mem FAILED!!\n");
	BUG_ON(-1);
}
memset(jack_mem, 'z', first_mem_size);
//memset(jack_mem, 'z', first_mem_size/divider);
//POP_PRINTF("[CKPT] - [TEST] write jack_mem size %llu B\n", first_mem_size/divider);
POP_PRINTF("[CKPT] - [TEST] write jack_mem size %llu B\n", first_mem_size);

while (oft < first_mem_size) {
	POP_PRINTF("\t %llu/%llu \n", oft, first_mem_size);
	oft += fwrite(jack_mem + oft, sizeof(char), first_mem_size/divider, fptr);
	ret = oft;
}
POP_PRINTF("\t %llu/%llu DONE\n", oft, first_mem_size);
free(jack_mem);
POP_PRINTF("jack_mem FREEed\n");
}
////////////////////////////////////////////////////////////
#endif
	ret = fwrite(first_mem, sizeof(char), first_mem_size, fptr);
//POP_PRINTF("closing fptr\n");
//POP_PRINTF("skip\n");
	fclose(fptr);
// testing 210224

	POP_PRINTF("[CKPT] - wrote mem1.dat "
				"%llu Bytes (%llu MB) [O: %llu MB] %p\n",
				ret, ret / 1024 / 1024,
				first_mem_size / 1024 / 1024, first_mem);
#endif // mem1

#if 1
	/* mem2 */
	name_ofs = 0;
#if POPHYPE_FT_TO_MEM
	name_ofs = snprintf(fname + name_ofs, MAX_FNAME, "/tmp/");
#endif
	name_ofs = snprintf(fname + name_ofs, MAX_FNAME, "/mem2.dat");
	//name_ofs = snprintf(fname + name_ofs, MAX_FNAME, "/ft/mem2.dat");
	POP_DBGPRINTF("%s(): debug w+ \"%s\"\n", __func__, fname);
	fptr = fopen(fname, "w+");
	if (fptr == NULL) {
		printf("File open Error!");
		exit(1);
	}
	ret = fwrite(second_mem, sizeof(char), second_mem_size, fptr);
	fclose(fptr);
	POP_PRINTF("CKPT - wrote mem2.dat "
				"0x%llx Bytes (%llu MB) [O: %llu MB] %p\n",
				ret, ret / 1024 / 1024,
				second_mem_size / 1024 / 1024, second_mem);
#endif // mem2

#endif
}
#endif

void do_restart_at_origin(struct kvm *kvm);
void do_restart_at_origin(struct kvm *kvm)
{
	FILE *infile;
	// mem
	long unsigned long ret;
// func("mem1.dat", first_mem, first_mem_size)
	char fname[MAX_FNAME];
	char fname_mem2[MAX_FNAME];
	int i, name_ofs = 0;

	POP_PRINTF("[ckpt] restore %d vcpus\n", kvm->cfg.nrcpus);
#if 0
	for (i = 0; i < kvm->cfg.nrcpus; i++) {
#else
	/* Only save/restotre original vCPU */
	for (i = 0; i < 1; i++) {
#endif
		POP_DBGPRINTF("[%d] [dbg] read vcpu%lu.dat / %d %p\n",
			popcorn_gettid(), gvcpu[i]->cpu_id, kvm->cfg.nrcpus, gvcpu[i]);
		load_vcpu_state(gvcpu[i]);
	}

#if POPHYPE_FT_TO_MEM
	name_ofs = snprintf(fname + name_ofs, MAX_FNAME, "/tmp/");
#endif
	name_ofs = snprintf(fname + name_ofs, MAX_FNAME, "/mem1.dat");
	//name_ofs = snprintf(fname + name_ofs, MAX_FNAME, "/ft/mem1.dat");
	POP_DBGPRINTF("%s(): debug r \"%s\"\n", __func__, fname);
	infile = fopen (fname, "r");
	if (infile == NULL) {
		fprintf(stderr, "\nError opening file\n");
		exit (1);
	}

	ret = fread(first_mem, sizeof(char), first_mem_size, infile);
	fclose (infile);
	POP_PRINTF("[%d] [[RESTART]] - read mem1.dat first_mem %p 0%llx "
				"(%llu MB) [O: %llu MB]\n", popcorn_gettid(),
				first_mem, ret, ret / 1024 / 1024, first_mem_size / 1024 / 1024);

// func("mem2.dat", second_mem, second_mem_size)
	name_ofs = 0;
#if POPHYPE_FT_TO_MEM
	name_ofs = snprintf(fname_mem2 + name_ofs, MAX_FNAME, "/tmp/");
#endif
	name_ofs = snprintf(fname_mem2 + name_ofs, MAX_FNAME, "/mem2.dat");
	//name_ofs = snprintf(fname_mem2 + name_ofs, MAX_FNAME, "/ft/mem2.dat");
	POP_DBGPRINTF("%s(): debug r \"%s\"\n", __func__, fname_mem2);
	infile = fopen (fname_mem2, "r");
	if (infile == NULL) {
		fprintf(stderr, "\nError opening file\n");
		exit (1);
	}

	ret = fread(second_mem, sizeof(char), second_mem_size, infile);
	fclose (infile);

	POP_PRINTF("[%d] [[RESTART]] - read mem2.dat second_mem %p 0x%llx "
				"(%llu MB) [O: %llu MB]\n", popcorn_gettid(),
				second_mem, ret, ret / 1024 / 1024,
				second_mem_size / 1024 / 1024);
}


static void kvm_cpu_signal_handler(int signum)
{
#if defined(CONFIG_POPCORN_HYPE)
	static int cnt = 0;
    int dst_nid = -1, i;
	cnt++;
	POP_PRINTF("\n\n\t-|> <%lu> [%d/%d/%d] %s(): got sig %d start %s "
		"(SIGKVMFTCKPT/RESTART) #%d\n",
		current_kvm_cpu->cpu_id, pop_get_nid(), getpid(),
		popcorn_gettid(), __func__, signum, __FILE__, cnt);
#endif
	if (signum == SIGKVMEXIT) {
		if (current_kvm_cpu && current_kvm_cpu->is_running)
			current_kvm_cpu->is_running = false;
	} else if (signum == SIGKVMPAUSE) {
#if defined(CONFIG_POPCORN_HYPE)
		for (i = 0; i < 10; i++) {
			POP_PRINTF("\t-> <%lu> [%d/%d/%d] %s(): BUG BUG BUG no longer used\n",
						current_kvm_cpu->cpu_id, pop_get_nid(),
						getpid(), popcorn_gettid(), __func__);
		}

		POP_PRINTF("\t-> <%lu> [%d/%d/%d] %s(): *** got signal SIGKVMPAUSE %d *** "
				"target's local/remote vcpu<%lu> "
				"do current_kvm_cpu->paused = 1\n",
				current_kvm_cpu->cpu_id, pop_get_nid(), getpid(), popcorn_gettid(),
				__func__, SIGKVMPAUSE, current_kvm_cpu->cpu_id);
#endif
		if (current_kvm_cpu->paused)
			die("Pause signaled for already paused CPU\n");

		/* pause_lock is held by kvm__pause() */
		current_kvm_cpu->paused = 1;

		/*
		 * This is a blocking function and uses locks. It is safe
		 * to call it for this signal as a second pause event should
		 * not be send to this thread until it acquires and releases
		 * the pause_lock.
		 */
#if defined(CONFIG_POPCORN_HYPE)
		POP_PRINTF("\t-> <%lu> [%d/%d/%d] %s(): do kvm__notify_paused()\n",
			current_kvm_cpu->cpu_id, pop_get_nid(), getpid(), popcorn_gettid(), __func__);
#endif
		kvm__notify_paused();
#if defined(CONFIG_POPCORN_HYPE)
		POP_PRINTF("\t-> <%lu> [%d/%d/%d] %s(): kvm__notify_paused() done\n",
			current_kvm_cpu->cpu_id, pop_get_nid(), getpid(),
			popcorn_gettid(), __func__);
#endif
	}
#if defined(CONFIG_POPCORN_HYPE)
	else if (signum == SIGKVMCPUMIGRATE_BACK_W_STATE ||
				signum == SIGKVMFTCKPT) {
		const char *op = "CKPT";
		/* remotes (w/ state transfer) to origin */
        // this vcpu does vcpu migration
        /* pophype - this is the place where one of the vcpus
            receives brocast SIGKVMCPUMIGRATE_BACK_W_STATE signals */
		POP_PRINTF("\t-> <%lu> [%d/%d/%d] %s(): "
					"***** got signal [[%d]] "
					"(SIGKVMCPUMIGRATE_BACK_W_STATE %d/"
					"SIGKVMFTCKPT %d) *****\n",
					current_kvm_cpu->cpu_id, pop_get_nid(), getpid(),
					popcorn_gettid(), __func__, signum,
					SIGKVMCPUMIGRATE_BACK_W_STATE, SIGKVMFTCKPT);

		if (current_kvm_cpu->paused)
			die("Pause signaled for already paused CPU\n");

		/* pause_lock is held by kvm__pause() */
		current_kvm_cpu->paused = 1;

		/* Now we have a ipc_thread waiting outside.
			These vcpu threads will complete ckpt here.
			Remote threads collect vcpu info and migrate back to origin.
			Oirign/leader vcpu thread does ckpt.
		*/
		if (pop_get_nid()) { // remote

//testing210305 start testing210309local
			/* Write remote vCPU states at local remote */
			/* Can migrate but don't load remote vCPU states at origin */
//			save_vcpu_state(gvcpu[current_kvm_cpu->cpu_id]);
//testing210305 done

#if 1 // testing210309local remote cannot write?
#if 0  //testing210305 done
			POP_PRINTF("\t-> <%lu> %s - pophype_prepare_vcpu_migrate() "
						"start\n", current_kvm_cpu->cpu_id, op);
			/* Retrive vCPU kernel states for pophype migration */
			pophype_prepare_vcpu_migrate(current_kvm_cpu->cpu_id);
			POP_PRINTF("\t-> <%lu> CKPT - pophype_prepare_vcpu_migrate() "
						"done\n", current_kvm_cpu->cpu_id);
			POP_PRINTF("\t\t\t\t-> <%lu> [%d/%d/%d] %s(): [[[CKPT]]]/RESTART\n",
						current_kvm_cpu->cpu_id, pop_get_nid(),
						getpid(), popcorn_gettid(), __func__);

			// pophype migration with states
			/* remote to origin */
			dst_nid = (MAX_POPCORN_VCPU * 1) + current_kvm_cpu->cpu_id;
			POP_PRINTF("\t\t-> [%d/%d/%d] %s(): <%ld> migrate(%d) <%ld>\n",
				pop_get_nid(), getpid(), popcorn_gettid(), __func__,
				current_kvm_cpu->cpu_id, dst_nid, current_kvm_cpu->cpu_id);
			migrate(dst_nid, NULL, NULL);
			POP_PRINTF("=> <%lu> [%d/%d/%d] %s(): [[[[[I'm from remote]]]]]\n",
						current_kvm_cpu->cpu_id, pop_get_nid(),
						getpid(), popcorn_gettid(), __func__);
#else
			//migrate(current_kvm_cpu->cpu_id, NULL, NULL);
///////###############210311nomigrate ckpt
//			migrate(0, NULL, NULL);
///////###############210311nomigrate
#endif
#endif
			/* Sync1 - transffering remote vcpu states to origin done.
						Notify origin. */
			ckpt_done[current_kvm_cpu->cpu_id] = true;
			//smp_wmb();

			/* Sync2 - origin ckpt done. Proceed & let ipc_thread resumes. */
			while (!ckpt_done[0]) { sched_yield(); }
			POP_PRINTF("\t\t-> [%d/%d/%d] %s(): origin done\n",
					pop_get_nid(), getpid(), popcorn_gettid(), __func__);

			/* TODO: AFTER write - migrate back to remote */
			POP_PRINTF("\t\t-> [%d/%d/%d] %s(): GOOD REMOTE\n",
					pop_get_nid(), getpid(), popcorn_gettid(), __func__);


			// goin to migrate back to remote
		} else { // origin
			POP_PRINTF("\t\t-> [%d/%d/%d] %s(): migrate(%d) stay at origin "
						"nrcpus = %d\n",
						pop_get_nid(), getpid(), popcorn_gettid(),
						__func__, dst_nid, current_kvm_cpu->kvm->nrcpus);
			/* sync1 - wait till all remote vcpu states transffered to origin.
				Do ckpt. */
			for (i = 1; i < current_kvm_cpu->kvm->nrcpus; i++) {
				//while (!ckpt_done[i]) { ; }
				while (!ckpt_done[i]) { sched_yield(); }
			}
			/* Do ckpt */
			POP_PRINTF("\t\t-> [%d/%d/%d] %s(): DO CKPT\n",
					pop_get_nid(), getpid(), popcorn_gettid(), __func__);

			do_ckpt_at_origin(current_kvm_cpu->kvm);
			// 1. remote cannot wait bits[origin]
			// 2. all vcpu shoul wait ipc_thread bit
			POP_PRINTF("\t\t-> [%d/%d/%d] %s(): CKPT done\n",
					pop_get_nid(), getpid(), popcorn_gettid(), __func__);

			/* Sync2 - notify remote vcpu threads "all done" */
			BUG_ON(current_kvm_cpu->cpu_id != 0);
			ckpt_done[current_kvm_cpu->cpu_id] = true; // cpu_id = 0;
			//smp_wmb();

			POP_PRINTF("\t\t-> [%d/%d/%d] %s(): GOOD ORIGIN\n",
					pop_get_nid(), getpid(), popcorn_gettid(), __func__);
		} // remote/origin done
		/* Go event write at origin! and migrate back */

		POP_PRINTF("\t-> <%lu> [%d/%d/%d] %s(): "
			"do kvm__notify_ft_vcpu_migrated(true) #%d\n",
			current_kvm_cpu->cpu_id, pop_get_nid(),
			getpid(), popcorn_gettid(), __func__, cnt);

		kvm__notify_ft_vcpu_migrated(true); // write(pause_event)

//		if (pop_get_nid()) {
////testing210305 start testing210309local
//			POP_PRINTF("[%d] [dbg] write vcpu%lu.dat / %d %p\n",
//						popcorn_gettid(), gvcpu[i]->cpu_id,
//						current_kvm_cpu->kvm->cfg.nrcpus,
//						gvcpu[current_kvm_cpu->cpu_id]);
//			/* Write remote vCPU states at local remote */
//			/* Can migrate but don't load remote vCPU states at origin */
//			save_vcpu_state(gvcpu[current_kvm_cpu->cpu_id]);
////testing210305 done
//		}
    } else if (signum == SIGKVMFTRESTART) {
		const char *op = "RESTART";
		POP_PRINTF("\t-> <%lu> [%d/%d/%d] %s(): [%s] "
					"***** got signal SIGKVMFTRESTART %d *****\n",
					current_kvm_cpu->cpu_id, pop_get_nid(), getpid(),
					popcorn_gettid(), __func__, op, SIGKVMFTRESTART);

		if (current_kvm_cpu->paused)
			die("Pause signaled for already paused CPU\n");

		/* pause_lock is held by kvm__pause() */
		current_kvm_cpu->paused = 1;

		if (pop_get_nid()) {
			POP_PRINTF("\t-> <%lu> [%s] - [remote] goes back to origin "
						"start\n", current_kvm_cpu->cpu_id, op);

//testing210305 start
			/* READ remote vCPU states at local remote */
			/* Can migrate but don't load remote vCPU states at origin */
			POP_PRINTF("[%d] [dbg] read vcpu%lu.dat / %d %p\n",
						popcorn_gettid(),
						gvcpu[current_kvm_cpu->cpu_id]->cpu_id,
						current_kvm_cpu->kvm->cfg.nrcpus,
						gvcpu[current_kvm_cpu->cpu_id]);
//			load_vcpu_state(gvcpu[current_kvm_cpu->cpu_id]);
//testing210305 done

/////////////////
#if 1 // testing210309local
#if 1 // popcorn migrate
			// popcorn migrate
			POP_PRINTF("\t\t-> [%d/%d/%d] %s(): %s <%ld> "
				"[popcorn migrate(%d)] <%ld>\n",
				pop_get_nid(), getpid(), popcorn_gettid(),
				__func__, op, current_kvm_cpu->cpu_id,
				dst_nid, current_kvm_cpu->cpu_id);
///////###############210311nomigrate restart
//			migrate(0, NULL, NULL);
///////###############210311nomigrate
#else
#if 1
/* because of gvcpu map */
			POP_PRINTF("\t-> <%lu> %s - pophype_prepare_vcpu_migrate() "
						"start\n", current_kvm_cpu->cpu_id, op);
			/* Retrive vCPU kernel states for pophype migration */
			pophype_prepare_vcpu_migrate(current_kvm_cpu->cpu_id);
			POP_PRINTF("\t-> <%lu> %s - pophype_prepare_vcpu_migrate() "
						"done\n", current_kvm_cpu->cpu_id, op);
			POP_PRINTF("\t\t\t\t-> <%lu> [%d/%d/%d] %s(): %s\n",
						current_kvm_cpu->cpu_id, pop_get_nid(),
						getpid(), popcorn_gettid(), __func__, op);

			// pophype migration with states
			/* remote to origin */
			dst_nid = (MAX_POPCORN_VCPU * 1) + current_kvm_cpu->cpu_id;
			POP_PRINTF("\t\t-> [%d/%d/%d] %s(): "
						"%s <%ld> [pophype migrate(%d)] <%ld>\n",
						pop_get_nid(), getpid(), popcorn_gettid(),
						__func__, op, current_kvm_cpu->cpu_id,
						dst_nid, current_kvm_cpu->cpu_id);
			migrate(dst_nid, NULL, NULL);
#endif
#endif
#endif
/////////

			restart_done[current_kvm_cpu->cpu_id] = true;

			while (!restart_done[0]) { sched_yield(); }

			/* restart at origin done */
			POP_PRINTF("\t-> <%lu> [%s] - [remote] goes back to remote "
						"start\n", current_kvm_cpu->cpu_id, op);
		} else {  // origin
			POP_PRINTF("\t-> <%lu> [%s] - [origin] waits for all remotes "
						"come back\n", current_kvm_cpu->cpu_id, op);
			for (i = 1; i < current_kvm_cpu->kvm->nrcpus; i++) {
				while (!restart_done[i]) { sched_yield(); }
			}

//POP_PRINTF("\t-> <%lu> RESTART - testing0303 skip do_restart_at_origin()\n",
//			current_kvm_cpu->cpu_id);
			POP_PRINTF("\t-> <%lu> RESTART - do_restart_at_origin() start\n",
						current_kvm_cpu->cpu_id);
			do_restart_at_origin(current_kvm_cpu->kvm);
			POP_PRINTF("\t-> <%lu> RESTART - do_restart_at_origin() done\n",
						current_kvm_cpu->cpu_id);
//POP_PRINTF("\t-> <%lu> RESTART - testing0303 skip do_restart_at_origin()\n",
//			current_kvm_cpu->cpu_id);

			restart_done[0] = true;
			/* Let remote vcpus migrate back to remote nodes */
		}

		POP_PRINTF("\t-> <%lu> [%d/%d/%d] %s(): "
			"do kvm__notify_ft_vcpu_migrated(false) #%d\n",
			current_kvm_cpu->cpu_id, pop_get_nid(),
			getpid(), popcorn_gettid(), __func__, cnt);

		kvm__notify_ft_vcpu_migrated(false); // write(pause_event)


//		if (pop_get_nid()) {
////testing210305 start
//			/* READ remote vCPU states at local remote */
//			/* Can migrate but don't load remote vCPU states at origin */
//			POP_PRINTF("[%d] [dbg] read vcpu%lu.dat / %d %p\n",
//						popcorn_gettid(), gvcpu[i]->cpu_id,
//						current_kvm_cpu->kvm->cfg.nrcpus, gvcpu[i]);
//			load_vcpu_state(gvcpu[current_kvm_cpu->cpu_id]);
////testing210305 done
//		}
	} // signum end
	/* origin (w/ state transfer) to origin: for restart/resume*/
	// maybe no need because origin can deal with it


// testing210310
	if (current_kvm_cpu->cpu_id > 0) { // && !pop_get_nid()) {
//		pophype_origin_checkin_vcpu_pid(current_kvm_cpu->cpu_id); // HACK nid=vcpu_id
		if (signum == SIGKVMFTCKPT ||
			signum == SIGKVMCPUMIGRATE_BACK_W_STATE) {
			POP_PRINTF("\t\t-> [%d/%d/%d] %s(): <%lu> [CKPT] "
						"[popcorn migrate] - origin to %d <%lu>\n",
						pop_get_nid(), getpid(), popcorn_gettid(),
						__func__, current_kvm_cpu->cpu_id,
						dst_nid, current_kvm_cpu->cpu_id);

///////###############210311nomigrate
//			migrate(current_kvm_cpu->cpu_id, NULL, NULL);
///////###############210311nomigrate
			popcorn_setcpuaffinity(current_kvm_cpu->cpu_id);
//			pophype_remote_checkin_vcpu_pid(getpid());

			POP_PRINTF("[%d] [dbg] write vcpu%lu.dat / %d %p\n",
						popcorn_gettid(),
						gvcpu[current_kvm_cpu->cpu_id]->cpu_id,
						current_kvm_cpu->kvm->cfg.nrcpus,
						gvcpu[current_kvm_cpu->cpu_id]);
			save_vcpu_state(gvcpu[current_kvm_cpu->cpu_id]);
		} else if (signum == SIGKVMFTRESTART) {
				POP_PRINTF("\t\t-> [%d/%d/%d] %s(): "
						"<%lu> [RESTART] [popcorn migrate] - "
						"origin to %d <%lu>\n",
						pop_get_nid(), getpid(), popcorn_gettid(),
						__func__, current_kvm_cpu->cpu_id,
				dst_nid, current_kvm_cpu->cpu_id);
///////###############210311nomigrate
//				migrate(current_kvm_cpu->cpu_id, NULL, NULL);
///////###############210311nomigrate
				popcorn_setcpuaffinity(current_kvm_cpu->cpu_id);
//				pophype_remote_checkin_vcpu_pid(getpid());

				// testing210311 KVM_SET_CPUID2
				//kvm_cpu__setup_cpuid(current_kvm_cpu); // reset
				// testing210311 KVM_SET_CPUID2
				POP_PRINTF("[%d] [dbg] read vcpu%lu.dat / %d %p\n",
							popcorn_gettid(),
							gvcpu[current_kvm_cpu->cpu_id]->cpu_id,
							current_kvm_cpu->kvm->cfg.nrcpus,
							gvcpu[current_kvm_cpu->cpu_id]);
				load_vcpu_state(gvcpu[current_kvm_cpu->cpu_id]);
		} else { BUG_ON(-1); }
		/* Solve vCPU 100% at remote after back migration.
			Solve network problem after ckpt but not restart.
			Previous delegation approach may wanna try this. */
//		popcorn_setcpuaffinity(current_kvm_cpu->cpu_id);
//		pophype_remote_checkin_vcpu_pid(getpid());
	}
// testing210310
	if (signum == SIGKVMFTCKPT ||
		signum == SIGKVMCPUMIGRATE_BACK_W_STATE ||
		signum == SIGKVMFTRESTART) {
		all_done[current_kvm_cpu->cpu_id] = true;
	}

#endif

	/* For SIGKVMTASK cpu->task is already set */

#if defined(CONFIG_POPCORN_HYPE)
	POP_PRINTF("\t-> <%lu> [%d/%d/%d] %s(): done #%d\n\n\n\n",
		current_kvm_cpu->cpu_id, pop_get_nid(), getpid(),
		popcorn_gettid(), __func__, cnt);
#endif
}

static void kvm_cpu__handle_coalesced_mmio(struct kvm_cpu *cpu)
{
	if (cpu->ring) {
		while (cpu->ring->first != cpu->ring->last) {
			struct kvm_coalesced_mmio *m;
			m = &cpu->ring->coalesced_mmio[cpu->ring->first];
			kvm_cpu__emulate_mmio(cpu,
					      m->phys_addr,
					      m->data,
					      m->len,
					      1);
			cpu->ring->first = (cpu->ring->first + 1) % KVM_COALESCED_MMIO_MAX;
		}
	}
}

static DEFINE_MUTEX(task_lock);
static int task_eventfd;

static void kvm_cpu__run_task(struct kvm_cpu *cpu)
{
	u64 inc = 1;

	pr_debug("Running task %p on cpu %lu", cpu->task, cpu->cpu_id);

	/* Make sure we see the store to cpu->task */
	rmb();
	cpu->task->func(cpu, cpu->task->data);

	/* Clear task before we signal completion */
	cpu->task = NULL;
	wmb();

	if (write(task_eventfd, &inc, sizeof(inc)) < 0)
		die("Failed notifying of completed task.");
}

void kvm_cpu__run_on_all_cpus(struct kvm *kvm, struct kvm_cpu_task *task)
{
	int i, done = 0;

	pr_debug("Running task %p on all cpus", task);

	mutex_lock(&task_lock);

	for (i = 0; i < kvm->nrcpus; i++) {
		if (kvm->cpus[i]->task) {
			/* Should never happen */
			die("CPU %d already has a task pending!", i);
		}

		kvm->cpus[i]->task = task;
		wmb();

		if (kvm->cpus[i] == current_kvm_cpu)
			kvm_cpu__run_task(current_kvm_cpu);
		else
			pthread_kill(kvm->cpus[i]->thread, SIGKVMTASK);
	}

	while (done < kvm->nrcpus) {
		u64 count;

		if (read(task_eventfd, &count, sizeof(count)) < 0)
			die("Failed reading task eventfd");

		done += count;
	}

	mutex_unlock(&task_lock);
}

u64 map1 = 0x0; // BSP <- AP
u64 map2 = 0x0; // AP -> BSP
u64 cpuid1 = 0x1; // new value
//u64 cpuid2 = 0x8; // new value
u64 empty = 0x0;

u64 value = 0x0; // map
u64 new = 0x1; // new value
extern pthread_barrier_t barrier_distribute_sync;
#if defined(CONFIG_POPCORN_HYPE) && PERF_CRITICAL_DEBUG
static unsigned long cnt[MAX_POPCORN_VCPU] = { 0, 0 };
static unsigned long vm_exit_io_cnt[MAX_POPCORN_VCPU] = { 0, 0 };
#endif

extern struct timeval tstart, tstop;
// <*> -> this (2 threads intotal) -> 2
int kvm_cpu__start(struct kvm_cpu *cpu)
{
#ifdef CONFIG_POPCORN_HYPE
	int nid;
	int pophype_nid;

	//int ret;
	//struct timespec time;
	//ret = clock_gettime(CLOCK_MONOTONIC, &time);
	//uint64_t tsdiff;
	//struct timespec tsstart, tsend;

#if SHOW_POPHYPE_MIGRATION_TIME
	int tvusecs;
	struct timeval tvstart, tvstop;
#endif
	int tusecs;
	//struct timeval tstart, tstop;
	//extern struct timeval tstart, tstop;
#endif
	sigset_t sigset;

	POP_PRINTF("-----------------------------------------\n"
			"\t[%d/%d/%d] <*%lu*> %s(): vcpu thread main [START]\n"
			"\t\tkvm_run vcpu %p vcpu->kvm_run %p\n"
			"---------------------------------------\n\n",
			pop_get_nid(), getpid(), popcorn_gettid(),
			cpu->cpu_id, __func__, cpu, cpu->kvm_run);


#if defined(CONFIG_POPCORN_HYPE)
	/* testing */
	nid = pop_get_nid();
	if (nid) {
		migrate(0, NULL, NULL);
		pophype_origin_checkin_vcpu_pid(nid);
		kvm_cpu__reset_vcpu(cpu); /* vcpu states before boot */
		migrate(nid, NULL, NULL);
		popcorn_setcpuaffinity(nid); /* If not, vmx_vcpu_load haapens frequently? */
        /* Tell origin my (remote vCPU thread) pid for signal forwarding
            TODO: after migration I also need to update again */
        printf("<%lu> [%d/%d] Pophype turns out doesn't forward "
				"signals by relying on this mechanism any more\n",
				cpu->cpu_id, pop_get_nid(), getpid());
        pophype_remote_checkin_vcpu_pid(getpid());
        //printf("Make sure this thread doesn't migrate again!!!!\n");
	}
#endif


#if RUN_REMOTE_MAIN_THREAD
	pthread_barrier_wait(&barrier_distribute_sync); // THIS DOESN'T HELP or MORE
#endif
	/* Check the following again */

	sigemptyset(&sigset);
	sigaddset(&sigset, SIGALRM);

	pthread_sigmask(SIG_BLOCK, &sigset, NULL);

	/* signals has to be broadcasted */
	signal(SIGKVMEXIT, kvm_cpu_signal_handler);
	signal(SIGKVMPAUSE, kvm_cpu_signal_handler);
	signal(SIGKVMTASK, kvm_cpu_signal_handler);
	signal(SIGKVMCPUMIGRATE_BACK_W_STATE, kvm_cpu_signal_handler);
	//signal(SIGKVMCPUMIGRATE, kvm_cpu_signal_handler);
	//signal(SIGKVMCPUMIGRATE_BACK, kvm_cpu_signal_handler);
	//signal(SIGKVMCPUMIGRATE_W_STATE, kvm_cpu_signal_handler);
	//signal(SIGKVMFTCKPT, kvm_cpu_signal_handler);
	signal(SIGKVMFTRESTART, kvm_cpu_signal_handler);

	signal(SIGKVMPAUSEVANILLA, kvm_cpu_signal_handler_vanilla);
	POP_PRINTF("pophype: <%lu> registered all signal handlers\n", cpu->cpu_id);

#if defined(CONFIG_POPCORN_HYPE)
	/* DSM doesn't like this alloc/dealloc */
	if (pop_get_nid()) {
		POP_PRINTF("I'm at remote sleep 3s\n");
		sleep(3); /* HACKING */
		POP_PRINTF("[remote] go init vcpu\n");
	} else {
		POP_PRINTF("[origin] go init vcpu\n");
	}
#endif

	/* in-kernel data per-node only */
	kvm_cpu__reset_vcpu(cpu); /* init vcpu */

#if defined(CONFIG_POPCORN_HYPE)
	if (pop_get_nid()) {
		POP_PRINTF("[remote] init vcpu done\n");
	} else {
		POP_PRINTF("[origin] go init vcpu done\n");
	}
#endif

	if (cpu->kvm->cfg.single_step)
		kvm_cpu__enable_singlestep(cpu);

#ifdef CONFIG_POPCORN_HYPE

#if RUN_GUEST_KERNEL

#if DSM_ATOMIC_TEST && RUN_REMOTE_MAIN_THREAD
	pthread_barrier_wait(&barrier_atomic_op_test_start);
	POP_PRINTF("\t[%d] ||prepare to start||\n", pop_get_nid());
	sleep(30);
	POP_PRINTF("\t[%d] ||START|| DSM KERNEL-LIKE ATOMIC OP TESTING!!\n", pop_get_nid());
	if (!pop_get_nid()) { /* MASTER BSP */
		POP_PRINTF("\t[%d] BSP waits for AP'S START !!\n", pop_get_nid());
		// if old == sum, sum =  old+1
		while (!__sync_bool_compare_and_swap(&map1, cpuid1, 0x2)) {
			//;
			sched_yield();
			//asm volatile("rep; nop" ::: "memory");
		}

		POP_PRINTF("\t[%d] BSP got START from AP'S START !!\n", pop_get_nid());
		/* master got sig from ap */
		/* master signals ap an ack */
		// set map2 cpuid1
		while (!__sync_bool_compare_and_swap(&map2, empty, cpuid1)) {
			//;
			sched_yield();
			//asm volatile("rep; nop" ::: "memory");
		}
		POP_PRINTF("\t[%d] BSP sent START ACK to AP!!\n", pop_get_nid());
	} else { /* REMOTE AP */
		//set map1 cpuid1
		while (!__sync_bool_compare_and_swap(&map1, empty, cpuid1)) {
			//;
			//sched_yield();
			asm volatile("rep; nop" ::: "memory");
		}

		/* ap set it wait for masters ack sig */
		POP_PRINTF("\t[%d] AP sent START to bsp!!\n", pop_get_nid());
		// test map2
		while (!__sync_bool_compare_and_swap(&map2, cpuid1, 0x2)) {
			//;
			//sched_yield();
			asm volatile("rep; nop" ::: "memory");
		}
		POP_PRINTF("\t[%d] AP got START ACK!!\n", pop_get_nid());
	}
	POP_PRINTF("\t[%d] ||DONE|| DSM KERNEL-LIKE ATOMIC OP TESTING!!\n", pop_get_nid());
	sleep(30);
#else //!(DSM_ATOMIC_TEST && RUN_REMOTE_MAIN_THREAD)
	POP_PRINTF("\t[%d] skip ATOMIC OP TESTING!!\n", pop_get_nid());
#endif //DSM_ATOMIC_TEST && RUN_REMOTE_MAIN_THREAD

	/* remote go first just in case */
//	POP_POTENTIAL_BUG_PRINTF("pophype: testing (REMOVE ME)\n");
//	if (!pop_get_nid()) {	// || cpu->is_remote_bsp) { //This doesn't matter
//		POP_PRINTF("\t<0> sleep at origin, let remote BSP go first.\n");
//		sleep(3);
//	}
	POP_PRINTF("\t[%d/%d/%d] <%lu> RUNNING KERNEL!!\n",
			pop_get_nid(), getpid(), popcorn_gettid(), cpu->cpu_id);
#else //!RUN_GUEST_KERNEL
	/* no KVM_SET_BOOT_CPU_ID */
	cpu->is_running = 0;
#endif //RUN_GUEST_KERNEL
#endif //CONFIG_POPCORN_HYPE


#ifdef CONFIG_POPCORN_HYPE
	/*** Time it - start ***/
	if (!pop_get_nid()) {
		gettimeofday(&tstop, NULL);
		tusecs = ((tstop.tv_sec - tstart.tv_sec) * 1000000) + (tstop.tv_usec - tstart.tv_usec);
		printf("pophype: lkvm init [time] = %d ms [[[%d s]]] gettimeofday()\n",
					tusecs / 1000,
					tusecs / 1000 / 1000);
	}
	/*** Time it - end ***/

//	current_kvm_cpu->pending_vcpu_migration = false; // kill me
#endif //CONFIG_POPCORN_HYPE
	while (cpu->is_running) {
		if (cpu->needs_nmi) {
			kvm_cpu__arch_nmi(cpu);
			cpu->needs_nmi = 0;
		}

		if (cpu->task)
			kvm_cpu__run_task(cpu);

		kvm_cpu__run(cpu);

		switch (cpu->kvm_run->exit_reason) {
		case KVM_EXIT_UNKNOWN:
			break;
		case KVM_EXIT_DEBUG:
			kvm_cpu__show_registers(cpu);
			kvm_cpu__show_code(cpu);
			break;
		case KVM_EXIT_IO: {
			bool ret;
			ret = kvm_cpu__emulate_io(cpu,
						  cpu->kvm_run->io.port,
						  (u8 *)cpu->kvm_run +
						  cpu->kvm_run->io.data_offset,
						  cpu->kvm_run->io.direction,
						  cpu->kvm_run->io.size,
						  cpu->kvm_run->io.count);

			if (!ret)
				goto panic_kvm;
			break;
		}
		case KVM_EXIT_MMIO: {
			bool ret;

			/*
			 * If we had MMIO exit, coalesced ring should be processed
			 * *before* processing the exit itself
			 */
			kvm_cpu__handle_coalesced_mmio(cpu);

			ret = kvm_cpu__emulate_mmio(cpu,
						    cpu->kvm_run->mmio.phys_addr,
						    cpu->kvm_run->mmio.data,
						    cpu->kvm_run->mmio.len,
						    cpu->kvm_run->mmio.is_write);

			if (!ret)
				goto panic_kvm;
			break;
		}
		case KVM_EXIT_INTR:
			if (cpu->is_running)
				break;
			goto exit_kvm;
		case KVM_EXIT_SHUTDOWN:
			goto exit_kvm;
#ifdef CONFIG_POPCORN_HYPE
		case KVM_EXIT_FAIL_ENTRY:
			POP_PRINTF("\t[%d/%d/%d] <%lu> %s(): "
					"(KVM_EXIT_FAIL_ENTRY 9) overriten to skip\n",
					pop_get_nid(), getpid(), popcorn_gettid(),
					cpu->cpu_id, __func__);
			break;
#endif
		case KVM_EXIT_SYSTEM_EVENT:
			/*
			 * Print the type of system event and
			 * treat all system events as shutdown request.
			 */
			switch (cpu->kvm_run->system_event.type) {
			default:
				pr_warning("unknown system event type %d",
					   cpu->kvm_run->system_event.type);
				/* fall through for now */
			case KVM_SYSTEM_EVENT_RESET:
				/* Fall through for now */
			case KVM_SYSTEM_EVENT_SHUTDOWN:
				/*
				 * Ensure that all VCPUs are torn down,
				 * regardless of which CPU generated the event.
				 */
				kvm__reboot(cpu->kvm);
				goto exit_kvm;
			};
			break;
#ifdef CONFIG_POPCORN_HYPE
		case KVM_EXIT_POPHYPE_MIGRATE: { //78 // (./include/linux/kvm.h)
			nid = pop_get_nid();
			if (nid) { //migrate_back. must to 0;
				if(cpu->kvm_run->a0 > 0 && cpu->kvm_run->a0 < 10)
				{
					pophype_nid = cpu->cpu_id + cpu->kvm_run->a0 * 100;
					//printf("you mom is dead, migrate to %lu, %ld\n",cpu->kvm_run->a0,pophype_nid);
				}
				else
				{
					//printf("a0 = 0 or a0 = default\n");
					pophype_nid = (MAX_POPCORN_VCPU * 1) + cpu->cpu_id;
				}
				migrate(pophype_nid, NULL, NULL);
				popcorn_setcpuaffinity(cpu->cpu_id);
			}
			else {
			
				if(cpu->kvm_run->a0 > 0 && cpu->kvm_run->a0 < 10)
				{
                    pophype_nid = cpu->cpu_id + cpu->kvm_run->a0 * 100;
//                  printf("from YOUR HOT MOM migrate to %lu, %ld\n",cpu->kvm_run->a0,pophype_nid);
                }
				else
				{
//					printf("a0 = 0 or a0 = default\n");
					pophype_nid = (MAX_POPCORN_VCPU * 2) +  cpu->cpu_id;
				}
				migrate(pophype_nid, NULL, NULL);
				popcorn_setcpuaffinity(cpu->cpu_id);

			}
			break;
		}
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
#endif
#ifdef CONFIG_POPCORN_HYPE
		//case KVM_EXIT_SHUTDOWN: // TODO
		//	goto exit_kvm;
#endif
		default: {
			bool ret;

			ret = kvm_cpu__handle_exit(cpu);
			if (!ret)
				goto panic_kvm;
			break;
		}
		}
		kvm_cpu__handle_coalesced_mmio(cpu);
	}

#ifdef CONFIG_POPCORN_HYPE
//	POP_DBG_PRINTF(pop_get_nid(),
//		"\t<%d> %s(): vcpu thread main [END] kvm_run %lu\n",
//						pop_get_nid(), __func__, cpu->cpu_id);
	POP_PRINTF("\t[%d] %s(): vcpu thread main [END] kvm_run <%lu>\n",
						pop_get_nid(), __func__, cpu->cpu_id);
#endif
exit_kvm:
	return 0;

panic_kvm:
	return 1;
}
extern pthread_barrier_t barrier_etask_fd;
extern pthread_barrier_t barrier_kvm_cpus;
extern pthread_barrier_t barrier_kvm_cpu;
extern pthread_barrier_t barrier_kvm_cpu_done;
//DEFINE_MUTEX(vcpu_mutex);
/* <#> */
int kvm_cpu__init(struct kvm *kvm)
{

	int max_cpus, recommended_cpus, i;

	max_cpus = kvm__max_cpus(kvm);
	recommended_cpus = kvm__recommended_cpus(kvm);

	POP_DPRINTF(pop_get_nid(),
		"\t[%d/%d] %s(): arch: max_cpus %d recommended_cpus %d\n",
		pop_get_nid(), popcorn_gettid(), __func__, max_cpus, recommended_cpus);

	if (kvm->cfg.nrcpus > max_cpus) {
#ifndef CONFIG_POPCORN_HYPE
		POP_PRINTF("  # Limit the number of CPUs to %d\n", max_cpus);
#else
		POP_DPRINTF(pop_get_nid(),"\t[%d/%d] # Limit the number of CPUs to %d\n",
					pop_get_nid(), popcorn_gettid(), max_cpus);
#endif
		kvm->cfg.nrcpus = max_cpus;
	} else if (kvm->cfg.nrcpus > recommended_cpus) {
#ifndef CONFIG_POPCORN_HYPE
		POP_PRINTF("  # Warning: The maximum recommended amount of VCPUs"
				" is %d\n", recommended_cpus);
#else
		POP_DPRINTF(pop_get_nid(),"\t[%d/%d] # Warning: "
					"The maximum recommended amount of VCPUs is %d\n",
					pop_get_nid(), popcorn_gettid(), recommended_cpus);
#endif
	}

	kvm->nrcpus = kvm->cfg.nrcpus;
	POP_CPU_PRINTF(pop_get_nid(), "\t[%d/%d] %s(): kvm->nrcpus %d (final data)\n",
						pop_get_nid(), popcorn_gettid(), __func__, kvm->nrcpus);

#ifndef CONFIG_POPCORN_HYPE
	task_eventfd = eventfd(0, 0);
#else
	int __task_eventfd = INT_MIN;
	if (!pop_get_nid()) {
		//mutex_lock(&vcpu_mutex);
		task_eventfd = eventfd(0, 0);
		//mutex_unlock(&vcpu_mutex);
	} else {
		//eventfd(0, 0);
		__task_eventfd = eventfd(0, 0);
		if (__task_eventfd < 0) {
			POP_DPRINTF(pop_get_nid(),"(BUG) %d\n",
						pop_get_nid(), __func__, __task_eventfd);
			 perror("kvm_cpu_task_eventfd");
		}
	}
	pthread_barrier_wait(&barrier_etask_fd);
//	if (pop_get_nid())
	POP_PRINTF("\t[%d/%d] %s(): eventfd() task_eventfd fd %d (10/SYSC_eventfd2)\n",
						pop_get_nid(), popcorn_gettid(), __func__, task_eventfd);
#endif
	if (task_eventfd < 0) {
		pr_warning("Couldn't create task_eventfd");
		return task_eventfd;
	}

	/* Alloc one pointer too many, so array ends up 0-terminated */
#ifndef CONFIG_POPCORN_HYPE
	kvm->cpus = calloc(kvm->nrcpus + 1, sizeof(void *));
#else
	if (!pop_get_nid()) { /* only <*> */
		kvm->cpus = calloc(kvm->nrcpus + 1, sizeof(void *));

		POP_DPRINTF(pop_get_nid(), "\t<*> [%d/%d] %s(): malloc (delegated) "
				"(void*)kvm->cpus[0/%d] %p to iter all vcpus\n\n\n",
				pop_get_nid(), popcorn_gettid(), __func__,
				kvm->nrcpus - 1, kvm->cpus);
	}
	pthread_barrier_wait(&barrier_kvm_cpus);
#endif

	if (!kvm->cpus) {
		pr_warning("Couldn't allocate array for %d CPUs", kvm->nrcpus);
		return -ENOMEM;
	}

#ifndef CONFIG_POPCORN_HYPE
	for (i = 0; i < kvm->nrcpus; i++) {
		kvm->cpus[i] = kvm_cpu__arch_init(kvm, i); // TODO Jack arch
		if (!kvm->cpus[i]) {
			pr_warning("unable to initialize KVM VCPU");
			goto fail_alloc;
		}
		POP_DPRINTF(pop_get_nid(), "\t[%d/%d] %s(): "
					"(void*)kvm->cpus[%d] %p\n",
					pop_get_nid(), popcorn_gettid(),
					__func__, i, kvm->cpus[i]);
	}
#else // CONFIG_POPCORN_HYPE
	popcorn_serial_threads_start(); // 0..1..2...
	if (pop_get_nid()) {
		if (ioctl(kvm->vm_fd, KVM_SET_BOOT_CPU_ID,
					REMOTE_NEVER_BOOT_BSP_ID)) {
			/* Let remote not booting kernel */
			POP_PRINTF("\tERROR: [%d/%d] <#> %s(): fd %d "
					"KVM_SET_BOOT_CPU_ID [[[[[ FAIL ]]]]]\n",
						pop_get_nid(), popcorn_gettid(), __func__, kvm->vm_fd);
			BUG_ON(1);
		}
		POP_PRINTF("\t[%d/%d/%d] <#> %s(): REMOTE set BSP as %d\n",
				pop_get_nid(), getpid(), popcorn_gettid(),
				__func__, REMOTE_NEVER_BOOT_BSP_ID);
	}

	/* [0] <0..2> [1] <0..2> [2] <0..2> ... */
	for (i = 0; i < kvm->nrcpus; i++) {
		/* making the same fd but use the last request as its addr */
		kvm->cpus[i] = kvm_cpu__arch_init(kvm, i); // TODO Jack arch
		if (!kvm->cpus[i]) {
			pr_warning("unable to initialize KVM VCPU");
			goto fail_alloc;
		}
		POP_DPRINTF(pop_get_nid(), "\t[%d] %s(): create VCPU <%d> "
									"(void*)kvm->cpus[%d] %p\n\n",
					pop_get_nid(), __func__, i, i, kvm->cpus[i]);
	}
	popcorn_serial_threads_end();

	pthread_barrier_wait(&barrier_kvm_cpu_done);

#if 0 // 200602 removed - testing
	POP_PRINTF("cpu done sleep 5s\n\n\n");
	sleep (5);
	POP_PRINTF("keep going\n\n\n");
#endif
	// TODO - should I remove it to speed up boot process?
	/* <#> ramdomly touching vcpu page (shared with kernel) */
	for (i = 0; i < kvm->nrcpus; i++) {
		int mmap_size = PAGE_SIZE * 3;
		struct kvm_cpu *vcpu = kvm->cpus[i];
		volatile char *a = malloc(mmap_size);
		memcpy((void*)a, vcpu->kvm_run, mmap_size);
		free((void*)a);
		POP_DPRINTF(pop_get_nid(), "\t\t[%d/%d] %s(): enforce to touch "
									"kvm_vcpu_fault kvm->cpus[%d] %p\n",
					pop_get_nid(), popcorn_gettid(), __func__, i, vcpu);

		POP_DPRINTF(pop_get_nid(), "\t\t[%d/%d] %s():  "
					"ckpt_done[%d] = false\n",
					pop_get_nid(), popcorn_gettid(), __func__, i);
		ckpt_done[i] = false;
		restart_done[i] = false;
		all_done[i] = false;
	}
	ipc_thread_ckpt_done = false;

#if 0 // 200602 removed - testing
	POP_PRINTF("enforced to touch *vcpu done sleep 5s\n\n\n");
	sleep (5);
	POP_PRINTF("keep going\n\n\n");
#endif
#endif // CONFIG_POPCORN_HYPE Done

	return 0;

fail_alloc:
	for (i = 0; i < kvm->nrcpus; i++)
		free(kvm->cpus[i]);
	return -ENOMEM;
}
base_init(kvm_cpu__init);

// <*> ->
int kvm_cpu__exit(struct kvm *kvm)
{
	int i, r;
	void *ret = NULL;

	kvm_cpu__delete(kvm->cpus[0]);
	kvm->cpus[0] = NULL;

	kvm__pause(kvm);
	for (i = 1; i < kvm->nrcpus; i++) {
		if (kvm->cpus[i]->is_running) {
			pthread_kill(kvm->cpus[i]->thread, SIGKVMEXIT);
			if (pthread_join(kvm->cpus[i]->thread, &ret) != 0)
				die("pthread_join");
			kvm_cpu__delete(kvm->cpus[i]);
		}
		if (ret == NULL)
			r = 0;
	}
	kvm__continue(kvm);

	free(kvm->cpus);

	kvm->nrcpus = 0;

	close(task_eventfd);

	return r;
}
