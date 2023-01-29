#! /bin/bash
#
# auto_het.sh
# Copyright (C) 2018 jackchuang <jackchuang@echo5>
#
# Distributed under terms of the MIT license.
#


#TSTAMP=`date |sed 's/ /_/g'`
TSTAMP=`date +%Y%m%d_%T | sed 's/:/_/g'`
FOLDER="share/test_$TSTAMP"

# $1: command
machines="echo5 fox5"

servers()
{
    for i in $machines
    do
        echo "(command) ssh $i $1"
        ssh $i $1
    done
}

cpu_governor(){

    for i in $machines
    do
        echo "(command) ssh $i echo cpu_governor"
        RCPU=`ssh $i "lscpu | grep ^CPU\(s\): | sed 's/^CPU.* //g'"` # | awk '{print $2}'"` doesn't work..........why?
        let "RCPU=$RCPU-1"
        echo "machine $i: 0 ~ $RCPU  cpus"
        for z in `seq 0 $RCPU`; do
            ssh $i "sudo bash -c \"echo performance > /sys/devices/system/cpu/cpu$z/cpufreq/scaling_governor\""
        done
        echo "check:"
        ssh $i "sudo bash -c \"cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor\""
    done
}

msg_drivers()
{
	./msg_het.sh
}

# popcorn_stat
remote_run_tracelog()
{
    servers "sudo bash -c \"dmesg -C\""
	servers "servers "wall \"app: $1 start\"""
    servers "echo > /proc/popcorn_stat" #clean popcorn_stat
    servers "echo > /proc/popcorn_stat" #clean popcorn_stat
    servers "echo > /proc/popcorn_stat" #clean popcorn_stat
    servers "sudo bash -c \"echo > /sys/kernel/debug/tracing/trace\"" #clean
    servers "sudo bash -c \"echo 1000000 > /sys/kernel/debug/tracing/buffer_size_kb\""
    servers "sudo bash -c \"echo 1 > /sys/kernel/debug/tracing/events/popcorn/pgfault_stat/enable\""
	echo " blocking running $1 ..."; echo " blocking running $1 ...";

	./$1
	#ssh $host "cd $location; ./$1" # & at remote
	sleep 3
	sudo bash -c "cat /sys/kernel/debug/tracing/trace > ${2}_trace"
	sudo chown jackchuang:jackchuang ${2}_trace
	mkdir -p $FOLDER
	mv ${2}_trace $FOLDER
	# TODO: kill first 11 lines

	# TODO servers+log
	# echo5
	echo "collecting msg statis... (echo5)"
	uname -a >> ${2}_popcorn_stat
	uname -a >> ${2}_popcorn_stat
	uname -a >> ${2}_popcorn_stat
	uname -a >> ${2}_dmesg
	cat /proc/popcorn_stat >> ${2}_popcorn_stat
	sudo bash -c dmesg -c >> ${2}_dmesg
	# fox5
	echo "collecting msg statis... (fox5)"
	ssh fox5 "uname -a" >> ${2}_popcorn_stat
	ssh fox5 "uname -a" >> ${2}_popcorn_stat
	ssh fox5 "uname -a" >> ${2}_popcorn_stat
	ssh fox5 "uname -a" >> ${2}_dmesg
	ssh fox5 "cat /proc/popcorn_stat" >> ${2}_popcorn_stat
	ssh fox5 "sudo bash -c \"dmesg -c\"" >> ${2}_dmesg

	mv ${2}_popcorn_stat $FOLDER # 
	mv ${2}_dmesg $FOLDER # 

	mv `ls |grep "\-[A-Z][a-z]*-[A-Z][a-z]*-[0-9]*-[0-9]*"` $FOLDER
}

check_sh()
{
	#if msg_het.sh
		echo $TSTAMP
		echo $FOLDER
}

#main
check_sh
msg_drivers
cpu_governor

remote_run_tracelog "./evaluate-het.sh -b lud-run" "lud"
#remote_run_tracelog "./evaluate-het.sh -b euler3d_cpu-run" "euler3d_cpu"
#remote_run_tracelog "./evaluate-het.sh -b streamcluster-run" "streamcluster"

remote_run_tracelog "./evaluate-het.sh -b CG-C" "CG-C"
remote_run_tracelog "./evaluate-het.sh -b SP-C" "SP-C"
remote_run_tracelog "./evaluate-het.sh -b BT-C" "BT-C" # may crash

#remote_run_tracelog "./evaluate-het.sh -b kmeans-run"
#remote_run_tracelog "./evaluate-het.sh -b lavaMD-run"
#remote_run_tracelog "./evaluate-het.sh -b blackscholes-run"

