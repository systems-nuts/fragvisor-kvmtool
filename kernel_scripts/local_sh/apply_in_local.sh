#! /bin/bash
#
# apply_in_local.sh
# Copyright (C) 2018 jackchuang <jackchuang@echo5>
#
# Distributed under terms of the MIT license.
#

scripts_location="/home/jackchuang/share/kernel_scripts"

FOLDER=$scripts_location
#if [ -f $FILE ]; then
if [ -e $FOLDER ]; then
    echo "File $FOLDER exists."
else
    echo "File $FOLDER does not exist."
    exit -1
fi

if [[ $1 == "1" ]]; then
	echo "cp $scripts_location/msg_test.c ~/k/msg_layer"
	cp $scripts_location/msg_test.c ~/k/msg_layer
	cp $scripts_location/rdma.c ~/k/msg_layer
	cp $scripts_location/common.h ~/k/msg_layer
	make -C msg_layer
else
	#rm ~kernel/popcorn/sync.c #TODO REMOVE HARDCODED
	#rm ~kernel/popcorn/sync.h #TODO REMOVE HARDCODED
	#rm ~include/popcorn/sync.h #TODO REMOVE HARDCODED

    rm include/popcorn/hype.h
    rm kernel/popcorn/hype_file.c
    rm include/popcorn/hype_file.h
    rm kernel/popcorn/hype_kvm.c
    rm include/popcorn/hype_kvm.h
	#rm virt/kvm/hype_kvm.c

	#cd $scripts_location &&\
	#echo "applying xxx.patch... (larger_msg_pool.patch included)" && \
	$scripts_location/auto_apply_git.sh $scripts_location/xxx.patch
	ret=$?

# 2 for master to use large_msg_pool.patch
if [[ $1 == "2" ]]; then
	ret2=-1
	patch="$scripts_location/larger_msg_pool.patch"
	if [[ $ret = 0 ]]; then
		echo "apply git patch \"$patch\" (stat)"
		echo; echo; echo;
		echo "===================================="
		git apply --stat $patch
		echo "===================================="
		echo; echo; echo;
		echo "apply git patch \"$patch\" (dry run)"
		git apply --check $patch
		echo; echo; echo;
		echo "apply git patch \"$patch\" (real)"
		git apply $patch
		ret2=$?
		echo; echo; echo; sleep 5
		echo "apply ret = $ret"
	fi
	if [[ $ret = 0 ]]; then
		if [[ $ret2 = 0 ]]; then
	    ./build.sh #&& sudo reboot
		fi
	fi
fi
	#cd -
	exit $ret
fi
