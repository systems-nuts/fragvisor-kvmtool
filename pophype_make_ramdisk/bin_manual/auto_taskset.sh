#! /bin/bash
#
# auto_setup.sh
# Copyright (C) 2019 jackchuang <jackchuang@echo>
#
# Distributed under terms of the MIT license.
#
#
#
echo -e "\n\n\n======\n$0 start\n=======\n\n\n\n"
echo "cpu affinity show and reconfig"
echo
echo
echo "= long ="
echo -e "- auto pin task on vcpu0 -"
ps aux |awk {'print $2'} | xargs -I {} taskset -p {}
echo -e "\n\n\n\n\n"
ps aux |awk {'print $2'} | xargs -I {} taskset -p 0x1 {}
echo -e "\n\n\n\n\n"
ps aux |awk {'print $2'} | xargs -I {} taskset -p {}
echo -e "\n\n\n\n\n"
echo
echo
echo
echo -e "$0 Done!!\n\n\n\n"

