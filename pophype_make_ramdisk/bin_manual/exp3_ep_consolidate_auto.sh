#! /bin/bash
#
# exp3_ep_consolidate_auto.sh
# Copyright (C) 2019 jackchuang <jackchuang@echo>
#
# Distributed under terms of the MIT license.
#
iter=12
echo "= This is for ref, copy and change parameters and run it! ="
sleep 5
lscpu
echo -e "\n= make sure you have 4 vcpu running in the system!!!! giving you 10s to check =\n"
sleep 10

echo -e "\n= $0 start =\n"
rm out
for i in `seq 0 3`
do
	echo -e "\n\n\n\n\n\n\n\ntime exp3_ep_consolidate.sh $i\n\n\n\n\n\n\n\n"
	for j in `seq $iter`; do (time exp3_ep_consolidate.sh $i) |& tee -a out; echo | tee -a out; echo | tee -a out; sleep 3; done
	# How to tee time output:
	#$ ( time ./foo ) |& bar
	#$ ( time ./foo ) 2>&1 | bar
done

echo "all the output log are in \"out\", plz scp it to the host machine"
