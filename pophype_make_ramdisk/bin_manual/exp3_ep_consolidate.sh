#!/bin/bash
#
# exp1.sh
# Copyright (C) 2019 jackchuang <jackchuang@echo>
#
# Distributed under terms of the MIT license.
#

#example
#my_function &
#PID="$!"
#PIDS+=($PID)

#let i=$i+1
echo "USAGE: $0 <LAST_CPU>"
echo "Put your bin (/ep-ao0-1t-argv) under / TODO auto check"
echo "TODO - check argv/argc"
echo "TODO - no file command in ramdisk yet"
#file /ep-ao0-1t-argv
echo "e.g."
echo "$0 0: 1vcpu"
echo "$0 1: 2vcpu"
echo "$0 2: 3vcpu"
echo "$0 3: 4vcpu"
echo "It will do 0 1 2 3 (4 ep-a processes on 4 vcpus)"

if [ -z "$1" ]
then
	echo "No argument supplied"
	exit -1
fi

LAST_CPU=$1
pid=()
#LAST_CPU=3 # 3 = (0 1 2 3) 4 iter
i=0

## date is not precise
##PIDS=""
#/ep-ao0-1t-argv 0 > /dev/null &
#cur_pid=$!
##PIDS="$PIDS $cur_pid"
#PIDS+="$cur_pid "
#echo "++ [$cur_pid] $PIDS"
#pid[$i]=$cur_pid
#let i=$i+1
#
#/ep-ao0-1t-argv 1 > /dev/null &
#cur_pid=$!
##PIDS="$PIDS $cur_pid"
#PIDS+="$cur_pid "
#echo "++ [$cur_pid] $PIDS"
#pid[$i]=$cur_pid
#let i=$i+1
#
#/ep-ao0-1t-argv 2 > /dev/null &
#cur_pid=$!
##PIDS="$PIDS $cur_pid"
#PIDS+="$cur_pid "
#echo "++ [$cur_pid] $PIDS"
#pid[$i]=$cur_pid
##let i=$i+1


# seq 0 1 3 = 0 1 2 3 (4 iter)
for j in `seq 0 1 $LAST_CPU`
do 
	/ep-ao0-1t-argv $j > /dev/null &
	cur_pid=$!
	#PIDS="$PIDS $cur_pid"
	PIDS+="$cur_pid "
	echo "++ [$cur_pid] $PIDS"
	pid[$i]=$cur_pid
	#let i=$i+1
done


# Try ${!pid[@]} ${#pid[@]}
#echo -ne "new: "
#for nn in `seq 0 1 $i`
#do
#	echo -ne "${pid[nn]} "
#done
#echo

for nn in `seq 0 1 $LAST_CPU`
do
	echo "new: wait${nn}/$LAST_CPU pid ${pid[$nn]}"
	wait ${pid[$nn]}
done
echo "========"
echo "| Done |"
echo "========"
echo -e "\n"
echo "= recap ="
echo "$0 $1"
#echo -e "\n\n"
#for _pid in "${PIDS[@]}"
#do
#	echo "wait PID [$_pid]"
#	wait $_pid
#done
#echo "========"
#echo "| Done |"
#echo "========"

# do this for real time first
#time "/ep-ao0-cpu0"
#time "/ep-ao0-cpu0 > /dev/null"
#time `/ep-ao0-cpu2 > /dev/null`
