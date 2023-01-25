#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>

#include <linux/kernel.h>
#include <linux/kvm.h>
#include <linux/types.h>

#include "kvm/ioeventfd.h"
#include "kvm/kvm.h"
#include "kvm/util.h"

#include <popcorn/utils.h>

#define IOEVENTFD_MAX_EVENTS	20

//#ifdef CONFIG_POPCORN_HYPE
////static struct	epoll_event events[MAX_POPCORN_NODES][IOEVENTFD_MAX_EVENTS];
//static struct	epoll_event events[IOEVENTFD_MAX_EVENTS];
//#else
//static struct	epoll_event events[IOEVENTFD_MAX_EVENTS];
//#endif
static int		epoll_fd = INT_MIN, epoll_stop_fd = INT_MIN;
static LIST_HEAD(used_ioevents);
static bool	ioeventfd_avail;

static void *ioeventfd__thread(void *param)
{
	u64 tmp = 1;
#ifdef CONFIG_POPCORN_HYPE
	int *nid = param;
	/* uevent - cannot use 2d array, leading to different uevent ptr before/after epoll_wait() */
	static struct epoll_event events[IOEVENTFD_MAX_EVENTS];

	if (*nid != pop_get_nid()) {
		POP_PRINTF("\t[%d/%d] %s(): migrate \"ioeventfd-worker\" to [%d]\n",
						pop_get_nid(), popcorn_gettid(), __func__, *nid);
		migrate(*nid, NULL, NULL);
	}

	POP_PRINTF("\t[%d/%d] %s(): this thread \"ioeventfd-worker\"\n",
							pop_get_nid(), popcorn_gettid(), __func__);
	POP_PRINTF("\t\t[net %d/%d] %s(): "
				"peek uninitialized events[0] fd %d, ptr %p \n",
				pop_get_nid(), popcorn_gettid(), __func__,
				events[0].data.fd, events[0].data.ptr);
#endif
	//kvm__set_thread_name("ioeventfd-worker");
	//BUG no as ofent as kvm-ipc (which is created at remote

	for (;;) {
		int nfds, i;

#ifdef CONFIG_POPCORN_HYPE
		POP_PRINTF("\t\t\t[net %d/%d] %s(): io epoll wait on "
				"***private*** uevent %p (\"ioeventfd-worker\")\n",
				pop_get_nid(), popcorn_gettid(), __func__, events);

		POP_PRINTF("\t\t\t[net %d/%d][dbg] %s(): wait on fd %d [dbg] uevent %p\n",
				pop_get_nid(), popcorn_gettid(), __func__, epoll_fd, events);

#if POPHYPE_USR_SANITY_CHECK
		void* old_uevent = events;
#endif
		nfds = epoll_wait(epoll_fd, events, IOEVENTFD_MAX_EVENTS, -1);

#if POPHYPE_USR_SANITY_CHECK
		/* The net-mq bug - uevent's ptr changed before/after epoll_wait()... */
		if ((u64)events > (u64)old_uevent) {
			POP_PRINTF("\t\t[ERROR] uevent old %p new %p diff (+)%p\n",
				old_uevent, events, (void*)((u64)events - (u64)old_uevent));
		}
#endif

		POP_PRINTF("\t\t[net %d/%d] %s(): io epoll "
				"got (nfds) %d tasks on fd %d - "
				"peek events[0] fd %d, ptr %p [dbg] uevent %p\n",
				pop_get_nid(), popcorn_gettid(), __func__, nfds, epoll_fd,
				events[0].data.fd, events[0].data.ptr, events);

#else
		nfds = epoll_wait(epoll_fd, events, IOEVENTFD_MAX_EVENTS, -1);
#endif


#if POPHYPE_USR_SANITY_CHECK
		/* We've moved uevent to stack to solve the bug. It was 2D arrary which is buggy.
			Check if REMOTE touches here. */
		BUG_ON(*nid);
#endif

		for (i = 0; i < nfds; i++) {
			struct ioevent *ioevent;

#ifdef CONFIG_POPCORN_HYPE
			POP_PRINTF("\t\t[net %d/%d] %s():  \"events[nid %d][i %d].data.ptr [%p]\" "
					"events[i].data.fd(=.ptr) %d =? epoll_stop_fd %d "
					"(same=exit) [[[uevent %p]]] (match in fs/eventpoll.c)"
					" WILL DIE SOON (.data is a union so it must be *ptr)\n",
						pop_get_nid(), popcorn_gettid(), __func__,
						*nid, i, events[i].data.ptr,
						events[i].data.fd, epoll_stop_fd, events);

			if (events[i].data.fd == epoll_stop_fd)
				goto done;

			ioevent = events[i].data.ptr;
			POP_PRINTF("\t\t[net %d/%d] %s(): ioepoll events[nid %d][i %d] "
					"(data.fd=[*ptr] %p(ptr) %d(fd)) "
					"got task->ioevent->fn %p fn_ptr %p #%d/%d (remove fd check just use ptr)\n",
					pop_get_nid(), popcorn_gettid(), __func__,
					*nid, i, events[i].data.ptr,
					ioevent->fd, ioevent->fn, ioevent->fn_ptr, i, nfds);
#if POPHYPE_USR_SANITY_CHECK
			BUG_ON(!ioevent); // The uevent ptr got changed bug
#endif
#else
			if (events[i].data.fd == epoll_stop_fd)
				goto done;

			ioevent = events[i].data.ptr;
#endif

			if (read(ioevent->fd, &tmp, sizeof(tmp)) < 0)
				die("Failed reading event");

			ioevent->fn(ioevent->fn_kvm, ioevent->fn_ptr);
		}
	}

done:
	tmp = write(epoll_stop_fd, &tmp, sizeof(tmp));

	return NULL;
}

static int ioeventfd__start(void)
{
	pthread_t thread;

	if (!ioeventfd_avail)
		return -ENOSYS;

#ifdef CONFIG_POPCORN_HYPE
	int ret, nid = pop_get_nid();
	migrate(HOST, NULL, NULL);
	ret = pthread_create(&thread, NULL, ioeventfd__thread, &nid);
	migrate(nid, NULL, NULL);
	POP_PRINTF("\t[%d/%d] %s(): created on host and "
			"now migrate back to [%d] \"ioeventfd-worker\"\n",
				pop_get_nid(), popcorn_gettid(), __func__, nid);
	return ret;
#else
	return pthread_create(&thread, NULL, ioeventfd__thread, NULL);
#endif
}

extern pthread_barrier_t barrier_io_fd;
extern pthread_barrier_t barrier_epollio_fd;
extern pthread_barrier_t barrier_estopio_fd;
int ioeventfd__init(struct kvm *kvm)
{
#ifndef CONFIG_POPCORN_HYPE
//	if (pop_get_nid()) { return 0; }
#endif
	struct epoll_event epoll_event = {.events = EPOLLIN};
	int r;

#ifndef CONFIG_POPCORN_HYPE
	ioeventfd_avail = kvm__supports_extension(kvm, KVM_CAP_IOEVENTFD);
#else
////	int __ioeventfd_avail = INT_MIN;
////	if (!pop_get_nid())
		ioeventfd_avail = kvm__supports_extension(kvm, KVM_CAP_IOEVENTFD);
////	else
////		__ioeventfd_avail = kvm__supports_extension(kvm, KVM_CAP_IOEVENTFD);
//	pthread_barrier_wait(&barrier_io_fd);
////	if (pop_get_nid())
////		if (__ioeventfd_avail != ioeventfd_avail) {
////			POP_SANI_PRINTF(pop_get_nid(), "(BUG) io_fd not matching %d %d\n",
////											__ioeventfd_avail, ioeventfd_avail);
////			BUG_ON(1);
////		}
//
//	POP_DPRINTF(pop_get_nid(), "\t<%d> %s(): ioeventfd_avail %d\n",
//							pop_get_nid(), __func__, ioeventfd_avail);
//	/* ioeventfd_avail: 1 */
//	/* This is fatal since pophype need fds aligned */
#endif
	if (!ioeventfd_avail)
		return 1; /* Not fatal, but let caller determine no-go. */

#ifndef CONFIG_POPCORN_HYPE
	epoll_fd = epoll_create(IOEVENTFD_MAX_EVENTS);
#else
//	int __epoll_fd = INT_MIN;
//	if (!pop_get_nid())
		epoll_fd = epoll_create(IOEVENTFD_MAX_EVENTS);
//	else
//		__epoll_fd = epoll_create(IOEVENTFD_MAX_EVENTS);
	pthread_barrier_wait(&barrier_epollio_fd);
//	if (pop_get_nid()) {
//		if (__epoll_fd != epoll_fd) {
//			POP_SANI_PRINTF(pop_get_nid(),
//				"(BUG) epool_stopio_fd not matching %d %d\n",
//										__epoll_fd, epoll_fd);
//			BUG_ON(1);
//		}
//	}

	POP_PRINTF("\t<%d> %s(): epoll_fd %d (8)  for ioevent "
			"(net-mq: [dbg] when making mq in effect)\n",
					pop_get_nid(), __func__, epoll_fd);
#endif
	if (epoll_fd < 0)
		return -errno;

#ifndef CONFIG_POPCORN_HYPE
	epoll_stop_fd = eventfd(0, 0);
#else
	int __epoll_stop_fd = INT_MIN;
	if (!pop_get_nid())
		epoll_stop_fd = eventfd(0, 0);
	else
		__epoll_stop_fd = eventfd(0, 0);

	pthread_barrier_wait(&barrier_estopio_fd);

	if (pop_get_nid()) {
		if (__epoll_stop_fd != epoll_stop_fd) {
			POP_SANI_PRINTF(pop_get_nid(),
				"(BUG) epool_stopio_fd not matching %d %d\n",
								__epoll_stop_fd, epoll_stop_fd);
			BUG_ON(1);
		}
	} else {
		POP_PRINTF("\t[%d/%d] %s(): eventfd() epoll_stop_fd fd %d "
				"(9/SYSC_eventfd2)\n",
				pop_get_nid(), popcorn_gettid(), __func__, epoll_stop_fd);
	}
#endif
	epoll_event.data.fd = epoll_stop_fd;

#ifdef CONFIG_POPCORN_HYPE
	POP_PRINTF("\t[net %d/%d] %s(): epoll add epoll_stop_fd %d to epoll_fd %d\n",
		pop_get_nid(), popcorn_gettid(), __func__, epoll_stop_fd, epoll_fd);
#endif
	r = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, epoll_stop_fd, &epoll_event);
	if (r < 0)
		goto cleanup;

#ifdef CONFIG_POPCORN_HYPE
	POP_PRINTF("\t[net %d/%d] %s(): going to create an io thread (2) "
			"\"ioeventfd-worker\" (serial)\n",
			pop_get_nid(), popcorn_gettid(), __func__);
	//popcorn_serial_threads_start();
	r = ioeventfd__start();
	//popcorn_serial_threads_end();
#else
	r = ioeventfd__start();
#endif
	if (r < 0)
		goto cleanup;

	r = 0;

	return r;

cleanup:
	close(epoll_stop_fd);
	close(epoll_fd);

	return r;
}
base_init(ioeventfd__init);

int ioeventfd__exit(struct kvm *kvm)
{
	u64 tmp = 1;
	int r;

	if (!ioeventfd_avail)
		return 0;

	r = write(epoll_stop_fd, &tmp, sizeof(tmp));
	if (r < 0)
		return r;

	r = read(epoll_stop_fd, &tmp, sizeof(tmp));
	if (r < 0)
		return r;

	close(epoll_fd);
	close(epoll_stop_fd);

	return 0;
}
base_exit(ioeventfd__exit);

int ioeventfd__add_event(struct ioevent *ioevent, int flags)
{
	struct kvm_ioeventfd kvm_ioevent;
	struct epoll_event epoll_event;
	struct ioevent *new_ioevent;
	int event, r;

	if (!ioeventfd_avail)
		return -ENOSYS;

	new_ioevent = malloc(sizeof(*new_ioevent));
	if (new_ioevent == NULL)
		return -ENOMEM;

	*new_ioevent = *ioevent;
	event = new_ioevent->fd;

	kvm_ioevent = (struct kvm_ioeventfd) {
		.addr		= ioevent->io_addr,
		.len		= ioevent->io_len,
		.datamatch	= ioevent->datamatch,
		.fd		= event,
		.flags		= KVM_IOEVENTFD_FLAG_DATAMATCH,
	};

	/*
	 * For architectures that don't recognize PIO accesses, always register
	 * on the MMIO bus. Otherwise PIO accesses will cause returns to
	 * userspace.
	 */
	if (KVM_IOEVENTFD_HAS_PIO && flags & IOEVENTFD_FLAG_PIO)
		kvm_ioevent.flags |= KVM_IOEVENTFD_FLAG_PIO;

	r = ioctl(ioevent->fn_kvm->vm_fd, KVM_IOEVENTFD, &kvm_ioevent);
	if (r) {
		r = -errno;
		goto cleanup;
	}

	if (flags & IOEVENTFD_FLAG_USER_POLL) {
		epoll_event = (struct epoll_event) {
			.events		= EPOLLIN,
			.data.ptr	= new_ioevent,
		};

		r = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, event, &epoll_event);
		if (r) {
			r = -errno;
			goto cleanup;
		}
	}

	new_ioevent->flags = kvm_ioevent.flags;
	list_add_tail(&new_ioevent->list, &used_ioevents);

	return 0;

cleanup:
	free(new_ioevent);
	return r;
}

int ioeventfd__del_event(u64 addr, u64 datamatch)
{
	struct kvm_ioeventfd kvm_ioevent;
	struct ioevent *ioevent;
	u8 found = 0;

	if (!ioeventfd_avail)
		return -ENOSYS;

	list_for_each_entry(ioevent, &used_ioevents, list) {
		if (ioevent->io_addr == addr &&
		    ioevent->datamatch == datamatch) {
			found = 1;
			break;
		}
	}

	if (found == 0 || ioevent == NULL)
		return -ENOENT;

	kvm_ioevent = (struct kvm_ioeventfd) {
		.fd			= ioevent->fd,
		.addr			= ioevent->io_addr,
		.len			= ioevent->io_len,
		.datamatch		= ioevent->datamatch,
		.flags			= ioevent->flags
					| KVM_IOEVENTFD_FLAG_DEASSIGN,
	};

	ioctl(ioevent->fn_kvm->vm_fd, KVM_IOEVENTFD, &kvm_ioevent);

	epoll_ctl(epoll_fd, EPOLL_CTL_DEL, ioevent->fd, NULL);

	list_del(&ioevent->list);

	close(ioevent->fd);
	free(ioevent);

	return 0;
}
