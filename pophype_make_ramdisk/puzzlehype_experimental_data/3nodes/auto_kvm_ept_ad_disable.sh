#! /bin/bash
#
# kvm_page_fault_ext.sh
# Copyright (C) 2020 jackchuang <jackchuang@echo5>
#
# Distributed under terms of the MIT license.
#

source ~/share/kernel_scripts/bash_lib/trace.sh
# TSTAMP
# MACHINES

#servers()
#{
#    for i in $MACHINES
#    do
#        echo "(command) ssh $i $1"
#        ssh $i $1
#    done
#}

#overwrite
echo "= original MACHINES info ="
echo "$MACHINES"
echo "= overwrite MACHINES info !!!!!!!!! ="
MACHINES="mir7 mir6 mir5" # nodes you wanna log trace
echo "$MACHINES"
echo "README:"
echo "- you can run this on any machine on the rack including gateway"
echo "- this script doesn't generate local files, so just run it"

servers "uname -a"

# $0.sh name is important because folder name and log name are based on trace name.
echo "generate log name"
_OUTPUT=`echo "${0}" | sed 's/\.sh//g' | sed 's/\.\///g'`
echo "\$_OUTPUT = $_OUTPUT"
OUTPUT="${_OUTPUT}_trace"
echo "\$output = $OUTPUT"

CUR_DIR=`pwd`
FOLDER="${CUR_DIR}/${OUTPUT}_${TSTAMP}" # folder has time
echo "\$FOLDER = $FOLDER"


#======================= func =========================
remote_run_tracelog() {
	# $1: ./app 
	# $2: trace_name

    servers "sudo bash -c \"dmesg -C\""
    #servers "servers "wall \"app: $1 start\"""
    #wall \"app: $1 start\"
    #servers "wall \"app: $1 start\""
    #servers "echo > /proc/popcorn_stat" #clean popcorn_stat
    #servers "echo > /proc/popcorn_stat" #clean popcorn_stat
    #servers "echo > /proc/popcorn_stat" #clean popcorn_stat
    servers "sudo bash -c \"echo 0 > /sys/kernel/debug/tracing/events/kvm/kvm_page_fault_ext/enable\"" # off
    servers "sudo bash -c \"echo > /sys/kernel/debug/tracing/trace\"" #clean
    #servers "sudo bash -c \"echo 1000000 > /sys/kernel/debug/tracing/buffer_size_kb\""
    servers "sudo bash -c \"echo 500000 > /sys/kernel/debug/tracing/buffer_size_kb\"" # config
    echo " blocking running $1 [localhost]"; echo " blocking running $1 [localhost]";

    servers "sudo bash -c \"echo 1 > /sys/kernel/debug/tracing/events/kvm/kvm_page_fault_ext/enable\"" # on
    $1 ## watchout localhost
    servers "sudo bash -c \"echo 0 > /sys/kernel/debug/tracing/events/kvm/kvm_page_fault_ext/enable\"" # off

    #ssh $host "cd $location; ./$1" # & at remote
    sleep 3
    servers "sudo bash -c \"cat /sys/kernel/debug/tracing/trace > ${_OUTPUT}_trace\""
	servers "sudo chown jackchuang:jackchuang ${_OUTPUT}_trace"
    servers "sudo bash -c \"echo > /sys/kernel/debug/tracing/trace\"" #clean

    mkdir -p $FOLDER
	echo "TODO - now manually (make it auto)"
	HOSTNAME=`ssh mir7 "uname -n"`
	echo "$HOSTNAME"
	HOSTNAME=`ssh mir6 "uname -n"`
	echo "$HOSTNAME"
	HOSTNAME=`ssh mir5 "uname -n"`
	echo "$HOSTNAME"
	ssh mir7 cp $OUTPUT ${OUTPUT}_mir7
	ssh mir7 cp ${OUTPUT}_mir7 $FOLDER
	# mir6
	ssh mir6 cp $OUTPUT ${OUTPUT}_mir6
	ssh mir6 cp ${OUTPUT}_mir6 $FOLDER
	# mir5
	ssh mir5 cp $OUTPUT ${OUTPUT}_mir5
	ssh mir5 cp ${OUTPUT}_mir5 $FOLDER
    #mv ${_OUTPUT}_trace $FOLDER
    # TODO: kill first 11 lines

	# ==========================================================
    # TODO servers+log
	# echo5
    #echo "collecting msg statis... (echo5)"
    #ssh echo5 "uname -a" >> ${_OUTPUT}_popcorn_stat
    #ssh echo5 "uname -a" >> ${_OUTPUT}_popcorn_stat
    #ssh echo5 "uname -a" >> ${_OUTPUT}_popcorn_stat
    #ssh echo5 "uname -a" >> ${_OUTPUT}_dmesg
    #ssh echo5 "cat /proc/popcorn_stat" >> ${_OUTPUT}_popcorn_stat
    #ssh echo5 "sudo bash -c \"dmesg -c\"" >> ${_OUTPUT}_dmesg

	echo "manullay program here !!!!!!!!!!!!!! (TODO AUTO)"
    # echo4
    #echo "collecting msg statis... (echo4)"
    #ssh echo4 "uname -a" >> ${_OUTPUT}_popcorn_stat
    #ssh echo4 "uname -a" >> ${_OUTPUT}_popcorn_stat
    #ssh echo4 "uname -a" >> ${_OUTPUT}_popcorn_stat
    #ssh echo4 "uname -a" >> ${_OUTPUT}_dmesg
    #ssh echo4 "cat /proc/popcorn_stat" >> ${_OUTPUT}_popcorn_stat
    #ssh echo4 "sudo bash -c \"dmesg -c\"" >> ${_OUTPUT}_dmesg
    
	
	#mv ${_OUTPUT}_popcorn_stat $FOLDER #
    #mv ${_OUTPUT}_dmesg $FOLDER #
	#
    #mv `ls |grep "\-[A-Z][a-z]*-[A-Z][a-z]*-[0-9]*-[0-9]*"` $FOLDER
	# ==========================================================
}

# auto register ssh connection
ssh-keyscan -t ECDSA -p 22 10.1.10.222 >> ~/.ssh/known_hosts2 # testing
#remote_run_tracelog "./evaluate-het.sh -b lud-run" "lud"
ssh root@10.1.10.222 "uname -a" # warmup ssh connection
#ssh root@10.1.10.222 "exp2_consolidate_2t.sh" # warmup ssh connection
#remote_run_tracelog "ssh root@10.1.10.222 \"exp2_consolidate_2t.sh\""
#remote_run_tracelog "ssh root@10.1.10.222 exp2_consolidate_2t.sh"
remote_run_tracelog "ssh root@10.1.10.222 exp3_ep_consolidate.sh 2" # 3vcpu


# parse scripts
cp parse_scripts/* $FOLDER

# lkvm
cp ~/c/lkvm $FOLDER
#ssh root@10.1.10.222 "cat /proc/kallsyms" > guest.kallsyms
#ssh root@10.1.10.222 "cat /proc/modules" > guest.modules
#mv guest.modules guest.kallsyms $FOLDER # mv into the folder
ssh mir7 nm ~/kh/vmlinux > nm_vmlinux
##
echo "- nm auto check for page false sharing-"
cat nm_vmlinux | egrep "hv_clock$|last_value$"
##
mv nm_vmlinux $FOLDER # mv into the folder
ssh mir7 objdump -dSlr ~/kh/vmlinux > ~/kh/vmlinux.asm # must do this on mir7 to get c code symbols
ssh mir7 cp ~/kh/vmlinux.asm $FOLDER
ssh mir7 cp ~/kh/vmlinux $FOLDER # in case
ssh mir7 sync
#scp mir7:~/kh/vmlinux.asm $FOLDER

echo "$0 Complete. Output = $FOLDER"
exit 0

echo "YOU SHOULD NOT SEE THIS. MY CHEATSHEET AS BELOW."

#servers "sudo bash -c \"echo > /sys/kernel/debug/tracing/trace\"" #clean
#servers "sudo bash -c \"echo 500000 > /sys/kernel/debug/tracing/buffer_size_kb\"" # 10G
##servers "sudo bash -c \"echo 1000000 > /sys/kernel/debug/tracing/buffer_size_kb\"" # 18G
#servers "sudo bash -c \"echo 1 > /sys/kernel/debug/tracing/events/popcorn/tso/enable\""
#servers "sudo bash -c \"echo 1 > /sys/kernel/debug/tracing/events/popcorn/pgfault/enable\""


# init
mount -t debugfs none /sys/kernel/debug
sudo bash -c "echo > /sys/kernel/debug/tracing/trace"
sudo bash -c "echo 500000 > /sys/kernel/debug/tracing/buffer_size_kb" # 10G
#sudo bash -c "echo 1000000 > /sys/kernel/debug/tracing/buffer_size_kb" # 18G
sudo bash -c "echo 0 > /sys/kernel/debug/tracing/events/kvm/kvm_page_fault_ext/enable"
sudo bash -c "echo > /sys/kernel/debug/tracing/trace" # clear
# ===


## PuzzleHype
#sudo bash -c "echo 1 > /sys/kernel/debug/tracing/events/popcorn/vmdsm_traffic/enable" # on
#echo > /proc/popcorn_debug # on
## start profiling
#ssh 10.4.4.222 ls #echo
#ssh 10.2.10.222 ls #mir
####end profiling
#sudo bash -c "echo 0 > /sys/kernel/debug/tracing/events/popcorn/vmdsm_traffic/enable" # off
#echo > /proc/popcorn_debug # off
## ===


# AB patch
sudo bash -c "echo 1 > /sys/kernel/debug/tracing/events/kvm/kvm_page_fault_ext/enable" # on
sudo bash -c "echo 1 > /sys/kernel/debug/tracing/events/kvm/kvm_page_fault_ext/enable" # on
# start profiling
#ssh 10.4.4.222 ls #echo
#ssh 10.2.10.222 ls #mir
ssh 10.2.10.222 ls
ssh 10.2.10.222 "./ep"
###end profiling
sudo bash -c "echo 0 > /sys/kernel/debug/tracing/events/kvm/kvm_page_fault_ext/enable" # off
# ===


# save
sudo bash -c "cat /sys/kernel/debug/tracing/trace > ${OUTPUT}"
sudo chown jackchuang:jackchuang ${OUTPUT}
# ===


# collect from all nodes ssh MACHINES
for i in $MACHINES
do
	echo "machine $i"
	#echo "(command) ssh $i $1"
	#ssh $i $1
done

# useful commands (do it outside the script):
# watch -n 1 "sudo bash -c \"wc -l /sys/kernel/debug/tracing/trace\""
