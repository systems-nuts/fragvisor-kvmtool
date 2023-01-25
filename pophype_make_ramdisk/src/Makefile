#
# Makefile
# jackchuang, 2019-07-16 02:43
#
CC = $(CROSS_COMPILE)gcc

LIBMIGRATE = pophype_util.a
#LIBMIGRATE += libmigrate.a

POPCORN_LIB_PATH = ../lib
POPCORN_INC_PATH = ../include

#all: affinity init cp
#	echo "Makefile needs your attention"
#TARGETS = affinity init kmeans grep cp migrate #init2.sh
TARGETS = affinity init kmeans cp migrate #init2.sh
OBJDUMPS =

LDFLAGS += -static -lpthread
LDFLAGS += -L$(POPCORN_LIB_PATH)

LIBS += -pthread
LIBS += -l:$(LIBMIGRATE)

CFLAGS += -O2
CFLAGS += -DDEBUG -g -Wall
CFLAGS += -I$(POPCORN_INC_PATH)
CFLAGS += -D_ALIGN_VARIABLES
CFLAGS += -D_VERBOSE
#CFLAGS += -D_POPCORN_ALIGN_HEAP
#CFLAGS += -DWAIT_FOR_DEBUGGER

OUT = ../bin

#`mkdir bin`

all: $(TARGETS) $(OBJDUMPS)

# subfolder
dex_app:
	make -C dex_app
	#make -C dex_app all

# generate .o first
%.o: %.c
	$(CC) -c $(CFLAGS) $^ -o $@

# then generate bins
affinity: affinity.o
	$(CC) -o $(OUT)/$@ $^ $(LDFLAGS)

init: init.o
	$(CC) -o $(OUT)/$@ $^ $(LDFLAGS)

kmeans: kmeans.o
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $(OUT)/$@

grep: grep.o
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $(OUT)/$@

migrate: migrate.o
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $(OUT)/$@
# shell
#init2.sh: init2
	#echo "generating init2"
	#cp init2 bin/init2
	#cp init2 $(OUT)/$@
	#cp $^ $(OUT)/$^

cp:
	echo "cpying from $(OUT)/ to ramdisk require sudo\n"
	#sudo cp init2 ../rootfs
	#sudo cp $(OUT)/* ../rootfs
	#sudo cp $(OUT)/* ../bin
	##gcc -o init2 init2.c -lpthread --static

clean:
	sudo rm *.o $(OBJDUMPS) $(OUT)/*
	ls $(OUT)
	#rm $(OUT)/$(TARGETS)
	#make clean -C dex_app
	#rm $(OUT)/kmeans $(OUT)/grep


# vim:ft=make
#
