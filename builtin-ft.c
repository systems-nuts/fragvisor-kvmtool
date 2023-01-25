/*
 * builtin-ft.c
 * Copyright (C) 2020 jackchuang <jackchuang@mir7>
 *
 * Distributed under terms of the MIT license.
 */
#include <kvm/util.h>
#include <kvm/kvm-cmd.h>
#include <kvm/builtin-ft.h>
#include <kvm/builtin-list.h>
#include <kvm/kvm.h>
#include <kvm/parse-options.h>
#include <kvm/kvm-ipc.h>

#include <stdio.h>
#include <string.h>
#include <signal.h>

#include <popcorn/utils.h>

static bool all;
static const char *instance_name;

static const char * const ft_usage[] = {
    "lkvm ft [--allTODO] [-n nameTODO]",
    NULL
};

static const struct option ft_options[] = {
    OPT_GROUP("General options:"),
    OPT_BOOLEAN('a', "allTODO", &all, "ft all instances"),
    OPT_STRING('n', "nameTODO", &instance_name, "name", "Instance name"),
    OPT_END()
};

static void parse_ft_options(int argc, const char **argv)
{
    while (argc != 0) {
        argc = parse_options(argc, argv, ft_options, ft_usage,
                PARSE_OPT_STOP_AT_NON_OPTION);
        if (argc != 0)
            kvm_ft_help();
    }
}

void kvm_ft_help(void)
{
    usage_with_options(ft_usage, ft_options);
}

static int do_ft_ckpt(const char *name, int sock)
{
    int r;
    int vmstate;

	POP_PRINTF("%s(): name '%s' sock %d\n", __func__, name, sock);

    vmstate = get_vmstate(sock); /* IPC -> handle_vmstate */ // read local sock file(fd)
	POP_PRINTF("%s(): (local read sock) ret = vmstate %d\n",
									__func__, vmstate);
    if (vmstate < 0)
        return vmstate;
    if (vmstate == KVM_VMSTATE_PAUSED) {
        POP_PRINTF("Guest %s is already paused. "
					"Do ft when it's running.\n", name);
        return 0;
    }

	POP_PRINTF("%s(): about to (remote write sock) sock %d\n",
												__func__, sock);

	/* This is just a sender */
    r = kvm_ipc__send(sock, KVM_IPC_FT_CKPT);
    if (r)
        return r;

	POP_PRINTF("%s(): (remote wrote sock) ret = r %d\n",
											__func__, r);

    POP_PRINTF("Guest %s fting CKPT signal sent\n", name);

    return 0;
}


int kvm_cmd_ft_ckpt(int argc, const char **argv, const char *prefix)
{
    int instance;
    int r;

//#if !POPHYPE_NOPRINT
	POP_PRINTF("%s(): CKPT\n", __func__);
//    dump_stack();
//#endif

    parse_ft_options(argc, argv);

	POP_PRINTF("%s(): done parsing\n", __func__);

    if (all) /* -a */
        return kvm__enumerate_instances(do_ft_ckpt);

    if (instance_name == NULL)
        kvm_ft_help();

	/* lkvm proces name (gues-xxxx) to sock_fd */
    instance = kvm__get_sock_by_instance(instance_name);

    if (instance <= 0) {
        die("Failed locating instance");
	}

	/* one instance */
    r = do_ft_ckpt(instance_name, instance);

    close(instance);

    return r;
}

static int do_ft_restart(const char *name, int sock)
{
    int r;
    int vmstate;

    vmstate = get_vmstate(sock);
    if (vmstate < 0)
        return vmstate;
    if (vmstate == KVM_VMSTATE_PAUSED) {
        POP_PRINTF("Guest %s is already paused. Do ft when it's running.\n", name);
        return 0;
    }

	/* This is just a sender */
    r = kvm_ipc__send(sock, KVM_IPC_FT_RESTART);
    if (r)
        return r;

    POP_PRINTF("Guest %s fting RESTART sigal sent\n", name);

    return 0;
}


int kvm_cmd_ft_restart(int argc, const char **argv, const char *prefix)
{
    int instance;
    int r;

//#if !POPHYPE_NOPRINT
	POP_PRINTF("%s(): RESTART\n", __func__);
//    dump_stack();
//#endif

    parse_ft_options(argc, argv);

    if (all) /* -a */
        return kvm__enumerate_instances(do_ft_restart);

    if (instance_name == NULL)
        kvm_ft_help();

    instance = kvm__get_sock_by_instance(instance_name);

    if (instance <= 0) {
        die("Failed locating instance");
	}

	/* one instance */
    r = do_ft_restart(instance_name, instance);

    close(instance);

    return r;
}
