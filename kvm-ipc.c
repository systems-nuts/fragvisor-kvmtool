#include <sys/epoll.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/eventfd.h>
#include <dirent.h>

#include "kvm/kvm-ipc.h"
#include "kvm/rwsem.h"
#include "kvm/read-write.h"
#include "kvm/util.h"
#include "kvm/kvm.h"
#include "kvm/builtin-debug.h"
#include "kvm/strbuf.h"
#include "kvm/kvm-cpu.h"
#include "kvm/8250-serial.h"

#include <popcorn/utils.h>

struct kvm_ipc_head {
	u32 type;
	u32 len;
};

#define KVM_IPC_MAX_MSGS 16

#define KVM_SOCK_SUFFIX		".sock"
#define KVM_SOCK_SUFFIX_LEN	((ssize_t)sizeof(KVM_SOCK_SUFFIX) - 1)

extern __thread struct kvm_cpu *current_kvm_cpu;
static void (*msgs[KVM_IPC_MAX_MSGS])(struct kvm *kvm, int fd, u32 type, u32 len, u8 *msg);
static DECLARE_RWSEM(msgs_rwlock);
static int epoll_fd = INT_MIN, server_fd = INT_MIN, stop_fd = INT_MIN;
#if !defined(CONFIG_POPCORN_HYPE)
static pthread_t thread;
#endif
#ifdef CONFIG_POPCORN_HYPE
static pthread_t _thread[255]; // TODO; max NODES
struct kvm_ipc_thread_data {
	int nid;
	struct kvm *kvm;
};
extern volatile bool all_done[];
#endif

static int kvm__create_socket(struct kvm *kvm)
{
	char full_name[PATH_MAX];
	int s;
	struct sockaddr_un local;
	int len, r;

	/* This usually 108 bytes long */
	BUILD_BUG_ON(sizeof(local.sun_path) < 32);

	snprintf(full_name, sizeof(full_name), "%s/%s%s",
		 kvm__get_dir(), kvm->cfg.guest_name, KVM_SOCK_SUFFIX);
	if (access(full_name, F_OK) == 0) {
		pr_err("Socket file %s already exist", full_name);
		return -EEXIST;
	}

	s = socket(AF_UNIX, SOCK_STREAM, 0);
	if (s < 0) {
		perror("socket");
		return s;
	}

	local.sun_family = AF_UNIX;
	strlcpy(local.sun_path, full_name, sizeof(local.sun_path));
	len = strlen(local.sun_path) + sizeof(local.sun_family);
	r = bind(s, (struct sockaddr *)&local, len);
	if (r < 0) {
		perror("bind");
		goto fail;
	}

	r = listen(s, 5);
	if (r < 0) {
		perror("listen");
		goto fail;
	}

//	Somehow this block the app...................
//	POP_DPRINTF(pop_get_nid(),"<%d> %s(): sock name %s established fd %d\n",
//									pop_get_nid(), __func__, full_name, s);
	return s;

fail:
	close(s);
	return r;
}

void kvm__remove_socket(const char *name)
{
	char full_name[PATH_MAX];

	snprintf(full_name, sizeof(full_name), "%s/%s%s",
		 kvm__get_dir(), name, KVM_SOCK_SUFFIX);
	unlink(full_name);
}

/* One name to one sock_fd */
int kvm__get_sock_by_instance(const char *name)
{
	int s, len, r;
	char sock_file[PATH_MAX];
	struct sockaddr_un local;

	snprintf(sock_file, sizeof(sock_file), "%s/%s%s",
		 kvm__get_dir(), name, KVM_SOCK_SUFFIX);
	s = socket(AF_UNIX, SOCK_STREAM, 0);

	local.sun_family = AF_UNIX;
	strlcpy(local.sun_path, sock_file, sizeof(local.sun_path));
	len = strlen(local.sun_path) + sizeof(local.sun_family);

	r = connect(s, (struct sockaddr *)&local, len);
	if (r < 0 && errno == ECONNREFUSED) {
		/* Tell the user clean ghost socket file */
		pr_err("\"%s\" could be a ghost socket file, please remove it",
				sock_file);
		return r;
	} else if (r < 0) {
		return r;
	}

	return s;
}

static bool is_socket(const char *base_path, const struct dirent *dent)
{
	switch (dent->d_type) {
	case DT_SOCK:
		return true;

	case DT_UNKNOWN: {
		char path[PATH_MAX];
		struct stat st;

		sprintf(path, "%s/%s", base_path, dent->d_name);
		if (stat(path, &st))
			return false;

		return S_ISSOCK(st.st_mode);
	}
	default:
		return false;
	}
}

int kvm__enumerate_instances(int (*callback)(const char *name, int fd))
{
	int sock;
	DIR *dir;
	struct dirent *entry;
	int ret = 0;
	const char *path;

	POP_PRINTF("%s(): -a will use name to find "
			"existing VM instance's sock_fd# "
			"LOTS OF FILE IO OPERATIONS\n", __func__);

	path = kvm__get_dir();

	POP_PRINTF("%s(): path '%s'\n", __func__, path);

	dir = opendir(path);
	if (!dir)
		return -errno;

	for (;;) {
		entry = readdir(dir); /* What files under this dir */
		if (!entry)
			break;
		if (is_socket(path, entry)) { /* Target sock files */
			ssize_t name_len = strlen(entry->d_name);
			char *p;

			if (name_len <= KVM_SOCK_SUFFIX_LEN)
				continue;

			p = &entry->d_name[name_len - KVM_SOCK_SUFFIX_LEN];
			if (memcmp(KVM_SOCK_SUFFIX, p, KVM_SOCK_SUFFIX_LEN))
				continue;

			*p = 0;
			/* Will check if gost fike */
			sock = kvm__get_sock_by_instance(entry->d_name);
			if (sock < 0)
				continue;
#if defined(CONFIG_POPCORN_HYPE)
			/* This process is not distributed (not a Popcorn process) */
//			for (i = 0; i < pop_msg_nodes; i++) {
//				printf("NO...this process is not distributed\n");
//				printf("Understand vanilla first\n");
//			}
#endif
			POP_PRINTF("%s(): call with '%s' and sock %d\n",
								__func__, entry->d_name, sock);
			ret = callback(entry->d_name, sock);
			close(sock);
			if (ret < 0)
				break;
		}
	}

	closedir(dir);

	return ret;
}

int kvm_ipc__register_handler(u32 type, void (*cb)(struct kvm *kvm, int fd, u32 type, u32 len, u8 *msg))
{
	if (type >= KVM_IPC_MAX_MSGS)
		return -ENOSPC;

#ifdef CONFIG_POPCORN_HYPE
     POP_PRINTF("\t\t[%d/%d] %s(): register one signal (ipc) handler type %u\n",
				pop_get_nid(), popcorn_gettid(), __func__, type);
#endif

	down_write(&msgs_rwlock);
	msgs[type] = cb;
	up_write(&msgs_rwlock);

	return 0;
}

int kvm_ipc__send(int fd, u32 type)
{
	struct kvm_ipc_head head = {.type = type, .len = 0,};

	if (write_in_full(fd, &head, sizeof(head)) < 0)
		return -1;

	return 0;
}

int kvm_ipc__send_msg(int fd, u32 type, u32 len, u8 *msg)
{
	struct kvm_ipc_head head = {.type = type, .len = len,};

	if (write_in_full(fd, &head, sizeof(head)) < 0)
		return -1;

	if (write_in_full(fd, msg, len) < 0)
		return -1;

	return 0;
}

static int kvm_ipc__handle(struct kvm *kvm, int fd, u32 type, u32 len, u8 *data)
{
	void (*cb)(struct kvm *kvm, int fd, u32 type, u32 len, u8 *msg);

	if (type >= KVM_IPC_MAX_MSGS)
		return -ENOSPC;

#ifdef CONFIG_POPCORN_HYPE
	POP_PRINTF("\n\t[%d/%d/%d] %s(): fd %d -> handle_* like _ft %s\n",
				pop_get_nid(), getpid(), popcorn_gettid(),
				__func__, fd, __FILE__);
	//die_perror("HACJ: I NEVER SAW THIS HAPPENING AND KVM-IPC ALWATS HANGS");
#endif

	down_read(&msgs_rwlock);
	cb = msgs[type];
	up_read(&msgs_rwlock);

	if (cb == NULL) {
		pr_warning("No device handles type %u\n", type);
		return -ENODEV;
	}

	cb(kvm, fd, type, len, data);
#ifdef CONFIG_POPCORN_HYPE
	POP_PRINTF("[%d/%d/%d] %s(): fd %d -> handle_* like _ft done\n\n\n",
				pop_get_nid(), getpid(), popcorn_gettid(),
				__func__, fd);
#endif

	return 0;
}

static int kvm_ipc__new_conn(int fd)
{
	int client;
	struct epoll_event ev;

	client = accept(fd, NULL, NULL);
	if (client < 0)
		return -1;

	ev.events = EPOLLIN | EPOLLRDHUP;
	ev.data.fd = client;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client, &ev) < 0) {
		close(client);
		return -1;
	}

	return client;
}

static void kvm_ipc__close_conn(int fd)
{
	epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
	close(fd);
}

static int kvm_ipc__receive(struct kvm *kvm, int fd)
{
	struct kvm_ipc_head head;
	u8 *msg = NULL;
	u32 n;

	n = read(fd, &head, sizeof(head));
	if (n != sizeof(head))
		goto done;

	msg = malloc(head.len);
	if (msg == NULL)
		goto done;

	n = read_in_full(fd, msg, head.len);
	if (n != head.len)
		goto done;

	kvm_ipc__handle(kvm, fd, head.type, head.len, msg);

	return 0;

done:
	free(msg);
	return -1;
}

static void *kvm_ipc__thread(void *param)
{
	struct epoll_event event;
#ifdef CONFIG_POPCORN_HYPE
	struct kvm_ipc_thread_data {
		int nid;
		struct kvm *kvm;
	};
	struct kvm_ipc_thread_data *t_data = param;
	int my_nid = t_data->nid;
	struct kvm *kvm = t_data->kvm;
#else
	struct kvm *kvm = param;
#endif

#ifdef CONFIG_POPCORN_HYPE
	POP_PRINTF("\t[%d/%d] %s(): this thread \"kvm-ipc\" - "
				"my_nid %d\n",
		pop_get_nid(), popcorn_gettid(), __func__, my_nid);
	if (my_nid) {
		POP_PRINTF("\t[%d/%d] %s(): this thread \"kvm-ipc\" - "
				"replace this thread\n",
				pop_get_nid(), popcorn_gettid(), __func__);
		migrate(my_nid, NULL, NULL);
		POP_PRINTF("\t[%d/%d] %s(): this thread \"kvm-ipc\" - "
				"migrated\n",
				pop_get_nid(), popcorn_gettid(), __func__);
	}

	POP_PRINTF("\t[%d/%d] %s(): this thread \"kvm-ipc\" - "
				"running on this node\n",
				pop_get_nid(), popcorn_gettid(), __func__);

#endif

	//kvm__set_thread_name("kvm-ipc"); /* BUG x 2 times.... confmrmed */
	//prctl(PR_SET_NAME, "kvm-ipc"); /* BUG x 2 times.... confmrmed */
	/* Checked - probably when return to user
		can we not use it or set it on origin */

	for (;;) {
		int nfds;

#ifdef CONFIG_POPCORN_HYPE
		POP_PRINTF("\n\t\t[%d/%d] %s(): epoll wait on "
					"***shared*** %p epoll_fd %d (\"kvm-ipc\")\n\n\n",
					pop_get_nid(), popcorn_gettid(),
					__func__, &event, epoll_fd);

		nfds = epoll_wait(epoll_fd, &event, 1, -1);

		POP_PRINTF("\n\n\n\t\t[%d/%d] %s(): ipc(fd %d) got %d task\n",
					pop_get_nid(), popcorn_gettid(),
					__func__, event.data.fd, nfds);
#else
		nfds = epoll_wait(epoll_fd, &event, 1, -1);
#endif
		if (nfds > 0) {
			int fd = event.data.fd;
#ifdef CONFIG_POPCORN_HYPE
			POP_PRINTF("\t\t[%d/%d] %s(): ioepoll (fd %d) "
					"got %d tasks [dbg] event.events %d\n",
					pop_get_nid(), popcorn_gettid(), __func__,
					epoll_fd, nfds, event.events);
#endif

			if (fd == stop_fd && event.events & EPOLLIN) {
				break;
			} else if (fd == server_fd) {
				int client, r;

				client = kvm_ipc__new_conn(fd);
#ifdef CONFIG_POPCORN_HYPE
				POP_PRINTF("\t\t[%d/%d] %s(): "
					"accept creates a fd %d and put it into epoll_fd %d\n",
					pop_get_nid(), popcorn_gettid(), __func__, client, epoll_fd);
#endif
				/*
				 * Handle multiple IPC cmd at a time
				 */
				do {
#ifdef CONFIG_POPCORN_HYPE
					POP_PRINTF("\t\t[%d/%d] %s(): "
							"call kvm_ipc__receive(kvm, client %d) -> "
							"kvm_ipc__handle() %s\n",
							pop_get_nid(), popcorn_gettid(),
							__func__, client, __FILE__);
#endif
					r = kvm_ipc__receive(kvm, client);
				} while	(r == 0);

			} else if (event.events & (EPOLLERR | EPOLLRDHUP | EPOLLHUP)) {
				kvm_ipc__close_conn(fd);
			} else {
#ifdef CONFIG_POPCORN_HYPE
				POP_PRINTF("\t\t[%d/%d] %s(): "
						"call kvm_ipc__receive(kvm, fd %d) -> "
						"kvm_ipc__handle()\n",
						pop_get_nid(), popcorn_gettid(), __func__, fd);
#endif
				kvm_ipc__receive(kvm, fd);
			}
		}
	}

	return NULL;
}

static void kvm__pid(struct kvm *kvm, int fd, u32 type, u32 len, u8 *msg)
{
	pid_t pid = getpid();
	int r = 0;

    POP_PRINTF("%s(): type %x kvm->vm_state %x\n",
				__func__, type, kvm->vm_state);
	if (type == KVM_IPC_PID)
		r = write(fd, &pid, sizeof(pid));

	if (r < 0)
		pr_warning("Failed sending PID");
}

static void handle_stop(struct kvm *kvm, int fd, u32 type, u32 len, u8 *msg)
{
#ifdef CONFIG_POPCORN_HYPE
    POP_PRINTF("%s(): type %x kvm->vm_state %x\n",
				__func__, type, kvm->vm_state);
#endif
	if (WARN_ON(type != KVM_IPC_STOP || len))
		return;

	kvm__reboot(kvm);
}

/* Pause/resume the guest using SIGUSR2 */
static int is_paused;

static void handle_pause(struct kvm *kvm, int fd, u32 type, u32 len, u8 *msg)
{
#ifdef CONFIG_POPCORN_HYPE
    POP_PRINTF("%s(): type %x kvm->vm_state %x\n",
				__func__, type, kvm->vm_state);
#endif
	if (WARN_ON(len))
		return;

	if (type == KVM_IPC_RESUME && is_paused) {
		kvm->vm_state = KVM_VMSTATE_RUNNING;
//		kvm__continue(kvm); // pause_unlock
		POP_PRINTF("Not called - "
					"called by the target: resume\n");
	} else if (type == KVM_IPC_PAUSE && !is_paused) {
		kvm->vm_state = KVM_VMSTATE_PAUSED;
		ioctl(kvm->vm_fd, KVM_KVMCLOCK_CTRL);
		kvm__pause(kvm);
		POP_PRINTF("Not called - "
					"called by the target: pause\n");
	} else {
		return;
	}

	is_paused = !is_paused;
}

#if defined(CONFIG_POPCORN_HYPE)
/* TODO: find the right number */
//#define MAX_FNAME   256 // killme after retart done
//#define MAX_MSR_ENTRIES 25 // killme after retart done

static void pre_kvm__pause(struct kvm *kvm)
{
	int i;
	// ckpt - pause
	kvm->vm_state = KVM_VMSTATE_PAUSED;
	is_paused = 1;
	POP_PRINTF("%s <_*_> [%d] [[CKPT/RESTART]] %s() - "
			"distribute KVM_KVMCLOCK_CTRL (stop guest time)\n\n\n",
			__FILE__, popcorn_gettid(), __func__);
	ioctl(kvm->vm_fd, KVM_KVMCLOCK_CTRL); //TODO distibute it
	/* replicate at remote */
	for (i = 1; i < kvm->nrcpus; i++) {
		migrate(i, NULL, NULL); // to remote
		POP_PRINTF("%s [%d/%d] [[CKPT/RESTART]] %s() - "
			"REMOTE replicates KVM_KVMCLOCK_CTRL\n",
			__FILE__, pop_get_nid(), popcorn_gettid(), __func__);
		ioctl(kvm->vm_fd, KVM_KVMCLOCK_CTRL);
		migrate(0, NULL, NULL); // back to host
	}
}

extern void *first_mem;
extern void *second_mem;
extern u64 first_mem_size;
extern u64 second_mem_size;
#define FT_ERR 0
#define FT_CKPT 1
#define FT_RESTORE 2
static void handle_ft(struct kvm *kvm, int fd, u32 type, u32 len, u8 *msg)
{
	// move to debug.h <_*_> serial but not vcpu0
	/* Cannot use current_kvm_cpu, OTHERWISE SEGFAULT !!!!!! */
    POP_DBGPRINTF("[dbg] <_*_> [%d] %s(): received signal 'KVM_IPC_FT_*' "
		"double check fd %d %s\n", popcorn_gettid(), __func__, fd, __FILE__);
	POP_PRINTF("<*> [%d/%d/%d] %s %s(): pophype explainasion -- \n"
				"\t====== kvm__pause() + kvm__pause_pophype_migrate_back() "
				"+ save_all_cpu_states(gvcpu[i]);"
				" ======\n\n\n",
				pop_get_nid(), getpid(), popcorn_gettid(),
				__FILE__, __func__);
#if SHOW_POPHYPE_FT_TIME
    int req = FT_ERR;
    //int tvusecs;
    //struct timeval tvstart, tvstop;
    struct timeval tstart, tstop;
    int tusecs;
    gettimeofday(&tstart, NULL);
#endif

    if (WARN_ON(len))
        return;

    if (type == KVM_IPC_FT_CKPT && !is_paused) {
		const char *req_str = "CKPT";
#if SHOW_POPHYPE_FT_TIME
        req = FT_CKPT;
#endif

#define NEW_SCHEME 1
        POP_PRINTF("<_*_> [%d] [[%s]] %s() - called by the target: ft %s\n",
					popcorn_gettid(), req_str, __func__, __FILE__);

        // ckpt - pause
		pre_kvm__pause(kvm);
        POP_PRINTF("\n\n\n\n<_*_> [%d] [[%s]] %s() - "
				"kvm__pause_pophype_migrate("
				"SIGKVMCPUMIGRATE_BACK_W_STATE/SIGKVMFTCKPT) "
				"start %s\n",
				popcorn_gettid(), req_str, __func__, __FILE__);
		kvm__pause_pophype_migrate(kvm, SIGKVMCPUMIGRATE_BACK_W_STATE);
		//kvm__pause_pophype_migrate(kvm, SIGKVMFTCKPT); // TODO: This is not working
		POP_PRINTF("<_*_> [%d] [[CKPT]] %s() - "
					"kvm__pause_pophype_migrate("
					"BACK_W_STATE) done\n\n\n\n",
					popcorn_gettid(), __func__);

		/**********************/
        /* ckpt - resume */
		/**********************/
        kvm->vm_state = KVM_VMSTATE_RUNNING;
		POP_PRINTF("<_*_> [%d/%d/%d] %s(): (ipc_thread) $UNLOCK$\n",
				pop_get_nid(), getpid(), popcorn_gettid(), __func__);
        kvm__continue(kvm); // pause_unlock
		is_paused = 0;
		POP_PRINTF("\n\n\n<_*_> [%d] [[CKPT]] - (ipc_thread) ALL DONE\n\n\n\n",
					popcorn_gettid());
    } else if (type == KVM_IPC_FT_RESTART && !is_paused) {
		const char *req_str = "RESTART";
#if SHOW_POPHYPE_FT_TIME
		req = FT_RESTORE;
#endif
        POP_PRINTF("<_*_> [%d] [[%s]] - %s() - called by the target: ft %s\n",
					popcorn_gettid(), req_str, __func__, __FILE__);

		pre_kvm__pause(kvm);

        POP_PRINTF("\n\n\n\n<_*_> [%d] [[%s]] %s() - "
				"kvm__pause_pophype_migrate(SIGKVMFTRESTART) "
				"start %s\n",
				popcorn_gettid(), req_str, __func__, __FILE__);
		kvm__pause_pophype_migrate(kvm, SIGKVMFTRESTART);

		/**********************/
        /* restart - resume */
		/**********************/
        kvm->vm_state = KVM_VMSTATE_RUNNING;
		POP_PRINTF("<ipc*> [%d/%d/%d] %s(): [%s] $UNLOCK$\n",
			pop_get_nid(), getpid(), popcorn_gettid(), __func__, req_str);
        kvm__continue(kvm); // pause_unlock // POPHYPE HACK
		is_paused = 0;
        POP_PRINTF("\n\n\n<ipc*> [[%s]] - (ipc_thread) ALL DONE\n\n\n\n", req_str);
    } else if (is_paused) {
        POP_PRINTF("\n\n\nBUG at %s:%d: "
					"ft is only supported under running mode\n\n\n\n",
					__FILE__, __LINE__);
    } else {
        printf("\n\n\nBUG at %s:%d\n\n\n", __FILE__, __LINE__);
        BUG_ON(-1);
    }

	if (type == KVM_IPC_FT_CKPT || type == KVM_IPC_FT_RESTART) {
		int i;
		for (i = 0; i < kvm->nrcpus; i++) {
			while (!all_done[i]) { sched_yield(); }
		}
		for (i = 0; i < kvm->nrcpus; i++) {
			all_done[i] = false;
		}
	}
#if SHOW_POPHYPE_FT_TIME
    gettimeofday(&tstop, NULL);
    tusecs = ((tstop.tv_sec - tstart.tv_sec) * 1000000) + (tstop.tv_usec - tstart.tv_usec);
    printf("pophype: lkvm ft \"%s\" [time] = %d ms [[[%d s]]] gettimeofday()\n",
                    req == FT_CKPT ? "ckpt" :
                        req == FT_RESTORE ? "restore" : "ERROR",
                    tusecs / 1000,
                    tusecs / 1000 / 1000);
//} else if (type == KVM_IPC_FT_RESTART && !is_paused) {
#endif
    return;
}
#endif

static void handle_vmstate(struct kvm *kvm, int fd, u32 type, u32 len, u8 *msg)
{
	int r = 0;

#ifdef CONFIG_POPCORN_HYPE
    POP_PRINTF("%s(): type %x kvm->vm_state %x\n",
				__func__, type, kvm->vm_state);
#endif
	if (type == KVM_IPC_VMSTATE)
		r = write(fd, &kvm->vm_state, sizeof(kvm->vm_state));

	if (r < 0)
		pr_warning("Failed sending VMSTATE");
}

/*
 * Serialize debug printout so that the output of multiple vcpus does not
 * get mixed up:
 */
static int printout_done;

static void handle_sigusr1(int sig)
{
	struct kvm_cpu *cpu = current_kvm_cpu;
	int fd = kvm_cpu__get_debug_fd();
#ifdef CONFIG_POPCORN_HYPE
    POP_PRINTF("%s(): type ? kvm->vm_state ? sig %d\n",
				__func__, sig);
#endif
	if (!cpu || cpu->needs_nmi)
		return;

#ifdef CONFIG_POPCORN_HYPE
    POP_PRINTF("\t[%d/%d] %s(): got signal - dump reg, code, pg\n",
					pop_get_nid(), popcorn_gettid(), __func__);
#endif

	dprintf(fd, "\n #\n # vCPU #%ld's dump:\n #\n", cpu->cpu_id);
	kvm_cpu__show_registers(cpu);
	kvm_cpu__show_code(cpu);
	kvm_cpu__show_page_tables(cpu);
	fflush(stdout);
	printout_done = 1;
}

static void handle_debug(struct kvm *kvm, int fd, u32 type, u32 len, u8 *msg)
{
	int i;
	struct debug_cmd_params *params;
	u32 dbg_type;
	u32 vcpu;

#ifdef CONFIG_POPCORN_HYPE
    POP_PRINTF("%s(): type %x kvm->vm_state %x\n",
				__func__, type, kvm->vm_state);
#endif
	if (WARN_ON(type != KVM_IPC_DEBUG || len != sizeof(*params)))
		return;

	params = (void *)msg;
	dbg_type = params->dbg_type;
	vcpu = params->cpu;

	if (dbg_type & KVM_DEBUG_CMD_TYPE_SYSRQ)
		serial8250__inject_sysrq(kvm, params->sysrq);

	if (dbg_type & KVM_DEBUG_CMD_TYPE_NMI) {
		if ((int)vcpu >= kvm->nrcpus)
			return;

		kvm->cpus[vcpu]->needs_nmi = 1;
		pthread_kill(kvm->cpus[vcpu]->thread, SIGUSR1);
	}

	if (!(dbg_type & KVM_DEBUG_CMD_TYPE_DUMP))
		return;

	for (i = 0; i < kvm->nrcpus; i++) {
		struct kvm_cpu *cpu = kvm->cpus[i];

		if (!cpu)
			continue;

		printout_done = 0;

		kvm_cpu__set_debug_fd(fd);
		pthread_kill(cpu->thread, SIGUSR1);
		/*
		 * Wait for the vCPU to dump state before signalling
		 * the next thread. Since this is debug code it does
		 * not matter that we are burning CPU time a bit:
		 */
		while (!printout_done)
			sleep(0);
	}

	close(fd);

	serial8250__inject_sysrq(kvm, 'p');
}

extern pthread_barrier_t barrier_server_fd;
extern pthread_barrier_t barrier_epoll_fd;
extern pthread_barrier_t barrier_estop_fd;
extern pthread_barrier_t barrier_ipc_thread;
int sock = INT_MIN;
//int _epoll_fd = INT_MIN;
//int _sock = INT_MIN;
int kvm_ipc__init(struct kvm *kvm)
{
	int ret;
	struct epoll_event ev = {0};
#ifndef CONFIG_POPCORN_HYPE
	sock = kvm__create_socket(kvm);
#else
//	int __sock = INT_MIN;
//	if (!pop_get_nid())
		sock = kvm__create_socket(kvm);
//	else
//		__sock = kvm__create_socket(kvm);

	pthread_barrier_wait(&barrier_server_fd);
//	if (pop_get_nid()) {
//		if (sock != __sock)
//			POP_DPRINTF(pop_get_nid(),"(BUG) sock not matching %d %d\n",
//							pop_get_nid(), __func__, sock, __sock);
//	}

	POP_DPRINTF(pop_get_nid(), "\t[%d/%d] %s(): server_fd %d (5/sock)\n",
						pop_get_nid(), popcorn_gettid(), __func__, sock);
#endif
	server_fd = sock;

#ifndef CONFIG_POPCORN_HYPE
	epoll_fd = epoll_create(KVM_IPC_MAX_MSGS);
#else
	/* do this to the fd happens before this one */
//	int __epoll_fd = INT_MIN;
//	if (!pop_get_nid())
		epoll_fd = epoll_create(KVM_IPC_MAX_MSGS);
//	else
//		__epoll_fd = epoll_create(KVM_IPC_MAX_MSGS);

	pthread_barrier_wait(&barrier_epoll_fd);
//	if (pop_get_nid())
//		if (__epoll_fd != epoll_fd) {
//			POP_SANI_PRINTF(pop_get_nid(),
//				"(BUG) epoll_fd not matching %d %d\n",
//									__epoll_fd, epoll_fd);
//			BUG_ON(1);
//		}
	POP_DPRINTF(pop_get_nid(), "\t[<%d>/%d] %s(): epoll_fd %d (6)\n",
				pop_get_nid(), popcorn_gettid(), __func__, epoll_fd);
#endif
	if (epoll_fd < 0) {
		perror("epoll_create");
		ret = epoll_fd;
		goto err;
	}

	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = sock;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock, &ev) < 0) {
		pr_err("Failed adding socket to epoll");
		ret = -EFAULT;
		goto err_epoll;
	}

#ifndef CONFIG_POPCORN_HYPE
	stop_fd = eventfd(0, 0);
#else
	int __stop_fd = INT_MIN;
	if (!pop_get_nid())
		stop_fd = eventfd(0, 0);
	else {
		__stop_fd = eventfd(0, 0);
		if (__stop_fd < 0) {
			perror("eventfd");
			ret = __stop_fd;
			goto err_epoll;
		}
	}

	pthread_barrier_wait(&barrier_estop_fd);
//	if (pop_get_nid())
//		if (stop_fd != __stop_fd) {
//			POP_SANI_PRINTF(pop_get_nid(),
//				"(BUG) stop_fd not matching %d %d\n",
//									stop_fd, __stop_fd);
//			BUG_ON(1);
//		}
	POP_PRINTF("\t[<%d>/%d] %s(): eventfd() stop_fd fd %d (7/SYSC_eventfd2)\n",
						pop_get_nid(), popcorn_gettid(), __func__, stop_fd);
#endif
	if (stop_fd < 0) {
		perror("eventfd");
		ret = stop_fd;
		goto err_epoll;
	}

	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = stop_fd;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, stop_fd, &ev) < 0) {
		pr_err("Failed adding stop event to epoll");
		ret = -EFAULT;
		goto err_stop;
	}

#ifdef CONFIG_POPCORN_HYPE
    POP_PRINTF("\t[%d/%d] %s(): going to create kvm_ipc__thread "
			"***pthread on remote (DANGEROUS)*** \"kvm-ipc\" (serial)\n",
			pop_get_nid(), popcorn_gettid(), __func__);

//	// popcorn cannot create pthread because of race condition? (testing)
//	// do it one by one (barrier) (doesn't work)
//
//	//pthread_barrier_wait(&barrier_ipc_thread);
//
//	/* Check: kvm_ipc__handle() */
//	POP_PRINTF("HACK: for solving popcorn bug but this is not the best way. "
//		"I should turn it on and see if kvm_ipc__handle() if called or not\n");
//	/*
//	popcorn_serial_threads_start();
//	if (pthread_create(&thread, NULL, kvm_ipc__thread, kvm) != 0) {
//		pr_err("Failed starting IPC thread");
//		ret = -EFAULT;
//		goto err_stop;
//	}
//	popcorn_serial_threads_end();
//	*/
//
//	POP_PRINTF("\n\t\t[%d/%d] %s(): kvm_ipc check point\n",
//				pop_get_nid(), popcorn_gettid(), __func__);
//
//	POP_PRINTF("\n\t\t[%d/%d] %s(): kvm_ipc comments are not "
//				"trustable. IPC thread must be created at remote. "
//				"TODO TODO: FIXING IT NOW!!!!!!!!\n\n",
//				pop_get_nid(), popcorn_gettid(), __func__);
//	POP_PRINTF("\n\t\t[%d/%d] %s(): checkpint 3\n",
//			pop_get_nid(), popcorn_gettid(), __func__);
	int my_nid = pop_get_nid();
	if (!my_nid) {
		int i;
		POP_PRINTF("\n\t\t<*>[%d/%d] %s():\n",
				pop_get_nid(), popcorn_gettid(), __func__);
		for (i = 0; i < kvm->nrcpus; i++) {
			struct kvm_ipc_thread_data *t_data =
				malloc(sizeof(struct kvm_ipc_thread_data)); // TODO free
			t_data->nid = i;
			t_data->kvm = kvm;

			POP_PRINTF("\n\t\t[%d/%d] %s(): %d/%d\n",
				pop_get_nid(), popcorn_gettid(), __func__, i, kvm->nrcpus);
			if (pthread_create(&_thread[i], NULL,
						kvm_ipc__thread, t_data) != 0) {
				pr_err("Failed starting IPC thread");
				ret = -EFAULT;
				goto err_stop;
			}
		}
	} // TODO exit path
/*******
    for (i = 0; i < kvm->nrcpus; i++) {
        kvm->cpus[i]->cpu_id = i;
        if (pthread_create(&kvm->cpus[i]->thread, NULL,
                            kvm_cpu_thread, kvm->cpus[i]) != 0) {
            die("unable to create KVM VCPU thread");
        }
    }
*/

#else
	if (pthread_create(&thread, NULL, kvm_ipc__thread, kvm) != 0) {
		pr_err("Failed starting IPC thread");
		ret = -EFAULT;
		goto err_stop;
	}
#endif

#ifdef CONFIG_POPCORN_HYPE
    POP_PRINTF("\t[%d/%d] %s(): register ipc handler =START=\n",
			pop_get_nid(), popcorn_gettid(), __func__);
#endif
	kvm_ipc__register_handler(KVM_IPC_PID, kvm__pid);
	kvm_ipc__register_handler(KVM_IPC_DEBUG, handle_debug);
	kvm_ipc__register_handler(KVM_IPC_PAUSE, handle_pause);
	kvm_ipc__register_handler(KVM_IPC_RESUME, handle_pause);
	kvm_ipc__register_handler(KVM_IPC_STOP, handle_stop);
	kvm_ipc__register_handler(KVM_IPC_VMSTATE, handle_vmstate);
    kvm_ipc__register_handler(KVM_IPC_FT_CKPT, handle_ft); // popcorn
    kvm_ipc__register_handler(KVM_IPC_FT_RESTART, handle_ft); // popcorn
	signal(SIGUSR1, handle_sigusr1);

	return 0;

err_stop:
	close(stop_fd);
err_epoll:
	close(epoll_fd);
err:
	return ret;
}
base_init(kvm_ipc__init);

int kvm_ipc__exit(struct kvm *kvm)
{
	u64 val = 1;
	int ret;

	ret = write(stop_fd, &val, sizeof(val));
	if (ret < 0)
		return ret;

	close(server_fd);
	close(epoll_fd);

	kvm__remove_socket(kvm->cfg.guest_name);

	return ret;
}
base_exit(kvm_ipc__exit);
