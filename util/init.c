#include <linux/list.h>
#include <linux/kernel.h>

#include "kvm/kvm.h"
#include "kvm/util-init.h"

#include <popcorn/utils.h>

#define PRIORITY_LISTS 10

static struct hlist_head init_lists[PRIORITY_LISTS];
static struct hlist_head exit_lists[PRIORITY_LISTS];

int init_list_add(struct init_item *t, int (*init)(struct kvm *),
			int priority, const char *name)
{
	t->init = init;
	t->fn_name = name;
	hlist_add_head(&t->n, &init_lists[priority]);

	return 0;
}

int exit_list_add(struct init_item *t, int (*init)(struct kvm *),
			int priority, const char *name)
{
	t->init = init;
	t->fn_name = name;
	hlist_add_head(&t->n, &exit_lists[priority]);

	return 0;
}

int init_list__init(struct kvm *kvm)
{
	unsigned int i;
	int r = 0;
	struct init_item *t;

	printf("[%d/%d] %s(): dbg 11\n",
            pop_get_nid(), popcorn_gettid(), __func__);

	POP_DPRINTF(pop_get_nid(), "[%d] %s(): ---START--- "
			"ARRAY_SIZE(init_lists) = %d\n\n",
			pop_get_nid(), __func__, ARRAY_SIZE(init_lists));

	for (i = 0; i < ARRAY_SIZE(init_lists); i++)
		hlist_for_each_entry(t, &init_lists[i], n) {
			POP_DPRINTF(pop_get_nid(), "[%d] class %d %s()\n",
								pop_get_nid(), i, t->fn_name);
			r = t->init(kvm);
			if (r < 0) {
				pr_warning("Failed init: %s\n", t->fn_name);
				goto fail;
			}
		}

	POP_DPRINTF(pop_get_nid(), "[%d] %s(): ---END---\n\n",
									pop_get_nid(), __func__);

#ifdef CONFIG_POPCORN_HYPE
	/* There is another file fd 15 open read close except sysconf() */
#endif
fail:
	return r;
}

int init_list__exit(struct kvm *kvm)
{
	int i;
	int r = 0;
	struct init_item *t;

	for (i = ARRAY_SIZE(exit_lists) - 1; i >= 0; i--)
		hlist_for_each_entry(t, &exit_lists[i], n) {
			r = t->init(kvm);
			if (r < 0) {
				pr_warning("%s failed.\n", t->fn_name);
				goto fail;
			}
		}
fail:
	return r;
}
