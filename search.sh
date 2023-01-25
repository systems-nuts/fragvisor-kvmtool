#! /bin/sh
#
# search.sh
# Copyright (C) 2020 jackchuang <jackchuang@mir>
#
# Distributed under terms of the MIT license.
#


grep --color=always -r $1 \
--exclude-dir=pophype_make_ramdisk \
--exclude-dir=busybox-1.30.1 \
--exclude-dir=x86_0.img \
--exclude-dir=lkvm.asm \
| egrep -v "vmlinux.asm|elf|pit2_bug_kern_log|lkvm.asm|vm_good.log|cscope" \
| egrep -v "powerpc|\.git|\.o\.d|jack_analize_kvmtool|log_working_mq" \
| egrep -v "patch|xxx" \
| grep $1
#--color=always
#--exclude-dir={\*pophype_make_ramdisk,busybox-1.30.1,x86_0.img,lkvm.asm\*}

#--exclude-dir=busybox-1.30.1/ --exclude-dir=x86_0.img --exclude-dir=lkvm.asm
