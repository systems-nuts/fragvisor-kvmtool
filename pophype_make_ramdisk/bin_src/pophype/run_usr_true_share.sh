#! /bin/bash
#
# run_false_share.sh
# Copyright (C) 2019 jackchuang <jackchuang@echo5>
#
# Distributed under terms of the MIT license.
#

pid=()
echo "$0 start"
#sleep 1 && taskset 0x02 ./dsm_generate 0&
#sleep 1 && taskset 0x01 ./dsm_generate 1&
# 5: server ptr+0 6: client ptr+1 7: usr no share 8:client ptr+0
taskset 0x02 ./dsm_generate 5& #server ptr+0
cur_pid=$!
PIDS+="$cur_pid "
echo "++ [$cur_pid] $PIDS"
pid[$i]=$cur_pid
let i=$i+1
taskset 0x01 ./dsm_generate 7& #client ptr+0
cur_pid=$!
PIDS+="$cur_pid "
echo "++ [$cur_pid] $PIDS"
pid[$i]=$cur_pid
let i=$i+1

for nn in `seq 0 1 $i`
do
    echo "new: wait${nn}/$i pid ${pid[$nn]}"
    wait ${pid[$nn]}
done
echo "========"
echo "| Done |"
echo "========"

echo -e "$0 done\n"
