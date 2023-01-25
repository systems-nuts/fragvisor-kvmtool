#ifndef _POPCORN_H_
#define _POPCORN_H_

#define _GNU_SOURCE /* pophype: order matters */
#include <sched.h>   /* cpu_set_t , CPU_SET, setaff */
#include <pthread.h> /* pthread_t */
#include <stdlib.h>

#include <sched.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

#include <stdbool.h>

//#define true 1
//#define false 0

#define TRUE 1
#define FALSE 0


#ifdef _VERBOSE
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_POPCORN_NODES	32

/**
 * Options to help performance evaluation
 */
//#define _POPCORN_ALIGN_VARIABLES
//#define _POPCORN_ALIGN_HEAP
#undef _POPCORN_ALIGN_VARIABLES
#undef _POPCORN_ALIGN_HEAP
#undef _POPCORN_PROFILE_REGION
#undef _POPCORN_RELEASE_OWNERSHIP
#undef _POPCORN_MIGRATE_SCHEDULE
#ifndef PAGE_SHIFT
#define PAGE_SHIFT	12
#endif
#ifndef PAGE_SIZE
#define PAGE_SIZE	(1 << PAGE_SHIFT)
#endif
#ifndef PAGE_MASK
#define PAGE_MASK	(~(PAGE_SIZE-1))
#endif

#ifdef __x86_64__
#define SYSCALL_POPCORN_MIGRATE 330
#define SYSCALL_POPCORN_PROPOSE_MIGRATION   331
#define SYSCALL_POPCORN_GET_THREAD_STATUS   332
#define SYSCALL_POPCORN_GET_NODE_INFO   333
#define SYSCALL_GETTID 186
#elif __aarch64__
#define SYSCALL_POPCORN_MIGRATE 285
#define SYSCALL_POPCORN_PROPOSE_MIGRATION   286
#define SYSCALL_POPCORN_GET_THREAD_STATUS   287
#define SYSCALL_POPCORN_GET_NODE_INFO   288
#define SYSCALL_GETTID 178
#elif __powerpc64__
#define SYSCALL_POPCORN_MIGRATE 379
#define SYSCALL_POPCORN_PROPOSE_MIGRATION   380
#define SYSCALL_POPCORN_GET_THREAD_STATUS   381
#define SYSCALL_POPCORN_GET_NODE_INFO   382
#define SYSCALL_GETTID 207
#else
#error Does not support this architecture
#endif

/*****
 * Pophype debug tools
 */
int popcorn_gettid(void);
void print_affinity();

void popcorn_setaffinity(int cpu)
{
    cpu_set_t cpuset;

    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
	/* 0 - cpu0: 1 0 0...,  1 - cpu1: 0 1 0...,  2 - cpu2: 0 0 1... */

    /*
     * cpu affinity for the calling thread
     * first parameter is the pid, 0 = calling thread
     * second parameter is the size of your cpuset
     * third param is the cpuset in which your thread will be
     * placed. Each bit represents a CPU
     */
    sched_setaffinity(0, sizeof(cpuset), &cpuset);
}

/**
 * Get the current status of the current thread regarding migration
 *
 * return 0 on fetching the status successfully.
 * return non-zero otherwise.
 *  EINVAL: invalid @status
 */
struct popcorn_thread_status {
	int current_nid;
	int proposed_nid;
	int peer_nid;
	int peer_pid;
};

int popcorn_get_status(struct popcorn_thread_status *status);

static inline int popcorn_current_nid(void) {
	struct popcorn_thread_status status;
	if (popcorn_get_status(&status)) return -1;

	return status.current_nid;
}


/**
 * Propose to migrate @tid to @nid
 *
 * If @tid 0 means to propose the migration for the current one.
 *
 * return 0 on success, return non-zero otherwise.
 *  ENOENT: no thread corresponding to @tid
 *  EINVAL: invalid @nid
 */
int popcorn_propose_migration(int tid, int nid);



/**
 * Allocate memory aligned to the page boundary
 */
#if defined(_POPCORN_ALIGN_VARIABLES) || defined(_POPCORN_ALIGN_HEAP)
#define ALIGN_TO_PAGE __attribute__ ((aligned (PAGE_SIZE)))
#else
#define ALIGN_TO_PAGE
#endif


#ifdef _POPCORN_ALIGN_HEAP
#include <malloc.h>
static inline void *popcorn_malloc(size_t size) {
	void *p = NULL;
	posix_memalign(&p, PAGE_SIZE, (size + PAGE_SIZE - 1) & PAGE_MASK);
	return p;
}
#else
static inline void *popcorn_malloc(size_t size) {
	return malloc(size);
}
#endif /* _POPCORN_ALIGN_HEAP */


/**
 * Frequently used configurations
 */
#ifdef _POPCORN_ALIGN_VARIABLES
struct __popcorn_config_t {
	int configs[1024];
} ALIGN_TO_PAGE;
#else
struct __popcorn_config_t {
	int configs[2];
};
#endif /* _POPCORN_ALIGN_VARIABLES */

#define POPCORN_DEFINE_CONFIGS()	\
	struct __popcorn_config_t __popcorn_configs = { \
		.configs = {0}, \
	}
#define POPCORN_CONFIG_CORES_PER_NODE	__popcorn_config.configs[0]
#define POPCORN_CONFIG_NODES			__popcorn_config.configs[1]


/**
 * Return the tid of the current context. This is a wrapper for
 * syscall(SYS_gettid)
 */
//int popcorn_gettid();


/**
 * Get the popcorn node information
 */
enum popcorn_node_status {
	POPCORN_NODE_OFFLINE = 0x00,
	POPCORN_NODE_ONLINE = 0x01,
};

enum popcorn_arch_types {
	POPCORN_NODE_UNKNOWN = -1,
	POPCORN_NODE_AARCH64 = 0,
	POPCORN_NODE_X86 = 1,
	POPCORN_NODE_PPC64 = 2,
};

struct popcorn_node_info {
	unsigned int status;
	int arch;
	int distance;
};

int popcorn_get_node_info(int *current_nid, struct popcorn_node_info *info);


/**
 *
 */
#define POPCORN_MIGRATE_START(tid) \
	popcorn_migrate((tid) / POPCORN_CONFIG_CORES_PER_NODE)

#define POPCORN_MIGRATE_END() \
	popcorn_migrate(0)


#ifdef _POPCORN_PROFILE_REGION
#define PRCTL_TRACE_AUX		48
#include <sys/prctl.h>
static inline void popcorn_region_id(int id) {
	prctl(PRCTL_TRACE_AUX, id);
}
#else
static inline void popcorn_region_id(int id) {}
#endif /* _POPCORN_PROFILE_REGION */


#ifdef _POPCORN_RELEASE_OWNERSHIP
#include <sys/mman.h>
#define MADVISE_RELEASE 18
static inline int popcorn_release_ownership(void *start, size_t len) {
	return madvise(start, len, MADVISE_RELEASE);
}
#else
static inline int popcorn_release_ownership(void *start, size_t len) { return -22; /* EINVAL */ }
#endif /* _POPCORN_RELEASE_OWNERSHIP */


#define POPCORN_MIGRATE_REGION_START(tid,R) \
	POPCORN_MIGRATE_START(tid); \
	popcorn_region_id(R);

#define POPCORN_MIGRATE_REGION_END() \
	popcorn_region_id(0); \
	POPCORN_MIGRATE_END();


#ifdef _OPENMP
#include <omp.h>

#ifdef _POPCORN_MIGRATE_SCHEDULE
#define POPCORN_OMP_MIGRATE_START(R) \
{	\
	popcorn_migrate_schedule(R, omp_get_popdorn_tid()); \
	popcorn_region_id(R); \


#define POPCORN_OMP_MIGRATE_END() \
	popcorn_region_id(0); \
	popcorn_migrate(0);
}

#else /* SCHEDULE_THREADS */
#define POPCORN_OMP_MIGRATE_START(R) \
{ \
	popcorn_migrate(omp_get_thread_num() / POPCORN_CONFIG_CORES_PER_NODE); \
	popcorn_region_id(R);

#define POPCORN_OMP_MIGRATE_END() \
	popcorn_region_id(0); \
	popcorn_migrate(0); \
}
#endif /* SCHEDULE_THREADS */
#endif /* _OPENMP */
#ifdef __cplusplus
}
#endif
#endif
