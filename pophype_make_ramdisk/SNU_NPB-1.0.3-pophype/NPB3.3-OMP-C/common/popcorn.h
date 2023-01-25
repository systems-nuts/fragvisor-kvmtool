#ifndef _POPCORN_MIGRATE_H_
#define _POPCORN_MIGRATE_H_

#include <stdlib.h>

#define MAX_POPCORN_NODES	32
#define PAGE_SIZE	(4096)

extern int __config__[];

#ifdef SEPARATE_VARIABLES
#define ALIGN_PAGE	__attribute__ ((aligned (PAGE_SIZE)))

#define POPCORN_DEFINE_CONFIGS		int ALIGN_PAGE __config__[1024] = {0}
#define POPCORN_CONFIG_CORES_PER_NODE	__config__[0]
#define POPCORN_CONFIG_NODES			__config__[1]
#else
#define ALIGN_PAGE

#define POPCORN_DEFINE_CONFIGS		int __config__[2]
#define POPCORN_CONFIG_CORES_PER_NODE	__config__[0]
#define POPCORN_CONFIG_NODES			__config__[1]
#endif

void migrate_schedule(size_t region,
                      int popcorn_tid,
                      void (*callback)(void *),
                      void *callback_data);

void migrate(int popcorn_tid,
			  void (*callback)(void *),
			  void *callback_data);


#ifdef PROFILE_REGION
#define PRCTL_TRACE_AUX		48

#include <sys/prctl.h>
static inline void popcorn_omp_id(int id)
{
	prctl(PRCTL_TRACE_AUX, id);
}
#else
static inline void popcorn_omp_id(int id) {}
#endif


#ifdef _OPENMP
#include <omp.h>

#ifdef SCHEDULE_THREADS
#define POPCORN_OMP_MIGRATE_START(R) \
{	\
	const int _tid = omp_get_popcorn_tid(); \
	migrate_schedule(R, _tid, NULL, NULL); \
	popcorn_omp_id(R); \


#define POPCORN_OMP_MIGRATE_END() \
	popcorn_omp_id(0); \
	migrate(0, NULL, NULL); \
}

#else /* SCHEDULE_THREADS */
#define POPCORN_OMP_MIGRATE_START(R) \
{ \
	const int _tid = omp_get_thread_num(); \
	if (_tid / POPCORN_CONFIG_CORES_PER_NODE) \
		migrate(_tid / POPCORN_CONFIG_CORES_PER_NODE, NULL, NULL); \
	popcorn_omp_id(R);

#define POPCORN_OMP_MIGRATE_END() \
	popcorn_omp_id(0); \
	migrate(0, NULL, NULL); \
}
#endif /* SCHEDULE_THREADS */

#endif /* _OPENMP */

#endif
