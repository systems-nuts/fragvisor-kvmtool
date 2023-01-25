/*
 * builtin-ft.h
 * Copyright (C) 2020 jackchuang <horenc@vt.edu>
 *
 * Distributed under terms of the MIT license.
 */
#ifndef BUILTIN_FT_H
#define BUILTIN_FT_H


#include <kvm/util.h>

int kvm_cmd_ft(int argc, const char **argv, const char *prefix, int mode);
int kvm_cmd_ft_ckpt(int argc, const char **argv, const char *prefix);
int kvm_cmd_ft_restart(int argc, const char **argv, const char *prefix);
void kvm_ft_help(void) NORETURN;


#endif /* !BUILTIN_FT_H */
