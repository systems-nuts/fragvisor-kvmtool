#! /bin/bash
#
# run_false_share.sh
# Copyright (C) 2019 jackchuang <jackchuang@echo5>
#
# Distributed under terms of the MIT license.
#

# lazy jack
#for kk in `seq 10`; do echo -e "\n\n=== $kk/10 ===\n\n"; time ./run_usr_all_test_in_guestvm.sh; sleep 10; done
TSTAMP=`date +%Y%m%d_%T | sed 's/:/_/g'`
TEST="
./run_usr_no_share2.sh
./run_usr_false_share2.sh
./run_usr_true_share2.sh

./run_usr_no_share3.sh
./run_usr_false_share3.sh
./run_usr_true_share3.sh

./run_usr_no_share4.sh
./run_usr_false_share4.sh
./run_usr_true_share4.sh
"
echo -e "\nThese are base/example script. They are the same as ****2.sh
./run_usr_no_share.sh\n
./run_usr_false_share.sh\n
./run_usr_true_share.sh\n
"

ITER=1

OUTPUT=${TSTAMP}_output
PERF_DATA=${TSTAMP}_perf_data

rm output
rm perf_data
for test in $TEST
do
	for i in `seq $ITER`
	do
		(time $test) |& tee tmp
		echo "[$i/$ITER] $test" | tee -a $OUTPUT | tee -a $PERF_DATA
		cat tmp | grep "exec time" | tee -a $OUTPUT
		cat tmp | grep "micro_perf" | tee -a $OUTPUT | tee -a $PERF_DATA
		cat tmp | grep real | tee -a $OUTPUT
		echo "sleep 2"
		sleep 2

		#(time ./run_usr_no_share.sh) |& tee tmp
		#cat tmp | grep real | tee -a output
		#sleep 5
		#(time ./run_usr_false_share.sh) 2>&1| tee tmp
		#cat tmp | grep real | tee -a output
		#sleep 5
		#(time ./run_usr_true_share.sh) |& tee tmp
		#cat tmp | grep real | tee -a output
		#sleep 5
	done
	echo -e "\n\n" | tee -a $OUTPUT | tee -a $PERF_DATA
done

#scp $PERF_DATA $OUTPUT 10.4.4.15:/home/jackchuang/share/popcorn-utils/pophype_micro_usr_dsmtraffic

echo -e "\n\n$0 done\n\n"


