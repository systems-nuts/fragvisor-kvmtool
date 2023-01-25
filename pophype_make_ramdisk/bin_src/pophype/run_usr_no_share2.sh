#! /bin/bash
#
# run_false_share.sh
# Copyright (C) 2019 jackchuang <jackchuang@echo5>
#
# Distributed under terms of the MIT license.
#
. func.sh # load lib

pid=()
echo "$0 start"

#sleep 1 && taskset 0x02 ./dsm_generate 3&
#sleep 1 && taskset 0x01 ./dsm_generate 4&
taskset 0x02 ./dsm_generate 5& # ptr + 0
get_running_task_pid

taskset 0x01 ./dsm_generate 8 2& # ptr + 4097
get_running_task_pid

for nn in `seq 0 1 $i`
do
    echo "new: wait${nn}/$i pid ${pid[$nn]}"
    wait ${pid[$nn]}
done
echo "========"
echo "| Done |"
echo "========"

echo -e "$0 done\n"

