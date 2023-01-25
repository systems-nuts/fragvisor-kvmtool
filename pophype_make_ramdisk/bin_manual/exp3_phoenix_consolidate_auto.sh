#! /bin/bash
#
# exp3_ep_consolidate_auto.sh
# Copyright (C) 2019 jackchuang <jackchuang@echo>
#
# Distributed under terms of the MIT license.
#
#iter=7
iter=7
nrthreads=4
echo "This is for ref, copy and change parameters and run it!"
sleep 5
lscpu
echo -e "\n\nmake sure you have 4 vcpu!!!! wating for 10s\n\n"
sleep 10

for i in `seq 1 $nrthreads`
do
	echo -e "\n\n\n\n\n\n\n\ntime exp3_phoenix2_consolidate.sh $i\n\n\n\n\n\n\n\n"
	echo -e "\n\nphoenix2_dhype_auto_kmeans -t $i -c 10 -p 500000\n=====\nstart\n=====\niter=$iter\n"
	for j in `seq $iter`; do time phoenix2_dhype_auto_kmeans -t $i -c 10 -p 500000; sleep 3; done
	echo -e "\n\nphoenix2_dhype_auto_word_count /input/word_100MB.txt 10 $i\n=====\nstart\n=====\niter=$iter\n"
	for j in `seq $iter`; do time phoenix2_dhype_auto_word_count /input/word_100MB.txt 10 $i; sleep 3; done
done

