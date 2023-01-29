#! /bin/bash
#
# auto_apply_git.sh
# Copyright (C) 2018 jackchuang <jackchuang@echo5>
#
# Distributed under terms of the MIT license.
#
echo "Script: applying git patch \"$1\" and modify corresponding things" 
echo "Usage: TODO"

echo "$0: pwd = `pwd`"
scripts_location="/home/jackchuang/share/kernel_scripts"
# auto_apply_git.sh  build.sh  config.h xxx.patch
#check file
#if [[ $1 -f ]]

# in case
#$CALLER=.
#rm ${CALLER}/kernel/popcorn/sync.c #TODO REMOVE HARDCODED
#rm ${CALLER}/kernel/popcorn/sync.h #TODO REMOVE HARDCODED
#rm ${CALLER}/include/popcorn/sync.h #TODO REMOVE HARDCODED

rm kernel/popcorn/sync.c #TODO REMOVE HARDCODED
rm kernel/popcorn/sync.h #TODO REMOVE HARDCODED
rm include/popcorn/sync.h #TODO REMOVE HARDCODED

echo "rebase" 
git reset --hard HEAD ######## TODO becareful...........!!!!!!!
echo "apply git patch \"$1\" (stat)" 
echo; echo; echo;
echo "====================================" 
echo -e "\t\tgit apply --stat $1"
git apply --stat $1
echo "====================================" 
echo; echo; echo;
echo "apply git patch \"$1\" (dry run)" 
echo -e "\t\tgit apply --check $1"
git apply --check $1
echo; echo; echo;
echo "apply git patch \"$1\" (real)" 
echo -e "\t\tgit apply $1"
git apply $1
ret=$?
echo; echo; echo;
echo "apply ret = $ret"

#echo "maually copy sync.c (somehow not in patch)"
#cp $scripts_location/sync.c kernel/popcorn


node_num=`uname -a |awk '{print$2}' | sed s/[a-z]//g`
#if [[ $node_num == 5 ]]; then
#	echo "replace msg_layer/config.h w/ msg_layer/config.h"
#	cp $scripts_location/config.h  msg_layer/config.h
#elif [[ $node_num == 4 ]]; then
#	echo "replace msg_layer/config.h w/  msg_layer/config4.h"
#	cp $scripts_location/config4.h  msg_layer/config.h
#else
#	echo "PLZ MANUALLY FIX msg_layer/config.h !!!"
#	echo "PLZ MANUALLY FIX msg_layer/config.h !!!"
#	echo "PLZ MANUALLY FIX msg_layer/config.h !!!"
#fi
#cp $scripts_location/config_echo54.h msg_layer/config.h
#cp $scripts_location/config_pophype2node_54.h msg_layer/config.h
cp $scripts_location/config_pophype3node_540.h msg_layer/config.h
#cp $scripts_location/config_pophype4node_5401.h msg_layer/config.h

#make oldconfig

#echo "TODO: double check for make menuconfig"

if [[ $ret = 0 ]]; then
	./build.sh #&& sudo reboot
	ret=$?
fi
#./build.sh
sync
exit $ret
# 1924  make menuconfig
# 1925  make -j33
# 1926  make modules -j44
# 1927  sudo make modules_install && sudo make
# 1928  sudo make modules_install && sudo make install
