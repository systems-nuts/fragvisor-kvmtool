/*
 * test.c
 * Copyright (C) 2019 jackchuang <jackchuang@echo5>
 *
 * Distributed under terms of the MIT license.
 */

//#include "test.h"

//compilation: gcc -o affinity affinity.c -lpthread --static

//#define _GNU_SOURCE  // cpu_set_t , CPU_SET
//#include <sched.h>   // cpu_set_t , CPU_SET
//#include <pthread.h> // pthread_t

#include <pophype_util.h> /* order matters */

#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

//#include <time.h>
//#include <sys/time.h>

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


#define MAX_CPU_CNT 32 // move to popcorn
#define DEFAULT_CPU_CNT 2

/* Watchout - gcc optimization  */
volatile unsigned long *global;
volatile unsigned long local[MAX_CPU_CNT];

struct thread_context {
  int id;
};


/* TODO move to popcorn lib */
int popcorn_gettid(void)
{
  return syscall(SYSCALL_GETTID);
}

int avail_cpu = -1;

/* TODO move to popcorn lib */
void print_affinity(int cpu) {
    cpu_set_t mask;
	int ofs = 0;
    long nproc, i;
	char *out_buf = (char*)calloc(1024,sizeof(char));

	if (!out_buf) { exit(-1); }

    if (sched_getaffinity(0, sizeof(cpu_set_t), &mask) == -1) {
        perror("sched_getaffinity");
        assert(false);
    } else {
        nproc = sysconf(_SC_NPROCESSORS_ONLN);
        //printf("[%d] sched_getaffinity = \n", popcorn_gettid());
        ofs += sprintf(out_buf, "<%d> [%d] sched_getaffinity = ", cpu, popcorn_gettid());
        for (i = 0; i < nproc; i++) {
            //printf("%d ", CPU_ISSET(i, &mask));
            ofs += sprintf(out_buf + ofs, "%d ", CPU_ISSET(i, &mask));
        }
        //printf("\n");
        //printf("debug out_buf len %d\n", strlen(out_buf));
        ofs += sprintf(out_buf + ofs, "\n");
		puts(out_buf);
		//printf("%s", out_buf);
    }
	free(out_buf);
}

void *th_func(void * arg); 

int main(int argc, char *argv[]) {
  int i, nthread = DEFAULT_CPU_CNT;
  pthread_t thread; //the thread
  struct thread_context t_ctx[MAX_CPU_CNT];
  avail_cpu = sysconf(_SC_NPROCESSORS_ONLN);

  if (argc <= 1) {
  } else if (argc <= 2) {
  	nthread = atoi(argv[1]);
  	printf("<threads>: %d\n", nthread);
  } else {
  	printf("Usage ./app <threads>\n");
  	exit(-1);
  }

  global = (unsigned long *)malloc(sizeof(unsigned long) * nthread);

  // if arg input then overwrite nthread

  printf("===============================\n");
  printf("nthread = %d (MAX %d)\n", nthread, MAX_CPU_CNT);
  printf("===============================\n\n\n");

  for (i = 0; i < nthread; i++) {
  	t_ctx[i].id = i;
	global[i] = 0;
	printf("[%d] main() create tsk for <%d>\n", popcorn_gettid(), i);
    pthread_create(&thread,NULL,th_func, &t_ctx[i]);
	printf("[%d] main() sleep 10s - created tsk for <%d>\n\n", popcorn_gettid(), i);
	sleep(5);
  }

  printf("\n\n========== [%d] main() done TODO LIST =====\n\n", popcorn_gettid());
  //printf("1. access global mem to trigger dsm\n");
  //printf("========================================\n\n");
  pthread_join(thread,NULL);   

  return 0;
}

int next_cpu(int cpu)
{
  cpu += 1;
  while (cpu >= avail_cpu)
    cpu -= avail_cpu;
  return cpu;
}

int nnext_cpu(int cpu)
{
  cpu += 2;
  while (cpu >= avail_cpu)
    cpu -= avail_cpu;
  return cpu;
}

unsigned long burn(int cpu_id)
{
  /* ratio for access local and global mem */
  int i, j, k, x = 1000, y = 1000, z = 2500; // host 5s guest ?s
  long sdiff_us = 0;
  unsigned long diff_s = 0;
  unsigned long diff_us = 0;
  struct timeval start;
  struct timeval end;

  gettimeofday(&start,NULL);
  for (i = 0; i < x; i++) {
  	for (j = 0; j < y; j++) {
  	  for (k = 0; k < z; k++) {
		local[cpu_id]++;
	  }
	  global[next_cpu(cpu_id)]++;
	}
	global[nnext_cpu(cpu_id)]++;
  }
  gettimeofday(&end,NULL);

  /* verbose - debug correctness */
  //printf("[%d] <%d> start %lu.%lu ~ end %lu.%lu\n", popcorn_gettid(), cpu_id,
  //						start.tv_sec, start.tv_usec, end.tv_sec, end.tv_usec);

  sdiff_us = (1000 * 1000 * (end.tv_sec - start.tv_sec)) + end.tv_usec - start.tv_usec;
  sdiff_us = end.tv_usec - start.tv_usec;
  diff_s = end.tv_sec - start.tv_sec;
  diff_us = sdiff_us > 0 ? (diff_s * 1 * 1000 * 1000) + sdiff_us :
  							(1 * 1000 * 1000 * (diff_s)) - ( sdiff_us * -1) ;
  printf("[%d] <%d> burned %lu s (%lu ms)\n",
  		popcorn_gettid(), cpu_id, diff_us / 1000 / 1000, diff_us / 1000);
  return diff_us;
  //return local[cpu_id]; 
}

void *th_func(void * arg)
{
  struct thread_context *t_ctx = arg;
  //we can set one or more bits here, each one representing a single CPU
//  cpu_set_t cpuset; 
  int i = 1000, j = 1000, k = 1000;
  int x, y, z;

  //the CPU we whant to use
  int cpu = t_ctx->id; // 0: 1 0 0... 1: 0 1 0...  2: 0 0 1...
  printf("[%d] on cpu %d\n", popcorn_gettid(), cpu);

//  CPU_ZERO(&cpuset);       //clears the cpuset
//  CPU_SET(cpu , &cpuset); //set CPU 2 on cpuset
//
//  /*
//   * cpu affinity for the calling thread 
//   * first parameter is the pid, 0 = calling thread
//   * second parameter is the size of your cpuset
//   * third param is the cpuset in which your thread will be
//   * placed. Each bit represents a CPU
//   */
//  sched_setaffinity(0, sizeof(cpuset), &cpuset);

  popcorn_setaffinity(cpu);

  print_affinity(cpu);

  //while (1) {
  for (x = 0; x < i; x++) {
    for (y = 0; y < j; y++) {
      for (z = 0; z < k; z++) {
		//unsigned long burn_t = 0;
		printf("[%d] <%d> ***burn()***\n", popcorn_gettid(), cpu);
		burn(cpu);
		//burn_t = burn(cpu);


		printf("[%d] <%d> sleep 10s\n", popcorn_gettid(), cpu);
		sleep(10);
		//printf("[%d] <%d> sleep %lu * 2 ms\n", popcorn_gettid(), cpu, burn_t);
		//usleep(burn_t * 2);
		//   ; //burns the CPU 2
	  }
	}
  }

  return 0;
}
