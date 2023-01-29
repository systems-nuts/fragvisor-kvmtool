#! /bin/bash
#
# jack_get_data.sh
# Copyright (C) 2018 jackchuang <jackchuang@echo>
#
# Distributed under terms of the MIT license.
#

APPS="BT-C SP-C CG-C lud euler3d_cpu streamcluster"

for app in $APPS
do
	echo "==============================================="
	echo "===== $app ======"
	echo "==============================================="
	echo "Execution Time Result:"
	folder=`ls | grep ${app}.*_[0-9]*_[0-9]*_[0-9]*_[0-9]*`
	cat $folder/`ls $folder | grep  [0-9]*_[0-9]*_1_1.log` |\
				egrep "Time in seconds|Compute time:|time = |real"
	echo
	echo "inv |fpiv |fp"
	cat ${app}_popcorn_stat|egrep "fp | inv |fpiv " |sed 's/.*cnt \([0-9]*\) .*/\1/g'
	echo
	echo "local inv, ww, total and remote inv, ww, total"
	cat ${app}_dmesg | grep exit | head -n 1 |sed 's/.*sys_inv_cnt \([0-9]*\) .*/\1/g'
	cat ${app}_dmesg | grep exit | head -n 1 |sed 's/.*remote side \([0-9]*\)).*/\1/g'
	cat ${app}_dmesg | grep exit | head -n 1 |sed 's/.*sys_rw_cnt \([0-9]*\) .*/\1/g'
	#echo "remote inv, ww, total"
	cat ${app}_dmesg | grep exit | tail -n 1 |sed 's/.*sys_inv_cnt \([0-9]*\) .*/\1/g'
	cat ${app}_dmesg | grep exit | tail -n 1 |sed 's/.*remote side \([0-9]*\)).*/\1/g'
	cat ${app}_dmesg | grep exit | tail -n 1 |sed 's/.*sys_rw_cnt \([0-9]*\) .*/\1/g'
done
