#! /bin/bash
#
# jack_objdump.sh
# Copyright (C) 2019 jackchuang <jackchuang@echo4>
#
# Distributed under terms of the MIT license.
#
host=`uname -a |awk '{print$2}' | sed s/[a-z]//g`
name=`date '+%x_%X'|sed 's/\///g'|sed 's/\:/_/g' |sed 's/ PM//g' |sed 's/ AM//g'`
output="xxx${name}_${host}.asm"
echo "Writting to \" ${output} \""
echo "TODO manually to \" dmesg_${name}_${host} \""
objdump -dSlr vmlinux > ~/share/${output}

