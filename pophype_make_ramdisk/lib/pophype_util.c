/*
 * pophype_util.c
 * Copyright (C) 2019 jackchuang <jackchuang@echo>
 *
 * Distributed under terms of the MIT license.
 */

#include "pophype_util.h"

int popcorn_gettid(void)
{
    return syscall(SYSCALL_GETTID);
}

void print_affinity()
{
    cpu_set_t mask;
    long nproc, i;

    if (sched_getaffinity(0, sizeof(cpu_set_t), &mask) == -1) {
        perror("sched_getaffinity");
        assert(false);
    } else {
        nproc = sysconf(_SC_NPROCESSORS_ONLN);
        printf("[%d] sched_getaffinity = ", popcorn_gettid());
        for (i = 0; i < nproc; i++) {
            printf("%d ", CPU_ISSET(i, &mask));
        }
        printf("\n");
    }
}

