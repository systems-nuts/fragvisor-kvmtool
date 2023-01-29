#! /bin/bash
#
# data_icdcs19_lat_parse.sh
# Copyright (C) 2019 jackchuang <jackchuang@echo>
#
# Distributed under terms of the MIT license.
#

TARGET_FILES="node_echo5_0_24
				node_echo5_0_4096
				node_echo5_0_16384
				node_echo5_0_32768
				node_echo5_0_65536
				node_echo5_0_131000
				node_echo5_1_24
				node_echo5_4_4096
				node_fox5_0_24
				node_fox5_0_4096
				node_fox5_0_16384
				node_fox5_0_32768
				node_fox5_0_65536
				node_fox5_0_131000
				node_fox5_1_24
				node_fox5_4_4096"
for i in $TARGET_FILES
do
	echo "======================="
	echo "=== $i ==="
	echo "======================="
	echo "ttt"
	cat $i | grep -v total |egrep "sync|RR"| awk '{print$7}'
	echo "lat"
	cat $i | grep -v total |grep lat| awk '{print$4}'
	echo "tps"
	cat $i | grep -v total |grep tps| awk '{print$4}'
done

#$ cat result_jack_testing_20190118_03_56_14_4_1/node_echo5_4_4096 |grep -v total |grep tps|awk '{print$4}'
#159
#274
#300
#313
#316
#$ cat result_jack_testing_20190118_03_56_14_4_1/node_echo5_4_4096 |grep -v total |grep lat|awk '{print$4}'
#25.485667
#29.405865
#54.746300
#104.195304
#207.110004
#$ cat result_jack_testing_20190118_03_56_14_4_1/node_echo5_4_4096 |grep -v total |egrep "sync|RR"|awk '{print$7}'
#1
#2
#4
#8
#16
