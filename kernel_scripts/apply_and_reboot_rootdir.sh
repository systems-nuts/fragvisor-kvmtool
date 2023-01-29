#! /bin/bash
#
# apply_and_reboot.sh
# Copyright (C) 2018 jackchuang <jackchuang@echo5>
#
# Distributed under terms of the MIT license.
#

sudo grub-set-default 1

#sleep 5
echo "Usage ./ (ipmi_reboot:1)"
bash -c "sudo mount -a"
sync
cd kh && ./apply_in_local.sh
ret=$?
#ssh echo4 "ssh echo "sleep 10  && ipmitool -I lanplus -H echo5-ipmi -U ssrg -P rtlabuser1%  power cycle"&"&
if [[ $1 == "1" ]]; then
    my_node=`uname -a |awk '{print$2}'`
	echo "I'm $my_node"
	sleep 5
	ssh 10.4.4.1 "ipmitool -I lanplus -H $my_node-ipmi -U ssrg -P rtlabuser1%  power cycle"&
	sleep 2
fi
if [[ $ret == 0 ]]; then
	sudo reboot
fi
