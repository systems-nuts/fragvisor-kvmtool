#! /bin/bash
#
# msg_het.sh
# Copyright (C) 2018 jackchuang <jackchuang@echo5>
#
# Distributed under terms of the MIT license.
#
scripts_location="/home/jackchuang/share/kernel_scripts"
kernel_location="/home/jackchuang/kh"

#LINUX_FOLDER=~/popcorn-kernel
LINUX_FOLDER=~/kh
TARGET_MODULE=msg_rdma.ko
#TARGET_MODULE=msg_socket.ko
TARGET_TEST_MODULE=msg_test.ko

ANOTHER_FOX_NODE=xxx
NODES="mir7 mir6 mir5" # 5 4 0 1

echo $TARGET_MODULE
echo $TARGET_MODULE
echo $TARGET_MODULE

for node in $NODES
do
	ssh $node "bash -c \"sudo mount -a\""
	ssh $node "sync"
	ssh $node "cp $scripts_location/config_pophype3node_mir_765.h $kernel_location/msg_layer/config.h"
	echo "sync $node"; echo;
done

# 1: testing module

#ssh $ANOTHER_FOX_NODE "sudo bash -c \"dmesg -n 7\"" # turn on the log on serial port
#make clean -C $LINUX_FOLDER/msg_layer/
#make -C $LINUX_FOLDER/msg_layer/
#sudo insmod $LINUX_FOLDER/msg_layer/$TARGET_MODULE &
#
#
#ssh $ANOTHER_FOX_NODE make clean -C $LINUX_FOLDER/msg_layer/
#ssh $ANOTHER_FOX_NODE make -C $LINUX_FOLDER/msg_layer/
#ssh $ANOTHER_FOX_NODE "bash -c \"sudo insmod $LINUX_FOLDER/msg_layer/$TARGET_MODULE\""

for node in $NODES
do
	ret=-1
	while [[ $ret != 0 ]];
	do

	echo " [$node]: msg cleaning..."
	ssh $node make clean -C $LINUX_FOLDER/msg_layer/ > /dev/null
	echo " [$node]: msg making..."
	ssh $node make -C $LINUX_FOLDER/msg_layer/ > /dev/null
	ret=$?
	echo "TODO copy to 2node 4node scripts"
	if [[ $ret != 0 ]]; then
		echo ""; echo ""; echo ""; echo "";
		echo "make msg_layer failed!!"
		echo "auto recovery start: recompile"
		ssh $node make clean -C $LINUX_FOLDER #> /dev/null
		ssh $node make -j32 -C $LINUX_FOLDER #> /dev/null
		echo "auto recovery done: reinsmod again"
		echo ""; echo ""; echo ""; echo "";
		#exit -1
		sleep 1
	fi
	echo;
	done
done
	
for node in $NODES
do
	echo "rmmod $TARGET_MODULE on ${node}..."
	#ssh $node cd ~/linux/msg_layer && git pull

	ssh $node sudo rmmod $LINUX_FOLDER/msg_layer/$TARGET_MODULE 2> /dev/null

	echo "insmod $TARGET_MODULE on ${node}..."
	ssh $node "sudo insmod $LINUX_FOLDER/msg_layer/$TARGET_MODULE" &
	#ssh $node "bash -c \"sudo insmod $LINUX_FOLDER/msg_layer/$TARGET_MODULE&\""
	#ssh $node sudo echo 0 > /proc/sys/kernel/hung_task_timeout_secs
	echo;echo;echo;
	sleep 2
done

if [[ $1 == "1" ]]; then
	echo "reinstalling $TARGET_TEST_MODULE on ${node}..."
	ssh $node "bash -c \"sudo rmmod $LINUX_FOLDER/msg_layer/$TARGET_TEST_MODULE\""
	ssh $nodeE "bash -c \"sudo insmod $LINUX_FOLDER/msg_layer/$TARGET_TEST_MODULE\""
	if [[ $? != 0 ]]; then
		echo "insmod msg_test.ko WRONG!!!"
		exit -1
	fi
	echo;
fi

#
for node in $NODES
do
	ssh $node "sudo bash -c \"dmesg -n 7\"" # turn on the log on serial port
	echo "dmesg - n 7 on $node";
done

#
for node in $NODES
do
	ssh $node "bash -c \"sudo mount -a\""
	ssh $node "sync"
	echo "sync $node"; echo;
done

#
for node in $NODES
do
	echo "[$node]: show info"
	#ssh $node "dmesg |grep -i popcorn"
	ssh $node "dmesg |grep popcorn"
	echo;echo;
done

cd c
exec bash
