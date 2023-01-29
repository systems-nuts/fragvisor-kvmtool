#! /bin/bash
#
# copy_to_desktop_dropbox.sh
# Copyright (C) 2018 jackchuang <jackchuang@mir7>
#
# Distributed under terms of the MIT license.
#



# my proxy computer
#PROXY_IN_LAB="10.1.1.146"
PROXY_IN_LAB="10.1.1.145"


echo "set #define VM_TESTING 0 in ./include/popcorn/pcn_kmsg.h"
sed -i 's/#define VM_TESTING 1/#define VM_TESTING 0/g' include/popcorn/pcn_kmsg.h

git diff origin/master > xxx.patch && \
scp xxx.patch $PROXY_IN_LAB:/home/jackchuang/Dropbox/
scp msg_layer/msg_test.c $PROXY_IN_LAB:/home/jackchuang/Dropbox/
scp msg_layer/rdma.c $PROXY_IN_LAB:/home/jackchuang/Dropbox/
scp msg_layer/common.h $PROXY_IN_LAB:/home/jackchuang/Dropbox/
echo "waiting for sync"
sleep 5
ssh $PROXY_IN_LAB "cd ~/Dropbox; ./copy_patch_to_echo_share.sh;"
sleep 5

echo "set #define VM_TESTING 1 in include/popcorn/pcn_kmsg.h"
sed -i 's/#define VM_TESTING 0/#define VM_TESTING 1/g' include/popcorn/pcn_kmsg.h

echo "rename .config to -mwpf"
echo "rename .config to -mwpf"
echo "rename .config to -mwpf"

echo "compile instead of sleep 20"
echo "compile instead of sleep 20"
echo "compile instead of sleep 20"
echo "compile instead of sleep 20"
echo "compile instead of sleep 20"
#if [[ $1 == "1" ]]; then
#	echo "grace sleeping 5s"; sleep 5
#	echo "auto ./apply_and_reboot.sh"
#	ssh echo5 ./apply_and_reboot.sh&
#	ssh echo4 ./apply_and_reboot.sh&
#fi
#echo "sleep 20s then compile the kernel"
make -j32
ret=$?
if [[ $ret != 0 ]]; then
	echo "DIE: $1"
	exit -1
fi
if [[ $1 == "1" ]]; then
	ssh echo5 "kh/apply_and_reboot.sh &" &
	ssh echo4 "kh/apply_and_reboot.sh &" &
elif [[ $1 == "2" ]]; then
	ssh echo5 "./apply_and_reboot.sh &" &
	ssh echo4 "./apply_and_reboot.sh &" & # Manual check first
fi
echo "doing make cscope"
make cscope
echo "ALL DONE"
