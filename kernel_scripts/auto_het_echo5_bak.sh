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

# $1: do cpu governor default(0)
echo "\$1: do cpu governor default(0)"
do_cpu_gov=$1
echo "\$2: copy binars for statis default(0)"
copy_apps_for_statis=$2
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
remote_run()
{
    servers "sudo bash -c \"dmesg -C\""
	servers "servers "wall \"app: $1 start\"""
    servers "echo > /proc/popcorn_stat" #clean popcorn_stat
    servers "echo > /proc/popcorn_stat" #clean popcorn_stat
    servers "echo > /proc/popcorn_stat" #clean popcorn_stat
	echo " blocking running $1 ..."; echo " blocking running $1 ...";

	./$1
	#ssh $host "cd $location; ./$1" # & at remote
	mkdir -p $FOLDER
	sleep 3


	#
	#
	#
	# TODO servers+log
	echo "collecting msg statis... (echo5)"
	uname -a >> ${2}_popcorn_stat
	uname -a >> ${2}_popcorn_stat
	uname -a >> ${2}_popcorn_stat
	uname -a >> ${2}_dmesg
	cat /proc/popcorn_stat >> ${2}_popcorn_stat
	sudo bash -c dmesg -c >> ${2}_dmesg

	echo "collecting msg statis... (fox5)"
	ssh fox5 "uname -a" >> ${2}_popcorn_stat
	ssh fox5 "uname -a" >> ${2}_popcorn_stat
	ssh fox5 "uname -a" >> ${2}_popcorn_stat
	ssh fox5 "uname -a" >> ${2}_dmesg
	ssh fox5 "cat /proc/popcorn_stat" >> ${2}_popcorn_stat
	ssh fox5 "sudo bash -c \"dmesg -c\"" >> ${2}_dmesg

	mv ${2}_popcorn_stat $FOLDER # 
	mv ${2}_dmesg $FOLDER #


	#
	#
	#
	#mv `ls |grep "\-[A-Z][a-z]*_[A-Z][a-z]*_[0-9]*_[0-9]*"` $FOLDER
	mv `ls |grep "_[0-9]*_[0-9]*_[0-9]*$"` $FOLDER
	if [[ $copy_apps_for_statis = "1" ]]; then
		echo "copying binaries for statis"
		cp ${2}_x86-64 $FOLDER
		cp ${2}_aarch64 $FOLDER
	fi

	#
	#
	echo "${2} done"
	echo "${2} done"
	echo "${2} done"
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
	# TODO: change this to a more meaningful way
    servers "sudo bash -c \"echo 1 > /sys/kernel/debug/tracing/events/popcorn/tso/enable\""
    servers "sudo bash -c \"echo 1 > /sys/kernel/debug/tracing/events/popcorn/pgfault/enable\""
#    #servers "sudo bash -c \"echo 1 > /sys/kernel/debug/tracing/events/popcorn/pgfault_stat/enable\"" # only for mm time
	echo " blocking running $1 ..."; echo " blocking running $1 ...";

	./$1
	#ssh $host "cd $location; ./$1" # & at remote
	mkdir -p $FOLDER
	sleep 3


	#
	#
	#
	# TODO servers+log
	echo "collecting msg statis... (echo5)"
	uname -a >> ${2}_popcorn_stat
	uname -a >> ${2}_popcorn_stat
	uname -a >> ${2}_popcorn_stat
	uname -a >> ${2}_dmesg
	cat /proc/popcorn_stat >> ${2}_popcorn_stat
	sudo bash -c dmesg -c >> ${2}_dmesg

	echo "collecting msg statis... (fox5)"
	ssh fox5 "uname -a" >> ${2}_popcorn_stat
	ssh fox5 "uname -a" >> ${2}_popcorn_stat
	ssh fox5 "uname -a" >> ${2}_popcorn_stat
	ssh fox5 "uname -a" >> ${2}_dmesg
	ssh fox5 "cat /proc/popcorn_stat" >> ${2}_popcorn_stat
	ssh fox5 "sudo bash -c \"dmesg -c\"" >> ${2}_dmesg

	mv ${2}_popcorn_stat $FOLDER # 
	mv ${2}_dmesg $FOLDER #


	#
	#
	#
	#mv `ls |grep "\-[A-Z][a-z]*_[A-Z][a-z]*_[0-9]*_[0-9]*"` $FOLDER
	mv `ls |grep "_[0-9]*_[0-9]*_[0-9]*$"` $FOLDER
	if [[ $copy_apps_for_statis = "1" ]]; then
		echo "copying binaries for statis"
		cp ${2}_x86-64 $FOLDER
		cp ${2}_aarch64 $FOLDER
	fi


	#
	#
	#
	echo "collecting ${2}_trace... (echo5)"
	sudo bash -c "cat /sys/kernel/debug/tracing/trace > ${2}_trace"
	sudo chown jackchuang:jackchuang ${2}_trace
	mv ${2}_trace $FOLDER
	# TODO: kill first 11 lines

	echo "collecting ${2}_trace_fox... (fox5)"
	ssh fox5 "sudo bash -c \"cat /sys/kernel/debug/tracing/trace > ${2}_trace_fox\""
	ssh fox5 "sudo chown jackchuang:jackchuang ${2}_trace_fox"
	ssh fox5 "mv ${2}_trace_fox $FOLDER"
	# TODO: kill first 11 lines


	#
	#
	echo "${2} done"
	echo "${2} done"
	echo "${2} done"
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
if [[ !$do_cpu_gov = "1" ]]; then
	cpu_governor
fi


remote_run "./evaluate-het.sh -b CG-C" "CG-C" $copy_apps_for_statis
remote_run "./evaluate-het.sh -b SP-C" "SP-C" $copy_apps_for_statis # pass # second # two begin
remote_run "./evaluate-het.sh -b BT-C" "BT-C" $copy_apps_for_statis # may crash # the most stable # no begin

remote_run "./evaluate-het.sh -b lud-run" "lud" $copy_apps_for_statis
remote_run "./evaluate-het.sh -b euler3d_cpu-run" "euler3d_cpu" $copy_apps_for_statis
remote_run "./evaluate-het.sh -b streamcluster-run" "streamcluster" $copy_apps_for_statis

#remote_run_tracelog "./evaluate-het.sh -b CG-C" "CG-C" $copy_apps_for_statis
#
#remote_run_tracelog "./evaluate-het.sh -b BT-C" "BT-C" $copy_apps_for_statis # may crash # the most stable # no begin
#remote_run_tracelog "./evaluate-het.sh -b SP-C" "SP-C" $copy_apps_for_statis # pass # second # two begin
#
#remote_run_tracelog "./evaluate-het.sh -b lud-run" "lud" $copy_apps_for_statis
#remote_run_tracelog "./evaluate-het.sh -b euler3d_cpu-run" "euler3d_cpu" $copy_apps_for_statis
#
#remote_run_tracelog "./evaluate-het.sh -b streamcluster-run" "streamcluster" $copy_apps_for_statis



#remote_run_tracelog "./evaluate-het.sh -b SP-C" "SP-C" $copy_apps_for_statis # pass # second # two begin
#remote_run_tracelog "./evaluate-het.sh -b CG-C" "CG-C" $copy_apps_for_statis
#remote_run_tracelog "./evaluate-het.sh -b BT-C" "BT-C" $copy_apps_for_statis # may crash # the most stable # no begin

#remote_run_tracelog "./evaluate-het.sh -b kmeans-run" "kmeans"
#remote_run_tracelog "./evaluate-het.sh -b lavaMD-run" "lavaMD"
#remote_run_tracelog "./evaluate-het.sh -b blackscholes-run" "blackscholes"

# OMP_SCHEDULE=STATIC POPCORN_PLACES={16},{96} ./SP-A_x86-64 -t 112

echo "Done!!!!"
echo "Done!!!!"
echo "Done!!!!"
