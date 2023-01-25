/*
 * pop_utils.h
 * Copyright (C) 2019 jackchuang <jackchuang@echo5>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef POP_UTILS_H
#define POP_UTILS_H

#include <errno.h>
#include <time.h>
#include <sys/time.h> /* gettimeofday */
#include <stdbool.h>

#include <pthread.h>

#include "migrate.h"

extern struct kvm_cpu *gvcpu[];

#define CONFIG_POPCORN_HYPE
#ifdef CONFIG_POPCORN_HYPE
#define POPHYPE_USR_SANITY_CHECK 1
#else
#define POPHYPE_USR_SANITY_CHECK 0
#endif

/* perf # */
#define PERF_EXP 1 /* 1 when init time, etc. */
#define PERF_CRITICAL_DEBUG 1 /* 1: kill performance 0: for running exp*/ // 0 when init time, etc.

/* perf # */
#define SHOW_POPHYPE_MIGRATION_TIME 0 /* show lkvm migration time!!!! If you are checking migration before/after time plz set this as 0. 1: pophype migration time */
#define SHOW_POPHYPE_FT_TIME 1 /* ft ckpt/restart time */
#define POPHYPE_FT_TO_MEM 0 /* ft ckpt/restart to/from mem/disk */

/*** Abstract(remove) all printf() ***/
#define POP_DPRINTF(nid, ...)
//#define POP_DPRINTF(nid, ...) pop_printf(nid, __VA_ARGS__) // verbose debug: ft
#define POP_PRINTF(...)
//#define POP_PRINTF(...) printf(__VA_ARGS__) // verbose debug: ft
#define POP_DUMP_STACK_PRINTF(...) //printf(__VA_ARGS__)

/* this is for karim's vhost work */
#define VHOSTPK(...) printf(__VA_ARGS__)

/* If bugs, check these recently chaged places */
#define POP_POTENTIAL_BUG_PRINTF(...) //printf(__VA_ARGS__)

/*** ERROR ***/
#define POP_SANI_PRINTF(nid, ...) //pop_printf(nid, __VA_ARGS__)
#define POP_ERR_PRINTF(nid, ...) //pop_printf(nid, __VA_ARGS__)

//#define POP_MEM_PRINTF(nid, ...) //pop_printf(nid, __VA_ARGS__)
#define POP_MEM_PRINTF(nid, ...) pop_printf(nid, __VA_ARGS__)
#define POP_CPU_PRINTF(nid, ...) //pop_printf(nid, __VA_ARGS__)
#define POP_DBG_PRINTF(nid, ...) //pop_printf(nid, __VA_ARGS__)
//#define POP_DBG_PRINTF(nid, ...) pop_printf(nid, __VA_ARGS__) // debug: ft
/* debug verbose */
#define POP_VERB_PRINTF(nid, ...)
#define POP_FS_PRINTF(nid, ...)
#define POP_ARG_PRINTF(...) //printf(__VA_ARGS__)
/* VM entry/exit info */
#define POP_VM_PRINTF(nid, ...) //pop_printf(nid, __VA_ARGS__)
/* MEM */
#define POP_ADDR_PRINTF(nid, ...)

/* PRINTFs() */
#if PERF_EXP
#define POPHYPE_DEBUG 0
#define POPHYPE_MIGRATION_DEBUG 0
//#define POPHYPE_MIGRATION_DEBUG 1
#define PHMIGRATEPRINTF(...)
//#define PHMIGRATEPRINTF(...) printf(__VA_ARGS__)
#define MEMPRINTF(...)
#define PRINTF(...)
#define POP_DBGPRINTF(...)
//#define POP_DBGPRINTF(...) printf(__VA_ARGS__) // debug: ft
#else
#define POPHYPE_DEBUG 1
#define POPHYPE_MIGRATION_DEBUG 1
#define PHMIGRATEPRINTF(...) printf(__VA_ARGS__)
#define MEMPRINTF(...) printf(__VA_ARGS__)
#define PRINTF(...) printf(__VA_ARGS__)
#define POP_DBGPRINTF(...) printf(__VA_ARGS__)
#endif

/* serial port */
#define DEBUG_SERIAL 0
#define CONSOLEPRINTF(...)	/* If on, bug also appears due to "iir |= UART_IIR_THRI;" */
//#define CONSOLEPRINTF(...) printf(__VA_ARGS__)

// 0: for debugging/developing 1: for normal lkvm
// 0: stop running guest kernel 1: run guest kernel
#define RUN_GUEST_KERNEL 1

#define DSM_ATOMIC_TEST 0


/* Match with kernel */
/* Pophype migration */
#define POPHYPE_MIGRATE_BACK -78
#define MAX_POPCORN_VCPU 32 /* Assumption */

/* host-net
 * 0: cannot ping
 * 1: debugging dies before rootfs
 */
#define HOST_NET 1

/* Distributed: mirror states on remote nodes */
#define RUN_REMOTE_MAIN_THREAD 1

#define POPHYPE_FORCE_TOUCH_MEM 0

/* Time */
#define BILLION 1000000000L

#define HOST 0 /* host nid */
#define VCPU_FD_BASE 11
//#define MAX_POPCORN_NODES 8

/* Global info */
extern int nodes;
extern int nthreads; /* Not used - only used in ./builtin-run.c & ./main.c */

extern int my_nid; /* don't use my_nid but pop_get_nid() */
// kvm->cfg.nrcpus = kvm->cfg.nodes[0] + kvm->cfg.nodes[1];
extern int gcpus[MAX_POPCORN_NODES]; // aka kvm->cfg.nodes[0/1/2]

/* serial */
//#define sync ;
//extern int popcorn_serial;
//void popcorn_serial_start(void);
//void popcorn_serial_end(void);
void popcorn_serial_threads_start(void);
void popcorn_serial_threads_end(void);

/* debug */
void dump_stack(void);

// Usage:
//	pop_printf(pop_get_nid(), "\t\t<%d> %s():\n", pop_get_nid(), __func__);
void popcorn_setcpuaffinity(int cpu);

int vcpuid_to_nid(int vcpu_id);

void pop_printf(int current_node, const char *fmt, ...);
void pop_dbg_printf(int current_node, const char *fmt, ...);
//void pop_printf(const char *fmt, ...);
//char *vstrcat(char *first, ...);
void pop_die(int a, char *b);
void pop_test(void);
void pop_distri(int total_nodes);
void pop_distri_main_thread(void);
int pop_cluster_state_update(void);

int pop_get_nid(void);

bool popcorn_broadcast_cpu_table(int *cpus);
bool pophype_remote_checkin_vcpu_pid(int pid);
bool pophype_origin_checkin_vcpu_pid(int from_nid);
bool pophype_vcpu_migrate_trigger(int vcpu_id);
bool pophype_prepare_vcpu_migrate(int vcpu_id);

/* echo: turn it on (uncomment), mir: turn it off (comment out) */
//extern int popcorn_gettid(void); // if no, compiler warn

/* Sync */
// Usage pthread_barrier_wait(&);
//TODO...
extern pthread_barrier_t barrier_vfio_malloc;
extern pthread_barrier_t barrier_vfio_heap;
extern pthread_barrier_t barrier_container_init;
extern pthread_barrier_t barrier_reserve_region_done;

extern pthread_barrier_t barrier_run_exit;

extern pthread_barrier_t barrier_run_destory;


extern pthread_barrier_t barrier_popcorn_serial_threads;
extern pthread_barrier_t barrier_popcorn_serial_threads_start;
#endif /* !POP_UTILS_H */
