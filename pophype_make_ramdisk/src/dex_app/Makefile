CC = $(CROSS_COMPILE)gcc
#OBJDUMP = $(CROSS_COMPILE)objdump -d -S

LIBMIGRATE = libmigrate.a
#POPCORN_LIB_PATH = ../lib
#POPCORN_INC_PATH = ../include 
POPCORN_LIB_PATH = /home/jackchuang/share/popcorn-rack-lib-pophype/lib #TODO fix
POPCORN_INC_PATH = /home/jackchuang/share/popcorn-rack-lib-pophype/include #TODO fix


# Features
CFLAGS += -O2
CFLAGS += -DDEBUG -g -Wall
CFLAGS += -I$(POPCORN_INC_PATH)
CFLAGS += -D_ALIGN_VARIABLES
CFLAGS += -D_VERBOSE
#CFLAGS += -D_POPCORN_ALIGN_HEAP
#CFLAGS += -DWAIT_FOR_DEBUGGER

LDFLAGS += -static
LDFLAGS += -L$(POPCORN_LIB_PATH)

LIBS += -pthread
LIBS += -l:$(LIBMIGRATE)

TARGETS = kmeans grep
#status
OBJDUMPS =

OUT = ../bin

all: $(TARGETS) $(OBJDUMPS)

%.o: %.c
	$(CC) -c $(CFLAGS) $^ -o $@

#status: status.o
#	$(CC) $(LDFLAGS) $^ $(LIBS) -o $(OUT)/$@

kmeans: kmeans.o
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $(OUT)/$@

grep: grep.o
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $(OUT)/$@

clean:
	rm -f $(OUT)/$(TARGETS) $(OBJDUMPS) *.o $(OUT)/kmeans $(OUT)/grep
