#
# Makefile
# jackchuang, 2019-12-31 01:17
#
.PHONY: all

all:
	gcc dsm_generate.c --static -g -ggdb -o dsm_generate -lrt
	chmod 775 dsm_generate
	objdump -dSlr dsm_generate > dsm_generate.asm
