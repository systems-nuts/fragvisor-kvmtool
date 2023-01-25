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
#sleep 1 && taskset 0x02 ./dsm_generate 0&
#sleep 1 && taskset 0x01 ./dsm_generate 1&

#server
taskset 0x08 ./dsm_generate 5& #server ptr+0
get_running_task_pid

taskset 0x04 ./dsm_generate 32& #server ptr+2
get_running_task_pid

taskset 0x02 ./dsm_generate 33& #server ptr+3
get_running_task_pid

#last client
taskset 0x01 ./dsm_generate 6 4& #client ptr+1
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
