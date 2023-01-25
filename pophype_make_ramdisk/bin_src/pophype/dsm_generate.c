/*
 * dsm_generate.c
 * Copyright (C) 2019 jackchuang <jackchuang@echo>
 *
 * Distributed under terms of the MIT license.
 */

#include <stdio.h>
#include <stdlib.h>
//#include "dsm_generate.h"
//#include <linux/syscalls.h>
//bool
#include <stdbool.h>

/* shm */
#include <string.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>

/* file */
#include <unistd.h>

/* time */
#include <time.h>
#define BILLION 1E9

#define OP_FALSE_SHARE_FIRST 0
#define OP_FALSE_SHARE_SECOND 1
#define OP_TRUE_SHARE 2
#define OP_NO_SHARE_FIRST 3
#define OP_NO_SHARE_SECOND 4

#ifdef __x86_64__
#define SYSCALL_POPCORN_BROADCAST_CPU_TABLE 370
#define SYSCALL_POPCORN_FALSE_SHARE 375
#define SYSCALL_POPCORN_TRUE_SHARE 376
#define SYSCALL_POPCORN_NO_SHARE 377
#else
#error Does not support this arch
#endif

long popcorn_false_share(int notused)
{
    printf("%s syscall(%d)\n", __func__, SYSCALL_POPCORN_FALSE_SHARE);
    return syscall(SYSCALL_POPCORN_FALSE_SHARE, notused);
}

long popcorn_true_share(int *notused)
{
    printf("%s syscall(%d)\n", __func__, SYSCALL_POPCORN_TRUE_SHARE);
    return syscall(SYSCALL_POPCORN_TRUE_SHARE, notused);
}

long popcorn_no_share(int notused)
{
    printf("%s syscall(%d)\n", __func__, SYSCALL_POPCORN_NO_SHARE);
    return syscall(SYSCALL_POPCORN_NO_SHARE, notused);
}

void show(char *argv[]) {
	printf("Your input: <1> %s\n", argv[1]);
	printf("Your input: <2> %s\n", argv[2]);
	printf("Usage: ./%s <MODE:0/1/2/3/4/5/6> \n", argv[0]);
}

//long noinline user_level4_stack(void);
//long noinline user_level3_stack(void);
//long noinline user_level2_stack(void);
//long noinline user_level1_stack(void);
volatile int no_share_first = 0; /* make it in .bss/.data to be visible in objdumped asm */
long __attribute__((noinline)) user_level4_stack(void) {
    int i, j, k;
    for (i = 0; i < 100; i++) {
        for (j = 0; j < 100000000; j++) {
            for (k = 0; k < 1; k++) {
				no_share_first += 1;
            }
        }
        //schedule();
    }
    return 0;
}
long __attribute__((noinline)) user_level3_stack(void) { return user_level4_stack(); }
long __attribute__((noinline)) user_level2_stack(void) { return user_level3_stack(); }
long __attribute__((noinline)) user_level1_stack(void) { return user_level2_stack(); }
long __attribute__((noinline)) burn_loop(void) { return user_level1_stack(); }
int shm_server(int is_ofs);
int shm_client(int is_ofs, int number_of_instances);
int shm_client_noclose(int is_ofs, int my_instance_number);

int main(int argc, char *argv[])
{
//
	struct timespec requestStart, requestEnd;
	clock_gettime(CLOCK_REALTIME, &requestStart);

	if (argc != 2 && argc != 3 ) {
		show(argv);
		return 1;
	}

	if (*argv[1] == '0') {
		printf("Do FALSE_SHARE1\n");
		popcorn_false_share(1);
	} else if (!strcmp(argv[1], "1")) {
		printf("Do FALSE_SHARE2\n");
		popcorn_false_share(2);
	} else if (!strcmp(argv[1], "2")) {
		printf("Do TRUE_SHARE\n");
		popcorn_true_share(NULL); /* TODO: remove argv in case kernel reads it */
	} else if (!strcmp(argv[1], "3")) {
		printf("Do NO_SHARE1\n");
		popcorn_no_share(1);
	} else if (!strcmp(argv[1], "4")) {
		printf("Do NO_SHARE2\n");
		popcorn_no_share(2);

/***************** SERVER ***************/
/*--------------------------------------*/
/***************** USER *****************/
	} else if (!strcmp(argv[1], "5")) { // server same
		/* usr */
		shm_server(0);
	} else if (!strcmp(argv[1], "6")) { // client same
		if (!atoi(argv[2]) || !strcmp(argv[2], "\0"))
			exit(-1);
		shm_client(1, atoi(argv[2])); // 5+6 = false sharing + close(TODO)
	} else if (!strcmp(argv[1], "7")) {
		if (!atoi(argv[2]) || !strcmp(argv[2], "\0"))
			exit(-1);
		shm_client(0, atoi(argv[2])); // 5+7 = true sharing + close(TODO)
	} else if (!strcmp(argv[1], "8")) {
		if (!atoi(argv[2]) || !strcmp(argv[2], "\0"))
			exit(-1);
		shm_client(4097, atoi(argv[2])); // 5+8 = no sharing + close(TODO)

	/* client no close*/
	} else if (!strcmp(argv[1], "22")) { // no sharing
		shm_client_noclose(8094, 3); // 5+8+9 = 3 no sharing
	} else if (!strcmp(argv[1], "23")) { // no sharing
		shm_client_noclose(12291, 4); // 5+8+9+10 = 4 no sharing
		//struct timespec requestStart, requestEnd;
		//clock_gettime(CLOCK_REALTIME, &requestStart);
		//burn_loop();
		//clock_gettime(CLOCK_REALTIME, &requestEnd);
		//double accum = (requestEnd.tv_sec - requestStart.tv_sec)
		//		  + (requestEnd.tv_nsec - requestStart.tv_nsec) / BILLION;
		//printf("\nexec time: %lf\n\n", accum);
	} else if (!strcmp(argv[1], "32")) { // false sharing no close
		shm_client_noclose(2, 3);
	} else if (!strcmp(argv[1], "33")) { // false sharing no close
		shm_client_noclose(3, 4);
	} else if (!strcmp(argv[1], "42")) { // true sharing no close
		shm_client_noclose(0, 3);
	} else if (!strcmp(argv[1], "43")) { // true sharing no close
		shm_client_noclose(0, 4);
	} else { show(argv); }

	clock_gettime(CLOCK_REALTIME, &requestEnd);
	double accum = ( requestEnd.tv_sec - requestStart.tv_sec )
	  + ( requestEnd.tv_nsec - requestStart.tv_nsec )
	  / BILLION;
	printf( "\nmain argv: %s %s exec time: %lf\n\n", argv[1], argv[2], accum );
//
}

int is_file(const char* fname) {
	if (access(fname, F_OK) != -1) {
		return 1; // file exists
	} else {
		return 0;// file doesn't exist
	}
}

int shm_server(int is_ofs) {
    void* ptr;
    int fd;
    int shm_fd;
    const int SIZE = 16384;
    const char* name = "OS";
    const char* message_0 = "Hello";
    const char* message_1 = "World!";
	const char* server_start = "server_start.lock";
    const char* pophype_server_done_sync = "pophype.lock";
	FILE *pFile;

	if (is_file(server_start)) {
		printf("%s: WRONG - %s should not exist\n", __func__, server_start);
		exit(-1);
	} else {
	}
   
	shm_fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    /* configure the size of the shared memory object */
    ftruncate(shm_fd, SIZE);

	// signal client - ready to run
	pFile = fopen(server_start, "w+");
	if (pFile != NULL) {
		fclose (pFile);
		printf("server done - %s created %s\n", server_start, pFile);
	} else {
		printf("%s: WRONG - %s cannot create\n", __func__, server_start);
		exit(-1);
	}


//
	struct timespec requestStart, requestEnd;
	clock_gettime(CLOCK_REALTIME, &requestStart);
 
    /* memory map the shared memory object */
    ptr = mmap(0, SIZE, PROT_WRITE, MAP_SHARED, shm_fd, 0);
	printf("%s: %p\n", __func__, ptr);

	/*	ofs+0/+1 true or false sharing controlled outside */
	ptr = (long *)(ptr + is_ofs);

    int i, j, k;
    for (i = 0; i < 100; i++) {
        //for (j = 0; j < 1; j++) {
        for (j = 0; j < 100000000; j++) {
            for (k = 0; k < 1; k++) {
				*(long *)ptr += 1;
            }
        }
	}

	clock_gettime(CLOCK_REALTIME, &requestEnd);
	double accum = ( requestEnd.tv_sec - requestStart.tv_sec )
	  + ( requestEnd.tv_nsec - requestStart.tv_nsec )
	  / BILLION;
	printf( "\n%s exec time: %lf\n\n", __func__, accum );
//


    /* write to the shared memory object */
    //ptr += sprintf(ptr, "%s", message_0);
    //ptr += sprintf(ptr, "%s", message_1);

    //ptr += strlen(message_0);
    //ptr += strlen(message_1);

	/* Let last client know I'm done */
	// create pophype_server_done_sync
	//fd = open(pophype_server_done_sync, O_CREAT | O_RDWR, 0666);
	pFile = fopen(pophype_server_done_sync, "w+");
	if (pFile != NULL)
	{
		//fputs ("fopen example",pFile);
		fclose (pFile);
		printf("server done - %s created %s\n", pophype_server_done_sync, pFile);
	} else {
		printf("%s: WRONG - %s cannot create\n", __func__, pophype_server_done_sync);
		exit(-1);
	}
    return 0;
}

int shm_client(int is_ofs, int number_of_instances) {
    void* ptr;
    int shm_fd;
    const int SIZE = 16384;
    const char* name = "OS";
    const char* server_start = "server_start.lock";
    const char* pophype_server_done_sync = "pophype.lock";
    char* _pophype_client_noclose_done_sync = "client_noclose.lock";
    char pophype_client_noclose_done_sync[255];
	FILE *file;


	while (!is_file(server_start)) {
		//printf("waiting server start\n"); // dbg
	}
//wait_start:
//	if ((file = fopen(server_start, "r"))) { // exist
//		fclose(file);
//		remove(server_start);
//    	return 0;
//	} else { // file exist - server not done
//		printf("wait until server start %s\n", server_start);
//		goto wait_start;
//	}

	/*****
	 * [sync] - server has been running before me
	 ***/
	struct timespec requestStart2, requestEnd2;
	clock_gettime(CLOCK_REALTIME, &requestStart2);

    shm_fd = shm_open(name, O_RDWR, 0666);

    /* memory map the shared memory object */
    ptr = mmap(0, SIZE, PROT_WRITE, MAP_SHARED, shm_fd, 0);
	printf("%s: %p\n", __func__, ptr);

	/*	ofs+0/+1 true or false sharing controlled outside */
	ptr = (long *)(ptr + is_ofs);

	struct timespec requestStart, requestEnd;
	clock_gettime(CLOCK_REALTIME, &requestStart);

	int i, j, k;
    for (i = 0; i < 100; i++) {
        //for (j = 0; j < 1; j++) {
        for (j = 0; j < 100000000; j++) {
            for (k = 0; k < 1; k++) {
				*(long *)(ptr) += 1;
            }
        }
	}

	clock_gettime(CLOCK_REALTIME, &requestEnd);
	double accum = ( requestEnd.tv_sec - requestStart.tv_sec )
	  + ( requestEnd.tv_nsec - requestStart.tv_nsec )
	  / BILLION;
	printf( "\n%s exec time: %lf\n\n", __func__, accum );
//
	/* TODO - count upto here but HOW... */

    /* Read from the shared memory object */
    //printf("%s", (char*)ptr);

	while (!is_file(pophype_server_done_sync)) {
		//printf("[don't trust bash time] waiting server done for timming\n"); // dbg
	}
	/* check server done or not */

    for (i = 3; i <= number_of_instances; i++) { /* 2nodes: no client_noclose*/
		sprintf (pophype_client_noclose_done_sync, "%s%d", _pophype_client_noclose_done_sync, i);
		//printf("client last waits %s\n", pophype_client_noclose_done_sync); // dbg
		while (!is_file(pophype_client_noclose_done_sync)) {
			//printf("[don't trust bash time] waiting clinet_noclose done for timming\n"); // dbg
		}
	}


	clock_gettime(CLOCK_REALTIME, &requestEnd2);
	double accum2 = ( requestEnd2.tv_sec - requestStart2.tv_sec )
	  + ( requestEnd2.tv_nsec - requestStart2.tv_nsec )
	  / BILLION;
	printf( "\n%s [micro_perf] exec time: %lf\n\n", __func__, accum2 );
	/**** [micro_perf] used for paper data/numbers
	 * Idea: 	check server_start at client ->
	 *			[ time_start at client ] ->
	 *			client finish + server finish ->
	 *			[ time_end at client ] ->
	 *			client release shm + releases locks
	 *
	 *			So that we don't consider shm_init and shm_destory.
	 */
//

retry:
	if (file = fopen(pophype_server_done_sync, "r")) {
		fclose(file);
		/* Clean all - two sync files and shm */
		remove(pophype_server_done_sync);
		printf("remove %s\n", pophype_server_done_sync);
		remove(server_start);
		printf("remove %s\n", server_start);
    	shm_unlink(name);
	} else { // file exist - server not done
		printf("FK no %s\n", pophype_server_done_sync);
		exit(-1);
		goto retry;
	}

    for (i = 3; i <= number_of_instances; i++) { /* 2nodes: no client_noclose*/
		sprintf (pophype_client_noclose_done_sync, "%s%d", _pophype_client_noclose_done_sync, i);
		printf("client last waits %s\n", pophype_client_noclose_done_sync);
		//while (!is_file(pophype_client_noclose_done_sync)) {
		//	//printf("[don't trust bash time] waiting clinet_noclose done for timming\n"); // dbg
		//}
		remove(pophype_client_noclose_done_sync);
		printf("remove %s\n", pophype_client_noclose_done_sync);
	}

	return 0;
}


int shm_client_noclose(int is_ofs, int my_instance_number) {
    void* ptr;
    int shm_fd;
    const int SIZE = 16384;
    const char* name = "OS";
    const char* server_start = "server_start.lock";
    char* _pophype_client_noclose_done_sync = "client_noclose.lock";
    char pophype_client_noclose_done_sync[255];
	FILE *file;

	sprintf (pophype_client_noclose_done_sync, "%s%d", _pophype_client_noclose_done_sync, my_instance_number);

	while (!is_file(server_start)) {
		//printf("waiting server start\n"); // dbg
	}

	/*****
	 * [sync] - server has been running before me
	 ***/
	struct timespec requestStart2, requestEnd2;
	clock_gettime(CLOCK_REALTIME, &requestStart2);

    shm_fd = shm_open(name, O_RDWR, 0666);

    /* memory map the shared memory object */
    ptr = mmap(0, SIZE, PROT_WRITE, MAP_SHARED, shm_fd, 0);
	printf("%s: %p\n", __func__, ptr);

	/*	ofs+0/+1 true or false sharing controlled outside */
	ptr = (long *)(ptr + is_ofs);

	struct timespec requestStart, requestEnd;
	clock_gettime(CLOCK_REALTIME, &requestStart);

	int i, j, k;
    for (i = 0; i < 100; i++) {
        //for (j = 0; j < 1; j++) {
        for (j = 0; j < 100000000; j++) {
            for (k = 0; k < 1; k++) {
				*(long *)(ptr) += 1;
            }
        }
	}

	/* Let last client know I'm done */
	FILE *pFile;
    pFile = fopen(pophype_client_noclose_done_sync, "w+");
    if (pFile != NULL)
    {
        fclose (pFile);
        printf("client_noclose done - %s created %s\n", pophype_client_noclose_done_sync, pFile);
    } else {
        printf("%s: WRONG - %s cannot create\n", __func__, pophype_client_noclose_done_sync);
        exit(-1);
    }

	clock_gettime(CLOCK_REALTIME, &requestEnd);
	double accum = ( requestEnd.tv_sec - requestStart.tv_sec )
	  + ( requestEnd.tv_nsec - requestStart.tv_nsec )
	  / BILLION;
	printf( "\n%s exec time: %lf\n\n", __func__, accum );
//
	/* TODO - count upto here but HOW... */

    /* Read from the shared memory object */
    //printf("%s", (char*)ptr);

//	while (!is_file(pophype_server_done_sync)) {
//		//printf("[don't trust bash time]waiting server done for timming\n"); // dbg
//	}
//	/* check server done or not */



//	clock_gettime(CLOCK_REALTIME, &requestEnd2);
//	double accum2 = ( requestEnd2.tv_sec - requestStart2.tv_sec )
//	  + ( requestEnd2.tv_nsec - requestStart2.tv_nsec )
//	  / BILLION;
//	printf( "\n%s [micro_perf] exec time: %lf\n\n", __func__, accum2 );
	/**** [micro_perf] used for paper data/numbers
	 * Idea: 	check server_start at client ->
	 *			[ time_start at client ] ->
	 *			client finish + server finish ->
	 *			[ time_end at client ] ->
	 *			client release shm + releases locks
	 *
	 *			So that we don't consider shm_init and shm_destory.
	 */
//



//retry:
//	if (file = fopen(pophype_server_done_sync, "r")) {
//		fclose(file);
//		/* Clean all - two sync files and shm */
//		remove(pophype_server_done_sync);
//		printf("remove %s\n", pophype_server_done_sync);
//		remove(server_start);
//		printf("remove %s\n", server_start);
//    	shm_unlink(name);
//    	return 0;
//	} else { // file exist - server not done
//		printf("FK no %s\n", pophype_server_done_sync);
//		exit(-1);
//		goto retry;
//	}
}


//volatile long level4_stack(int op)
//{
//    if (op == OP_FALSE_SHARE) {
//
//    } else if (op == OP_TRUE_SHARE) {
//
//    } else if (op == OP_NO_SHARE) {
//
//    } else {
//        BUG();
//    }
//    return 0;
//}
//volatile long level3_stack(int op) { return level4_stack(op); }
//volatile long level2_stack(int op) { return level3_stack(op); }
//volatile long level1_stack(int op) { return level2_stack(op); }
//
//SYSCALL_DEFINE1(popcorn_false_share, int __user *, notused)
//{
//    return level1_stack(OP_FALSE_SHARE);
//}
//
//SYSCALL_DEFINE1(popcorn_true_share, int __user *, notused)
//{
//    return level1_stack(OP_TRUE_SHARE);
//}
//
//SYSCALL_DEFINE1(popcorn_no_share, int __user *, notused)
//{
//    return level1_stack(OP_NO_SHARE);
//}

