#! /bin/bash
#
# jack_build_all_classA.sh
# Copyright (C) 2020 jackchuang <jackchuang@echo>
#
# Distributed under terms of the MIT license.
#

echo "TODO - make sure"
echo "config/make.def"
echo "CFLAGS  = -g -Wall -O3 -mcmodel=medium -fPIC"
echo -e "\n"
echo "In generate_ramdisk.sh:" 
echo "sudo cp -varf ./SNU_NPB-1.0.3-pophype/NPB3.3-SER-C/bin/*.ser.van \$ROOTFS_DIR"

APP="bt cg ep ft is lu lu-hp mg sp ua"
CLASS="A B C D"
for class in  $CLASS
do
	for app in $APP
	do
		make $app CLASS=$class
		cp bin/${app}.${class}.x bin/${app}.${class}.x.ser.van
	done
done
echo "Class E requires you to manually try each one since some may fail (code it in this script)"
echo "TODO"


