#! /bin/bash
#
# func.sh
# Copyright (C) 2020 jackchuang <jackchuang@echo5>
#
# Distributed under terms of the MIT license.
#
PIDS=""
i=0
function get_running_task_pid()
{
    cur_pid=$!
    PIDS+="$cur_pid "
    echo "++ [$cur_pid] $PIDS"
    pid[$i]=$cur_pid
    let i=$i+1
}
