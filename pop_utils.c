/*
 * popcorn_utils.c
 * Copyright (C) 2019 jackchuang <jackchuang@echo5>
 *
 * Distributed under terms of the MIT license.
 */
#include <popcorn/utils.h>

#include <err.h>
#include <fcntl.h>
#include <linux/kvm.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <unistd.h>

#include <stdarg.h>

#include <execinfo.h> // for dump_stack()

#include <kvm/util.h> // for BUG_ON()

#include "migrate.h"

#include <stdatomic.h>

//#define JACK(...) POP_DPRINTF(__VA_ARGS__)
//#define JACK(...)

/* ft */
struct kvm_cpu *gvcpu[255];

int pop_msg_nodes = 0;
//int current_node = 1;

void popcorn_setcpuaffinity(int cpu)
{
	cpu_set_t cpuset;
    CPU_ZERO(&cpuset);       //clears the cpuset
    CPU_SET(cpu , &cpuset); //set CPU X on cpuset
    sched_setaffinity(0, sizeof(cpuset), &cpuset);
    PRINTF("[%d] I'm running on <%d>\n", popcorn_gettid(), cpu);
}

/* kernel version: popcorn_vcpuid_to_nid() */
int vcpuid_to_nid(int vcpu_id)
{
    if (gcpus[vcpu_id] >= 0) {
		POP_PRINTF("\t\ttrans: vcpu_id <%d> is on nid [%d]\n",
						vcpu_id, gcpus[vcpu_id]);
		return gcpus[vcpu_id];
	}
    BUG_ON(-1);
	return -1;
}

void dump_stack(void) {
    void* callstack[128];
    int i, frames = backtrace(callstack, 128);
    char** strs = backtrace_symbols(callstack, frames);
    POP_DUMP_STACK_PRINTF("dump_stack():\n");
    for (i = 0; i < frames; ++i) {
        POP_DUMP_STACK_PRINTF("%s\n", strs[i]);
    }
    free(strs);
    //backtrace();
    //backtrace_symbols();
}


void pop_printf(int current_node, const char *fmt, ...)
{
#ifdef CONFIG_POPCORN_HYPE
    va_list args;
	char buffer[4096];

	va_start(args, fmt);
	//sprintf(buffer, fmt, args);
	vsprintf(buffer, fmt, args);

    if (current_node > 0)
        migrate(0, NULL, NULL);

    asm volatile("" ::: "memory");
    //va_start(args, fmt); // correct
    //vprintf(fmt, args); // correct
	printf("%s", buffer);

    fflush(stdout);
    //va_end(args);
    asm volatile("" ::: "memory");

    if (current_node > 0)
        migrate(current_node, NULL, NULL);

	va_end(args);
#endif
}

void pop_die(int a, char *b)
{
	/* TODO: tell if remote */
    migrate(0, NULL, NULL);
//    err(1, (const char *)b);
	printf("a %d b %s\n", a, b);
    exit(-1);
}

int pop_get_nid(void)
{
    int current_nid;
	struct popcorn_node_info nodes_info[MAX_POPCORN_NODES];
    if (popcorn_get_node_info(&current_nid, nodes_info)) {
        //perror(" Cannot retrieve the nodes' information");
		//BUG();
		return -EINVAL;
    }
	return current_nid;
}

void pop_test(void)
{
/* =================================================== */
	int a = -99;
	//POP_PRINTF("my nid is ???\n");
	a = popcorn_current_nid();
	POP_PRINTF("my nid is %d\n", a);

	POP_PRINTF("[%d/%d/%d] <test> 08 IS AT ORIGIN!!!!!!\n", pop_get_nid(), getpid(), popcorn_gettid());

    migrate(1, NULL, NULL);
	POP_PRINTF("[%d/%d/%d] <test> 08 IS AT REMOTE!!!!!!\n", pop_get_nid(), getpid(), popcorn_gettid());
	// popcorn_gettid() == syscall(SYSCALL_GETTID);

	a = popcorn_current_nid();
    migrate(0, NULL, NULL);

	POP_PRINTF("my nid is %d DONE\n", a);
/* =================================================== */
}

/* migration in order. This makes remote nodes to register remote_tgid[] in the kernel */
void pop_distri(int total_nodes)
{
/* =================================================== */
	int i, a = -99;

	for (i = 0; i < total_nodes; i++) {
		if (pop_get_nid() == i)
			continue;

		a = popcorn_current_nid();
		POP_PRINTF("%s(): my nid is %d total_nodes %d\n",
									__func__, a, total_nodes);

		POP_PRINTF("[%d/%d/%d] %s(): 08 IS ON %s!!!!!!\n",
				pop_get_nid(), getpid(), popcorn_gettid(),
				__func__, !pop_get_nid() ? "ORIGIN" : "REMOTE");
		migrate(i, NULL, NULL);
		POP_PRINTF("[%d/%d/%d] %s(): 08 IS ON %s!!!!!!\n",
				pop_get_nid(), getpid(), popcorn_gettid(),
				__func__, !pop_get_nid() ? "ORIGIN" : "REMOTE");

		a = popcorn_current_nid();
		migrate(0, NULL, NULL);
	}

/* =================================================== */
}

static const char *arch_sz[] = {
    "unknown",
    "arm64",
    "x86-64",
    "ppc64le",
    NULL,
};
int pop_cluster_state_update(void)
{
    struct popcorn_thread_status status;
    struct popcorn_node_info nodes[MAX_POPCORN_NODES];
    int i;
    int current_nid;

    /* Inquiry the entire rack's status */
    //POP_DPRINTF(0, "** Nodes' status  **\n"); // unknow yet
    //POP_PRINTF(0, "** Nodes' status  **\n"); // unknow yet
    if (popcorn_get_node_info(&current_nid, nodes)) {
        perror(" Cannot retrieve the nodes' information");
        return -EINVAL;
    }
    POP_DPRINTF(pop_get_nid(), "[%d/%d] - Currently at node %d\n",
							pop_get_nid(), getpid(), current_nid);
    for (i = 0; i < MAX_POPCORN_NODES; i++) {
        if (nodes[i].status != POPCORN_NODE_ONLINE) continue;
        POP_DPRINTF(pop_get_nid(), "[%d] node%d: %s\n",
			pop_get_nid(), i, arch_sz[nodes[i].arch + 1]);
		pop_msg_nodes++;
    }

	POP_PRINTF(" - pop_msg_nodes %d\n", pop_msg_nodes); /* TODO use it */

    /* Query the current thread's migration status */
    POP_DPRINTF(pop_get_nid(), "\n[%d] ** Current thread status **\n", pop_get_nid());
    if (popcorn_get_status(&status)) {
        perror(" Cannot retrieve the current thread's status");
        return -EINVAL;
    }
    POP_DPRINTF(pop_get_nid(), "[%d] - currently at node %d\n", pop_get_nid(), status.current_nid);
    POP_DPRINTF(pop_get_nid(), "[%d] - migration destination is ", pop_get_nid());
    if (status.proposed_nid == -1) {
        POP_DPRINTF(pop_get_nid(), "[%d] not proposed\n", pop_get_nid());
    } else {
        POP_DPRINTF(pop_get_nid(), "[%d] proposed to %d\n", pop_get_nid(), status.proposed_nid);
    }


#if 0
    int dest;
    srand(time(NULL));
    dest = rand() % MAX_POPCORN_NODES;

    /* Propose to migrate this thread to 1 */
    POP_DPRINTF(pop_get_nid(), "\n** Migration proposal test **\n");
    POP_DPRINTF(pop_get_nid(), " - Propose to migrate this to node %d\n", dest);
    popcorn_propose_migration(0, dest);

    popcorn_get_status(&status);
    POP_DPRINTF(pop_get_nid(), " - migration destination is ");
    if (status.proposed_nid == -1) {
        POP_DPRINTF(pop_get_nid(), "not proposed\n");
    } else {
        POP_DPRINTF(pop_get_nid(), "proposed to %d (%s)\n", status.proposed_nid,
            dest == status.proposed_nid ? "PASSED" : "FAILED");
    }
#endif
    return 0;
}

void pop_distri_main_thread(void)
{
/* =================================================== */
//	int a = -99;
	POP_PRINTF("Distributing main thread to remote node 1\n");
//	a = popcorn_current_nid();
//	POP_PRINTF("my nid is %d\n", a);

    migrate(1, NULL, NULL);
//	sleep(9999999);
	//sleep(10);
//	a = popcorn_current_nid();
//    migrate(0, NULL, NULL);

//	POP_PRINTF("my nid is %d DONE\n", a);
/* =================================================== */
}

/* Origin broadcasts cpu table to remote nodes  */
#include <sys/syscall.h>
#include <stdbool.h>
#ifdef __x86_64__
#define SYSCALL_POPCORN_BROADCAST_CPU_TABLE 370
#define SYSCALL_POPCORN_FALSE_SHARE 375
#define SYSCALL_POPCORN_TRUE_SHARE 376
#define SYSCALL_POPCORN_NO_SHARE 377
#define SYSCALL_POPHYPE_MIGRATE_ON_HOSTUSR 381 //TODO rename
#define SYSCALL_POPHYPE_REMOTE_CHECKIN_VCPU_PID 382
#define SYSCALL_POPHYPE_ORIGIN_CHECKIN_VCPU_PID 383
#define SYSCALL_POPHYPE_VCPU_MIGRATION_TRIGGER 384
#elif __aarch64__
#define SYSCALL_POPCORN_BROADCAST_CPU_TABLE 370
#define SYSCALL_POPCORN_FALSE_SHARE 375
#define SYSCALL_POPCORN_TRUE_SHARE 376
#define SYSCALL_POPCORN_NO_SHARE 377
#define SYSCALL_POPHYPE_MIGRATE_ON_HOSTUSR 381 //TODO rename
#define SYSCALL_POPHYPE_REMOTE_CHECKIN_VCPU_PID 382
#define SYSCALL_POPHYPE_ORIGIN_CHECKIN_VCPU_PID 383
#define SYSCALL_POPHYPE_VCPU_MIGRATION_TRIGGER 384
#elif __powerpc64__
#define SYSCALL_POPCORN_BROADCAST_CPU_TABLE 390
#define SYSCALL_POPCORN_FALSE_SHARE 395
#define SYSCALL_POPCORN_TRUE_SHARE 396
#define SYSCALL_POPCORN_NO_SHARE 397
#define SYSCALL_POPHYPE_MIGRATE_ON_HOSTUSR 401 //TODO rename
#define SYSCALL_POPHYPE_REMOTE_CHECKIN_VCPU_PID 402
#define SYSCALL_POPHYPE_ORIGIN_CHECKIN_VCPU_PID 403
#define SYSCALL_POPHYPE_VCPU_MIGRATION_TRIGGER 404
#else
#error Does not support this arch
#endif
/* e.g. propose_migration
 * x86: ./arch/x86/include/generated/uapi/asm/unistd_64.h
 *		./arch/x86/include/generated/asm/syscalls_64.h
 *		./arch/x86/entry/syscalls/syscall_64.tbl
 * arm: ./include/uapi/asm-generic/unistd.h
 * x86 & arm func: ./include/linux/syscalls.h
 * power: ./arch/powerpc/include/uapi/asm/unistd.h
 *		func: ./arch/powerpc/include/asm/systbl.h
 * .c
 */
bool popcorn_broadcast_cpu_table(int *cpus)
{
	POP_PRINTF("broadcast_cpu syscall(%d)\n", SYSCALL_POPCORN_BROADCAST_CPU_TABLE);
	BUG_ON(pop_get_nid());
	return syscall(SYSCALL_POPCORN_BROADCAST_CPU_TABLE, cpus);
}

bool pophype_remote_checkin_vcpu_pid(int pid)
{
	POP_PRINTF("checkin origin vcpu pid syscall(%d)\n",
				SYSCALL_POPHYPE_REMOTE_CHECKIN_VCPU_PID);
	BUG_ON(!pop_get_nid());
	return syscall(SYSCALL_POPHYPE_REMOTE_CHECKIN_VCPU_PID, pid);
}

bool pophype_origin_checkin_vcpu_pid(int pid)
{
	POP_PRINTF("checkin origin vcpu pid syscall(%d)\n",
				SYSCALL_POPHYPE_ORIGIN_CHECKIN_VCPU_PID);
	BUG_ON(pop_get_nid());
	return syscall(SYSCALL_POPHYPE_ORIGIN_CHECKIN_VCPU_PID, pid);
}

bool pophype_vcpu_migrate_trigger(int vcpu_id)
{
	POP_PRINTF("trigger vcpu migrationsyscall(%d)\n",
				SYSCALL_POPHYPE_VCPU_MIGRATION_TRIGGER);
	//BUG_ON(!pop_get_nid());
	return syscall(SYSCALL_POPHYPE_VCPU_MIGRATION_TRIGGER, vcpu_id);
}

/* Retrive vCPU kernel states for pophype migration */
bool pophype_prepare_vcpu_migrate(int vcpu_id)
{
	POP_PRINTF("%s(): syscall(%d) - save current vcpu<%d> states\n",
				__func__, SYSCALL_POPHYPE_MIGRATE_ON_HOSTUSR, vcpu_id);
	//BUG_ON(!pop_get_nid());
	return syscall(SYSCALL_POPHYPE_MIGRATE_ON_HOSTUSR, vcpu_id);
}



/* nodes */
/* don't use my_nid but pop_get_nid() */
int popcorn_serial_threads = 0;

_Atomic int acnt;
#if 0
_Atomic int a;
atomic_init(&a, 42);
atomic_store(&a, 5);
int b = atomic_load(&a);
POP_PRINTF("b = %i\n", b);
#endif
pthread_barrier_t barrier_popcorn_serial_threads;
pthread_barrier_t barrier_popcorn_serial_threads_start;
void popcorn_serial_threads_start(void)
{
	int _my_nid = pop_get_nid();
	//POP_PRINTF("[%d/%d] popcorn_serial_threads ticket %d (=0)  nodes = %d nodes - 1 = %d\n",
	//		_my_nid, popcorn_gettid(), popcorn_serial_threads, nodes, nodes - 1); // pop_get_nid()
	POP_PRINTF("\t\t\t[%d/%d] acnt %d (=0)  total nodes = %d (0~%d)\n",
			_my_nid, popcorn_gettid(), atomic_load(&acnt), nodes, nodes - 1); // pop_get_nid()

	//while (popcorn_serial_threads > 0)
	//	;
	pthread_barrier_wait(&barrier_popcorn_serial_threads_start);

	if (!_my_nid) /* origin */
		pthread_barrier_init(&barrier_popcorn_serial_threads, NULL, nodes);

	POP_PRINTF("\t\t\t[%d/%d] acnt %d (=0) ready go spin\n",
			_my_nid, popcorn_gettid(), atomic_load(&acnt)); // pop_get_nid()

	//while (popcorn_serial_threads < _my_nid) /* origin first */
	while (atomic_load(&acnt) < _my_nid) /* origin first */
		; /* 0..1..2... let go */

	POP_PRINTF("\t\t\tpopcorn serial [%d/%d]: my turn\n",
					_my_nid, popcorn_gettid());
}

void popcorn_serial_threads_end(void)
{
	//POP_PRINTF("[%d/%d] popcorn_serial_threads %d (+1)  nodes = %d nodes - 1 = %d\n",
	//		pop_get_nid(), popcorn_gettid(), popcorn_serial_threads, nodes, nodes - 1); // pop_get_nid()
	POP_PRINTF("\t\t\t[%d/%d] ***acnt %d (+1)***  total nodes = %d (0~%d)\n",
			pop_get_nid(), popcorn_gettid(), atomic_load(&acnt), nodes, nodes - 1); // pop_get_nid()
	//if (popcorn_serial_threads == nodes - 1) { /* last */
	if (pop_get_nid() == nodes - 1) { /* last */
		//popcorn_serial_threads = 0;
		atomic_store(&acnt, 0);
		pthread_barrier_init(&barrier_popcorn_serial_threads_start, NULL, nodes);
		pthread_barrier_wait(&barrier_popcorn_serial_threads);
	} else {
		//popcorn_serial_threads++;
		atomic_fetch_add_explicit(&acnt, 1, memory_order_release);
		pthread_barrier_wait(&barrier_popcorn_serial_threads);
	}
}

/* thread */
//int popcorn_serial_threads = 0;
//pthread_barrier_t barrier_popcorn_serial_threads;
//void popcorn_serial_threads_start(void)
//{
//	POP_PRINTF("\t\t[%d/%d] popcorn_serial_threads %d  nodes = %d nodes - 1 = %d\n",
//		my_nid, popcorn_gettid(), popcorn_serial_threads, nodes, nodes - 1); // pop_get_nid()
//	if (!thread_nid) /* TODO host thread */
//		pthread_barrier_init(&barrier_popcorn_serial_threads, NULL, nodes);
//
//	while (popcorn_serial_threads > 0)
//		;
//
//	while (popcorn_serial_threads < my_nid) /* origin first */
//		; /* 0..1..2... let go */
//}
//
//void popcorn_serial_threads_end(void)
//{
//	POP_PRINTF("\t\t[%d/%d] popcorn_serial_threads %d  nodes = %d nodes - 1 = %d\n",
//			my_nid, popcorn_gettid(), popcorn_serial_threads, nodes, nodes - 1); // pop_get_nid()
//	if (popcorn_serial_threads == nodes - 1) { /* last */
//		popcorn_serial_threads = 0;
//		pthread_barrier_wait(&barrier_popcorn_serial_threads);
//	} else {
//		popcorn_serial_threads++;
//		pthread_barrier_wait(&barrier_popcorn_serial_threads);
//	}
//}



