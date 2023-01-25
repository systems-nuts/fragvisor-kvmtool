/*
 * migrate.c
 * Copyright (C) 2020 jackchuang <jackchuang@mir>
 *
 * Distributed under terms of the MIT license.
 */

//#include "migrate.h"

#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>

int main(int argc, char* argv[]) {
    int a0 = -1, a1 = -1;

//	printf("Argument cnt: %d\n", argc);

////	if (argc != 2) { /* including ./ */
////		printf("./%s <target_node>\n", argv[0]);
////		return -1;
////	}

	if (argc >= 2) {
		sscanf(argv[1], "%d", &a0);
//		printf("1st argument: %d\n", a0);
	}
	if (argc >= 3) {
		sscanf(argv[2], "%d", &a1);
//		printf("2nd argument: %d\n", a1);
	}

//	printf("(Guest User) Pophype migrate start\n");
	//syscall(380, 0);
	syscall(380, a0);
	//syscall(380, a0, a1);
//	printf("(Guest User) Pophype migrate done\n");
	return 0;
}
