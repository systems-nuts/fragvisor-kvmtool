#! /bin/sh
#
# test_pts.sh
# Copyright (C) 2019 jackchuang <jackchuang@echo5>
#
# Distributed under terms of the MIT license.
#

for i in `seq 0 1 3`
do
	echo $i > /dev/pts/$i
	ret=$?
	echo "$i - $ret"
done
