#! /bin/bash
#
# trace.sh
# Copyright (C) 2020 jackchuang <jackchuang@echo>
#
# Distributed under terms of the MIT license.
#

TSTAMP=`date +%Y%m%d_%T | sed 's/:/_/g'`
machines="echo4 fox4" # test


servers()
{
    for i in $machines
    do
        echo "(command) ssh $i $1"
        ssh $i $1
    done
}

clear_log()
{
    servers "echo > /proc/popcorn_stat"
}


# usage:
#if [[ !$do_cpu_gov = "1" ]]; then
#    cpu_governor
#fi
cpu_governor(){
	echo "perform on - $machines"
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

clean_cache() {
    sudo bash -c "sudo echo 1 > /proc/sys/vm/drop_caches"
    sudo bash -c "sudo echo 2 > /proc/sys/vm/drop_caches"
    sudo bash -c "sudo echo 3 > /proc/sys/vm/drop_caches"
}

start_time() {
	# Time: start
	start_time=$( date +%s.%N )
}

end_time() {
	elapsed_time=$( date +%s.%N --date="$start_time seconds ago" )
	echo elapsed_time: $elapsed_time
}
