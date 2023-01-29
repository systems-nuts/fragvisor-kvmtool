#! /bin/bash
#
# timeer.sh
# Copyright (C) 2018 jackchuang <jackchuang@fox5>
#
# Distributed under terms of the MIT license.
#

start=`date +%s`
while [ 1 ]; do

	sleep 10
	end=`date +%s`
	runtime=$((end-start))
	echo $runtime
done
