#! /bin/bash
#
# auto_het_remote_contro.sh
# Copyright (C) 2018 jackchuang <jackchuang@echo4>
#
# Distributed under terms of the MIT license.
#

machines="echo5 fox5"

# $1: command
# $2: is nonblocking/background
servers() {
	for machine in $machines; do
		if [[ $2 = "1" ]]; then
			echo "(command) ssh $i $1 &"
			ssh $machine $1 &
		else	
			echo "(command) ssh $i $1"
			ssh $machine $1
		fi
	done
}


for i in `seq 1 1 10`
do

	# print iter
	servers "wall \"Iter: $i start\"" 1
	#ssh echo5 wall "Iter: $i start" &
	#ssh fox5 wall "Iter: $i start" &

	# run + wait
	ssh echo5 ./auto_het.sh &
	echo "running sleep 2 hrs"
	./external_sleep.sh

	#
	echo "running sleep 10 mins"
	ssh echo5 wall "Iter: $i done" &
	ssh fox5 wall "Iter: $i done" &

    ssh echo5 "sudo bash -c \"sudo reboot\"" &
    ssh fox5 "sudo bash -c \"sudo reboot\"" &
	sleep 20
	ipmitool -I lanplus -H fox5-ipmi -U ssrg -P rtlabuser1%  power cycle &
	ipmitool -I lanplus -H echo5-ipmi -U ssrg -P rtlabuser1%  power cycle &
	sleep 600
done
