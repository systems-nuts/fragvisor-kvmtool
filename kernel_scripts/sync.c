/*
 * sync.c
 * Copyright (C) 2018 jackchuang <jackchuang@mir7>
 *
 * Distributed under terms of the MIT license.
 */
#include <popcorn/debug.h>
#include <linux/syscalls.h>

// TODO: take ip into consideration

unsigned long long plock_system_tso_wr_cnt = 0;
//TODO lock

void collect_tso_wr(void)
{
	if (current->plock_total_tso_wr_cnt) {
		// TODO lock
		plock_system_tso_wr_cnt += current->plock_total_tso_wr_cnt;
		// TODO lock
		printk("[%d]: exit contributs #%llu %llu -> system %llu\n",
								current->pid, current->plock_cnt,
								current->plock_total_tso_wr_cnt,
								plock_system_tso_wr_cnt);
	}
}

void clean_tso_wr(void)
{
	plock_system_tso_wr_cnt = 0;	
}

#ifdef CONFIG_POPCORN
SYSCALL_DEFINE2(popcorn_lock, int, a, void __user *, b)
{
	if (current->plock || current->plock_tso_wr_cnt || current->plock_tso_wx_cnt)
		PCNPRINTK_ERR("BUG plock order violation when \"lock\"\n");

	current->plock = true;

	current->plock_cnt++;
	//printk("[%d] %s(): %llu\n",
	//		current->pid, __func__, current->plock_cnt);
	return 0;
}

SYSCALL_DEFINE2(popcorn_unlock, int, a, void __user *, b)
{
	if (!current->plock)
		PCNPRINTK_ERR("BUG plock order violation when \"unlock\"\n");
	if (!current->plock_tso_wr_cnt)
			//|| !current->plock_tso_wx_cnt)
		PCNPRINTK_ERR("WARNNING no benefits here\n");

	//printk("[%d] %s(): #%llu tso_wr %llu/%llu\n", current->pid, __func__,
	//					current->plock_cnt, current->plock_tso_wr_cnt,
	//					current->plock_total_tso_wr_cnt);
	current->plock_tso_wr_cnt = 0;
	current->plock_tso_wx_cnt = 0;

	current->plock = false;
	return 0;
}
#else // CONFIG_POPCORN
SYSCALL_DEFINE2(popcorn_lock, int, a void __user *, b)
{
	PCNPRINTK_ERR("Kernel is not configured to use popcorn\n");
	return -EPERM;
}

SYSCALL_DEFINE2(popcorn_unlock, int, a, void __user *, b)
{
	PCNPRINTK_ERR("Kernel is not configured to use popcorn\n");
	return -EPERM;
}
#endif

