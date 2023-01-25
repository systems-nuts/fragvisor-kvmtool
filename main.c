#include "kvm/kvm.h"

#include <stdlib.h>
#include <stdio.h>

/* user defined header files */
#include <kvm/kvm-cmd.h>
#include <popcorn/utils.h>

static int handle_kvm_command(int argc, char **argv)
{
	/* at kvm-cmd.c */
	return handle_command(kvm_commands, argc, (const char **) &argv[0]);
}

/****
 * Distributing main threads
 */
int nthreads = 1;
int nodes = 0;
int my_nid = -1;
int here = 0;
int check_stall = 1;
unsigned long *global;

//extern struct timeval tstart;
struct timeval tstart, tstop;

pthread_barrier_t barrier_start;
pthread_barrier_t barrier_cmd_init;
pthread_barrier_t barrier_sys_open;
pthread_barrier_t barrier_vm_open;
pthread_barrier_t barrier_arch_init;
pthread_barrier_t barrier_arch_init_2;
pthread_barrier_t barrier_arch_init_3;
pthread_barrier_t barrier_arch_init_done;
pthread_barrier_t barrier_mem_register_order;
pthread_barrier_t barrier_mem_register_done;

/* class 2 ipc__init() */
pthread_barrier_t barrier_server_fd; // sock
pthread_barrier_t barrier_epoll_fd;
pthread_barrier_t barrier_estop_fd;
pthread_barrier_t barrier_ipc_thread;

pthread_barrier_t barrier_io_fd;
pthread_barrier_t barrier_epollio_fd;
pthread_barrier_t barrier_estopio_fd;

pthread_barrier_t barrier_etask_fd;

pthread_barrier_t barrier_kvm_cpus;
pthread_barrier_t barrier_kvm_cpu; /* rename to _order*/
pthread_barrier_t barrier_kvm_cpu_done;

/* class 4 irq__init() (./x86/irq.c) */
pthread_barrier_t barrier_irq_setup;

/* class 4 vfio__init() (./vfio/core.c) skip */
pthread_barrier_t barrier_vfio_malloc;
pthread_barrier_t barrier_vfio_heap;
pthread_barrier_t barrier_container_init;
pthread_barrier_t barrier_reserve_region_done;

/*
class 4 pci__init() (./pci.c)
class 4 ioport__init() (./ioport.c)
class 4 disk_image__init() (disk/core.c)
class 5 kbd__init() (hw/i8042.c)
class 5 pci_shmem__init() (hw/pci-shmem.c)
class 5 term_init() (term.c)
class 5 serial8250__init() (hw/serial.c)
class 5 rtc__init() (hw/rtc.c)
class 6 virtio_9p__init() (virtio/9p.c)
class 6 virtio_bln__init() (virtio/balloon.c)
class 6 virtio_rng__init() (virtio/rng.c)
class 6 virtio_net__init() (virtio/net.c)
class 6 virtio_console__init() (virtio/console.c)
class 6 virtio_scsi_init() (virtio/scsi.c)
class 6 virtio_blk__init() (virtio/blk.c)
class 7 mptable__init() (x86/mptable.c)
	class 7 fb__init() (framebuffer.c)
	class 9 thread_pool__init() (util/threadpool.c)
*/

pthread_barrier_t barrier_dbg_last_sync;

pthread_barrier_t barrier_distribute_sync; // testing the right loacation now
pthread_barrier_t barrier_atomic_op_test_start;

pthread_barrier_t barrier_run_exit;

pthread_barrier_t barrier_run_destory;

pthread_barrier_t barrier_end;

struct thread {
    pthread_t thread_info;
    int nid; // nid == id
    int id; // nid == id

	int argc;
	char **argv;
//    int tid;
//    int at;
    int ret;
//    int finish;
//    unsigned long count;
};
struct thread **main_threads;
//struct thread stall_monitor;

int __init_thread_params(void);
/* Use nodes */
int __init_thread_params(void)
{
    int i;

	POP_PRINTF("= init threads[] and global[] =\n");
    main_threads = (struct thread **)malloc(sizeof(struct thread *) * nodes);
    global = (unsigned long *)malloc(sizeof(unsigned long) * nodes);

	for (i = 0; i < nodes; i++) {
		global[i] = 0;
		posix_memalign((void **)&(main_threads[i]),
						PAGE_SIZE, sizeof(struct thread));
	}
	POP_PRINTF("=================done\n\n");

	POP_PRINTF("========== init barrieres =======\n");
	POP_PRINTF(" - nodes %d (double check for barriers)\n", nodes);

	//printf("BUG: msg_popcorn2.sh will set nodes.....\n");

	/* now only main thread */
    //pthread_barrier_init(&barrier_start, NULL, 1 + 1); // main() + 1 main thread
	//pthread_barrier_init(&barrier_end, NULL, 1 + 1);
	//pthread_barrier_init(&barrier_cmd_init, NULL, 1);
    //pthread_barrier_init(&barrier_dbg_last_sync, NULL, 1);
	// after fixing in parse_args()
    //pthread_barrier_init(&barrier_start, NULL, nthreads + 1);
    pthread_barrier_init(&barrier_start, NULL, nodes + 1); // all threads + main()
	pthread_barrier_init(&barrier_end, NULL, nodes + 1);
	pthread_barrier_init(&barrier_cmd_init, NULL, nodes);
    pthread_barrier_init(&barrier_dbg_last_sync, NULL, nodes);
	/* lkvm commands like ckpt need these barriers above */

	/* Not distributed lkvm should not touch things below */
	pthread_barrier_init(&barrier_popcorn_serial_threads, NULL, nodes);
	pthread_barrier_init(&barrier_popcorn_serial_threads_start, NULL, nodes);

    pthread_barrier_init(&barrier_sys_open, NULL, nodes);
    pthread_barrier_init(&barrier_vm_open, NULL, nodes);

	pthread_barrier_init(&barrier_arch_init, NULL, nodes);
	pthread_barrier_init(&barrier_arch_init_2, NULL, nodes);
	pthread_barrier_init(&barrier_arch_init_3, NULL, nodes);
    pthread_barrier_init(&barrier_arch_init_done, NULL, nodes);
    pthread_barrier_init(&barrier_mem_register_order, NULL, nodes);
    pthread_barrier_init(&barrier_mem_register_done, NULL, nodes);

    pthread_barrier_init(&barrier_server_fd, NULL, nodes);
    pthread_barrier_init(&barrier_epoll_fd, NULL, nodes);
    pthread_barrier_init(&barrier_estop_fd, NULL, nodes);
    pthread_barrier_init(&barrier_ipc_thread, NULL, nodes);

	pthread_barrier_init(&barrier_io_fd, NULL, nodes);
	pthread_barrier_init(&barrier_epollio_fd, NULL, nodes);
	pthread_barrier_init(&barrier_estopio_fd, NULL, nodes);

	pthread_barrier_init(&barrier_etask_fd, NULL, nodes);

	pthread_barrier_init(&barrier_kvm_cpus, NULL, nodes);
	pthread_barrier_init(&barrier_kvm_cpu, NULL, nodes);
	pthread_barrier_init(&barrier_kvm_cpu_done, NULL, nodes);

	pthread_barrier_init(&barrier_irq_setup, NULL, nodes);

	pthread_barrier_init(&barrier_vfio_malloc, NULL, nodes);
	pthread_barrier_init(&barrier_vfio_heap, NULL, nodes);
	pthread_barrier_init(&barrier_container_init, NULL, nodes);
	pthread_barrier_init(&barrier_reserve_region_done, NULL, nodes);


	pthread_barrier_init(&barrier_distribute_sync, NULL, nodes);
	pthread_barrier_init(&barrier_atomic_op_test_start, NULL, nodes);

	pthread_barrier_init(&barrier_run_exit, NULL, nodes);

	pthread_barrier_init(&barrier_run_destory, NULL, nodes);

	POP_PRINTF("--- init barrier done ---\n\n");
    return 0;
}

//int nids[MAX_POPCORN_NODES];
//int *current_nid = nids[nid] // global arry
/* All vcpu threads */
static void *__main(void *_thread)
//static int *__main(void *_thread)
//static int __main(int argc, char *argv[])
{
	struct thread *t = (struct thread*)_thread;
	int argc = t->argc;
	char **argv = t->argv;
	int nid = t->nid;

	/************************************************
		Popcorn multithreaded migration test - START
	 ************************************************/
    asm volatile("" ::: "memory");
	if (nid > 0) {
		migrate(nid, NULL, NULL);
	} else {
		/* lkvm init time */
		gettimeofday(&tstart, NULL);
	}
	POP_PRINTF("%s %s(): beacon (threads all spawned, argv didn't read yet)\n",
														__FILE__, __func__);
	POP_VERB_PRINTF(pop_get_nid(), "[%d/%d] %s(); Currently at the node\n",
									pop_get_nid(), popcorn_gettid(), __func__);

#if 1
    POP_VERB_PRINTF(pop_get_nid(), "[%d/%d] __main migrated\n",
								pop_get_nid(), popcorn_gettid());
	POP_VERB_PRINTF(pop_get_nid(), "[%d/%d] argc %d\n",
					pop_get_nid(), popcorn_gettid(), argc);
	int i;
	for (i = 0; i < argc ; i++) {
		POP_VERB_PRINTF(pop_get_nid(), "[%d/%d] argv[%d/%d] %s\n",
				pop_get_nid(), popcorn_gettid(), i, argc - 1, argv[i]);
	}
#endif

	pthread_barrier_wait(&barrier_start);

//#if !RUN_REMOTE_MAIN_THREAD
//	if (nid > 0) { /* Consider not migration case nid = -1 */
//		printf("Souldn't be here\n");
//		BUG_ON(-1);
//		goto remote_skip; }
//#endif
	/************************************************
		Popcorn multithreaded migration test - DONE
	 ************************************************/

	/* <*> */
	///////////////kill me/* moved to earlier place (before spawning threads) */
	// still requires this for all threads to follow the sementics/lkvm design
   POP_DBGPRINTF("<*> [%d/%d] %s(): call "
			"handle_kvm_command()->handle_command()\n",
		   pop_get_nid(), popcorn_gettid(), __func__);
	/* Original main (remote skip to run it) */
	kvm__set_dir("%s/%s", HOME_DIR, KVM_PID_FILE_PATH);
	t->ret = handle_kvm_command(argc - 1, &argv[1]);

	/* Thinking why I cannot check cmd before spawning */
	/* I found handle_kvm_command
		is the *main()* which spawns vcpu threads */


//#if !RUN_REMOTE_MAIN_THREAD
//remote_skip:
//#endif
//	if (nid > 0) { /* Consider not migration case nid = -1 */
//		printf("Souldn't be here\n");
//		BUG_ON(-1);
//		migrate(0, NULL, NULL);
//	}



	int time = 5;
	POP_PRINTF("<*> __main (local thread) DONE from nid %d "
				"sleeping time %d for deBUGing\n", nid, time);
	sleep(time);
	pthread_barrier_wait(&barrier_end);
    return 0;
}

/** parse_args()
 *  Parse the user arguments
 */
void parse_args(int argc, char **argv);
void parse_args(int argc, char **argv)
{
	int i;
	unsigned long j;
	const char *ft_target[] = {"ckpt", "restart"};
	POP_PRINTF("---\npre parse start\n---\n");

	for (i = 0; i < argc; i++) {
		const char *target = "-c";
		//POP_PRINTF("i = %d *argv %s %d %d\n",
		//		i, *(argv + i), strlen(*(argv + i)), strlen(target));
		if (!strcmp(*(argv + i), target)) {
			POP_PRINTF("\tfound\ni = %d i++ *argv %s\n", i, *(argv + i + 1));
			nodes = atoi(*(argv + i + 1));
		} else {
		}
	}
    POP_PRINTF("paese '-c': # of nodes = %d\n\n", nodes);

	POP_PRINTF("ARRAY_SIZE(ft_target) %lu\n", ARRAY_SIZE(ft_target));
	/* If ft related command, overwriete 'nodes' */
	for (j = 0; j < ARRAY_SIZE(ft_target); j++) {
		/* For lkvm commands other than run like ckpt, restart */
		for (i = 0; i < argc; i++) {
			const char *target = ft_target[j];
			if (!strcmp(*(argv + i), target)) {
				POP_PRINTF("\tfound\ni = %d i++ *argv %s\n",
										i, *(argv + i + 1));
				nodes = 1;
				POP_PRINTF("paese '%s': *OVERWRITE* # of nodes "
						"= %d\n\n", target, nodes);
				POP_PRINTF("paese '%s': *OVERWRITE* # of nodes "
						"= %d (TODO restart)\n\n", target, nodes);
			}
		}
	}
	/* TODO then check if numa-fake=<the same #> */
}

int main(int argc, char *argv[])
{

/////// distributing threads to remote nodes
    int i;
    struct thread *thread;
	POP_PRINTF("%s %s(): beacon\n", __FILE__, __func__);
	POP_PRINTF("=== lkvm start ===\n");

	/* Pophype arguments */
	nthreads = 1;
	parse_args(argc, argv); /* get nodes from user specified -c <nodes> */
	// ********************************/

	POP_PRINTF("=== %s(): Popcorn env info/init === \n", __func__);
	my_nid = pop_get_nid();

	/* Popcorn migration test */
	//pop_test();
	pop_cluster_state_update();
	POP_PRINTF("%s(): Pophype - make it distributed "
					"using pop_distri()\n", __func__);
	pop_distri(nodes);
	//pop_distri_main_thread();
	POP_PRINTF("- Popcorn env info/init done -\n\n");

	POP_PRINTF("=========================================\n");
    POP_PRINTF("  %s(): Welcome to pocporn lkvm\n", __func__);
    POP_PRINTF("=========================================\n");
    POP_PRINTF("- # of threads: %d\n", nthreads);
    POP_PRINTF("- # of *nodes* = has main_threads[%d] = %d\n", nodes, nodes);
    POP_PRINTF("- my_nid %d\n", my_nid);
    POP_PRINTF("\n");
    POP_PRINTF("%s() argc %d argv %p\n", __func__, argc, argv);
    //POP_PRINTF("- Here        : %d\n", here);
    POP_PRINTF("\n\n");

	POP_PRINTF("===========================\n");
	POP_PRINTF("%s(): pophype - init\n", __func__);
	POP_PRINTF("===========================\n\n");

    __init_thread_params();
	/* need nodes but arg got in __main
		The problem is only thead0 and this function not only
		init barriers but also thread.
		My sol:
			thread0 init -> thread 0 read parameters -> the rest threads init
	*/

	POP_PRINTF("===========================\n");
	POP_PRINTF("%s(): pophype - main thread spawns threads (nodes = %d)\n",
				__func__, nodes);
	POP_PRINTF("===========================\n\n");

    for (i = 0; i < nodes; i++) {
    //for (i = 0; i < 1; i++) { // no need to spawn many
        thread = main_threads[i];
        thread->nid = i;
        thread->id = i;
        thread->argc = argc;
		thread->argv = argv;
		/* This looks redundant since remote threads do not do anything */
        pthread_create(&thread->thread_info, NULL, &__main, thread);
    }

	/* This main() thread's jobs are all don, waiting for
		cmd thread which will spawn all vcpu threads and run them */
	pthread_barrier_wait(&barrier_start); /* t0 read argv, all t goes to init classes */
	pthread_barrier_wait(&barrier_end); /* very end */
	POP_PRINTF("almost all done - let vcpu threads end first sleeping 5s\n");
	sleep(3); // trying
	POP_PRINTF("ALL DONE!!\n");
	return 0;
}
