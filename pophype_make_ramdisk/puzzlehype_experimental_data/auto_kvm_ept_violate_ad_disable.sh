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
echo "\$MACHINES $MACHINES"
echo "= overwrite MACHINES info !!!!!!!!! ="
MACHINES="mir7 mir6" # nodes you wanna log trace
echo "\$MACHINES $MACHINES"
VM_IP="10.1.10.222"
echo "\$VM_IP $VM_IP"
echo "README:"
echo "- you can run this on any machine on the rack including gateway"
echo "- this script doesn't generate local files, so just run it"
echo "- save interested logs in popcorn_log"

servers "uname -a"

start_time=$( date +%s.%N )

# $0.sh name is important because folder name and log name are based on trace name.
echo "generate log name"
_OUTPUT=`echo "${0}" | sed 's/\.sh//g' | sed 's/\.\///g'`
echo "\$_OUTPUT = $_OUTPUT"
OUTPUT="${_OUTPUT}_trace"
echo "\$output = $OUTPUT"

CUR_DIR=`pwd`
FOLDER="${CUR_DIR}/${OUTPUT}_${TSTAMP}" # folder has time
#FOLDER="/home/jackchuang/c/pophype_make_ramdisk/puzzlehype_experimental_data/${OUTPUT}_${TSTAMP}" # folder has time
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
    servers "sudo bash -c \"echo 0 > /sys/kernel/debug/tracing/events/popcorn/kvm_ept_retry/enable\"" # off
    servers "sudo bash -c \"echo 0 > /sys/kernel/debug/tracing/events/kvm/kvm_page_fault_ext/enable\"" # off
    servers "sudo bash -c \"echo 0 > /sys/kernel/debug/tracing/events/kvm/kvm_ept_inv/enable\"" # off
    servers "sudo bash -c \"echo > /sys/kernel/debug/tracing/trace\"" #clean
    #servers "sudo bash -c \"echo 1000000 > /sys/kernel/debug/tracing/buffer_size_kb\""
    servers "sudo bash -c \"echo 500000 > /sys/kernel/debug/tracing/buffer_size_kb\"" # config
    echo " blocking running $1 [localhost]"; echo " blocking running $1 [localhost]";

	servers "echo > /proc/popcorn_hype"



    servers "sudo bash -c \"echo 1 > /sys/kernel/debug/tracing/events/popcorn/kvm_ept_retry/enable\"" # on
    servers "sudo bash -c \"echo 1 > /sys/kernel/debug/tracing/events/kvm/kvm_ept_inv/enable\"" # on
    servers "sudo bash -c \"echo 1 > /sys/kernel/debug/tracing/events/kvm/kvm_page_fault_ext/enable\"" # on
    $1 | tee -a popcorn_log ## watchout localhost
    servers "sudo bash -c \"echo 0 > /sys/kernel/debug/tracing/events/kvm/kvm_page_fault_ext/enable\"" # off
    servers "sudo bash -c \"echo 0 > /sys/kernel/debug/tracing/events/kvm/kvm_ept_inv/enable\"" # off
    servers "sudo bash -c \"echo 0 > /sys/kernel/debug/tracing/events/popcorn/kvm_ept_retry/enable\"" # on



	servers "cat /proc/popcorn_hype"
	echo -e "\n\nRecap popcorn_log:\n"
	servers "cat /proc/popcorn_hype | grep eptreinv" | tee -a popcorn_log

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
	ssh mir7 cp $OUTPUT ${OUTPUT}_mir7
	ssh mir7 cp ${OUTPUT}_mir7 $FOLDER
	# mir5
	ssh mir6 cp $OUTPUT ${OUTPUT}_mir6
	ssh mir6 cp ${OUTPUT}_mir6 $FOLDER
    #mv ${_OUTPUT}_trace $FOLDER
    # TODO: kill first 11 lines

	# move log
	mv popcorn_log $FOLDER

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
ssh-keyscan -t ECDSA -p 22 $VM_IP >> ~/.ssh/known_hosts2 # testing
#remote_run_tracelog "./evaluate-het.sh -b lud-run" "lud"
ssh root@$VM_IP "uname -a" # warmup ssh connection
COMMAND="ssh root@$VM_IP exp3_ep_consolidate.sh 1" # 2vcpu
$COMMAND # warmup
sleep 3
remote_run_tracelog "$COMMAND"
######ssh root@10.1.10.222 "exp2_consolidate_2t.sh" # warmup ssh connection
######remote_run_tracelog "ssh root@10.1.10.222 \"exp2_consolidate_2t.sh\""
########remote_run_tracelog "ssh root@10.1.10.222 exp2_consolidate_2t.sh" # 2vcpu
########remote_run_tracelog "ssh root@10.1.10.222 exp3_ep_consolidate.sh 1" # 2vcpu

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
elapsed_time=$( date +%s.%N --date="$start_time seconds ago" )
echo "$0 elapsed_time: $elapsed_time"
exit 0
echo "YOU SHOULD NOT SEE THIS. MY CHEATSHEET AS BELOW."
echo "YOU SHOULD NOT SEE THIS. MY CHEATSHEET AS BELOW."
echo "YOU SHOULD NOT SEE THIS. MY CHEATSHEET AS BELOW."
echo "YOU SHOULD NOT SEE THIS. MY CHEATSHEET AS BELOW."
echo "YOU SHOULD NOT SEE THIS. MY CHEATSHEET AS BELOW."
echo "YOU SHOULD NOT SEE THIS. MY CHEATSHEET AS BELOW."
echo "YOU SHOULD NOT SEE THIS. MY CHEATSHEET AS BELOW."
echo "YOU SHOULD NOT SEE THIS. MY CHEATSHEET AS BELOW."
echo "YOU SHOULD NOT SEE THIS. MY CHEATSHEET AS BELOW."
echo "YOU SHOULD NOT SEE THIS. MY CHEATSHEET AS BELOW."
echo "YOU SHOULD NOT SEE THIS. MY CHEATSHEET AS BELOW."
echo "YOU SHOULD NOT SEE THIS. MY CHEATSHEET AS BELOW."
echo "YOU SHOULD NOT SEE THIS. MY CHEATSHEET AS BELOW."
echo "YOU SHOULD NOT SEE THIS. MY CHEATSHEET AS BELOW."
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
