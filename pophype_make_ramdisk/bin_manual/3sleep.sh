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

pid=()
i=0
# date is not precise
#PIDS=""
#./ep-ao0-cpu0 > /dev/null &
sleep 95&
cur_pid=$!
taskset -p 0x1 $cur_pid
#PIDS="$PIDS $cur_pid"
PIDS+="$cur_pid "
echo "++ [$cur_pid] $PIDS"
pid[$i]=$cur_pid
let i=$i+1
echo ""

#./ep-ao0-cpu1 > /dev/null &
sleep 95&
cur_pid=$!
taskset -p 0x2 $cur_pid
#PIDS="$PIDS $cur_pid"
PIDS+="$cur_pid "
echo "++ [$cur_pid] $PIDS"
pid[$i]=$cur_pid
let i=$i+1
echo ""

#./ep-ao0-cpu1 > /dev/null &
#./ep-ao0-cpu2 > /dev/null &
sleep 95&
cur_pid=$!
taskset -p 0x4 $cur_pid
#PIDS="$PIDS $cur_pid"
PIDS+="$cur_pid "
echo "++ [$cur_pid] $PIDS"
pid[$i]=$cur_pid
#let i=$i+1
echo ""

# Try ${!pid[@]} ${#pid[@]}
#echo -ne "new: "
#for nn in `seq 0 1 $i`
#do
#	echo -ne "${pid[nn]} "
#done
#echo

for nn in `seq 0 1 $i`
do
	echo "new: wait${nn}/$i pid ${pid[$nn]}"
	wait ${pid[$nn]}
done
echo "========"
echo "| Done |"
echo "========"

#for _pid in "${PIDS[@]}"
#do
#	echo "wait PID [$_pid]"
#	wait $_pid
#done
#echo "========"
#echo "| Done |"
#echo "========"

# do this for real time first
#time "./ep-ao0-cpu0"
#time "./ep-ao0-cpu0 > /dev/null"
#time `./ep-ao0-cpu2 > /dev/null`
