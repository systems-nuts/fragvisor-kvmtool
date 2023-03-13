#! /bin/bash
#
# run.sh
# Copyright (C) 2018 jackchuang <jackchuang@Jack-desktop>
#
# Distributed under terms of the MIT license.
#

# $1 (1/0): default: 1 - compile lkvm
# $2 (1/0): default: 1 - compile guest kernel
# $3 (1/0): default: 1 - cscope -Rbqk
#

KERNEL_PATH="/home/ssrg1/fragvisor-linux"

USER=/home/ssrg1/
MACHINES="echo5 echo4 echo0 echo1"
THISNODE=`uname -n`
SUPERNODE="echo0"
DO_CSCOPE=$3
echo "Usage: ./$0 <compile lkvm> <compile guest kernel> <cscope lkvm>"
echo -e "\n\nUSER KERNEL = $KERNEL_PATH on echo5\n\n\n"
##########################
# compile guest kernel
#########################
function compile_guest_kernel {
	echo "Compiling guest kernel..."
	echo "Compiling guest kernel..."
	echo "Compiling guest kernel..."
	echo;echo;echo

	# compile it on 32core machine is faster
	ssh $THISNODE taskset -c 0-15 make -j16 -C $KERNEL_PATH/
	ssh $THISNODE cp $KERNEL_PATH/vmlinux $KERNEL_PATH/vmlinux_guest

	#echo "====================================="
	#echo "Use super powerful node to compile !!"
	#echo "====================================="
	#ssh $SUPERNODE make -j32 -C $KERNEL_PATH/

    ret=$?
    if [[ $ret != 0 ]]; then
        echo "DIE: $ret"
        exit -1
    else
        echo;echo;echo
        echo "GUEST KERNEL: $ret GOOD GOOD GOOD !!!"
        echo "GUEST KERNEL: $ret GOOD GOOD GOOD !!!"
        echo "GUEST KERNEL: $ret GOOD GOOD GOOD !!!"
        echo;echo;echo
    fi
}


#######
# main
#######

# FULLY compile lkvm
if [[ $1 != "0" ]]; then
	make clean
	taskset -c 0-15 make -j16
	if [[ $DO_CSCOPE != "0" ]]; then
		cscope -Rbqk&
	fi
fi

# 
for machine in $MACHINES
do
	echo "Cleaning lkvm garbages on ${machine}..."
	echo "ssh $machine \"sudo bash -c \"rm /root/.lkvm/*\"\""
	ssh $machine "sudo bash -c \"rm /root/.lkvm/*\""
	echo "ssh $machine \"sudo bash -c \"ls /root/.lkvm/\"\""
	ssh $machine "sudo bash -c \"ls /root/.lkvm/\""

	echo "CEALN kvm temporary sock files"
done

##########################
# compile guest kernel
#########################
### change flags
echo "set #define POPHYPE_HOST_KERNEL 0 in $KERNEL_PATH/include/popcorn/debug.h"
sed -i 's/#define POPHYPE_HOST_KERNEL 1/#define POPHYPE_HOST_KERNEL 0/g' $KERNEL_PATH/include/popcorn/debug.h
cat $KERNEL_PATH/.config | grep POPHYPE_HOST_KERNEL

echo "set CONFIG_RCU_STALL_COMMON=n in $KERNEL_PATH/.config"
# sed -i 's/RCU_STALL_COMMON=y/RCU_STALL_COMMON=n/g' $KERNEL_PATH/.config
sed -i 's/CONFIG_RCU_STALL_COMMON=y/CONFIG_RCU_STALL_COMMON=n/g' $KERNEL_PATH/.config
cat $KERNEL_PATH/.config | grep CONFIG_RCU_STALL_COMMON
grep -r POPHYPE_HOST_KERNEL $KERNEL_PATH/include/popcorn/debug.h

tmp=`grep -r POPHYPE_HOST_KERNEL $KERNEL_PATH/include/popcorn/debug.h | awk '{print$3}'`
echo "Jack $tmp"
if [[ tmp != 0 ]]; then
	printf "\n\n\n\n\n"
	printf "\t\tGOOD GUEST KERNEL!!!\n"
	printf "\n\n\n\n\n"
else
	echo "DIE: $1"
	exit -1
fi

old_val=$(cat $KERNEL_PATH/.version)
new_val=0
sed -i 's/[0-9]*/0/g' $KERNEL_PATH/.version

if [[ $2 != "0" ]]; then
	compile_guest_kernel
fi

### restore flags
echo "restore #define POPHYPE_HOST_KERNEL 1 in $KERNEL_PATH/include/popcorn/debug.h"
sed -i 's/#define POPHYPE_HOST_KERNEL 0/#define POPHYPE_HOST_KERNEL 1/g' $KERNEL_PATH/include/popcorn/debug.h
cat $KERNEL_PATH/.config | grep POPHYPE_HOST_KERNEL

echo "restore CONFIG_RCU_STALL_COMMON=y in $KERNEL_PATH/.config"
sed -i 's/CONFIG_RCU_STALL_COMMON=n/CONFIG_RCU_STALL_COMMON=y/g' $KERNEL_PATH/.config
cat $KERNEL_PATH/.config | grep CONFIG_RCU_STALL_COMMON


sed -i 's/[0-9]*/12345/g' $KERNEL_PATH/.version


sleep 3


#
# Network enable here
#
sleep 120 && echo "sudo brctl addif br0 tap0" && echo "brctl show" && echo "TODO"&
sleep 60 && echo "sudo brctl addif br0 tap0" && sudo brctl addif br0 tap0 && echo "brctl show" &
ret=`sudo brctl show |wc -l`
while [[ $ret == 3 ]]; do
	echo "sleep 10 brctl addif"
	sleep 10
	echo "sudo brctl addif br0 tap0"
	sudo brctl addif br0 tap0
	echo "brctl show"
	ret=`brctl show |wc -l`
	echo "ret=" && echo $ret
	#brctl show
done
#
# Boot LKVM here
#

#Experiment for helloworld (taskset -c <?> yes>/dev/null) and Nginx
#Uncoment for using this, please comment others
sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -y 1 -w 4 -i $USER/fragvisor-kvmtool/ramdisk_NPB_ATC.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 32768 -c 4 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=4 percpu_alloc=page -no-kvm-pit-reinjection clocksource=tsc\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"

#Experiment for LEMP Uncoment for using this, please comment others
#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -y 1 -w 4 -i $USER/fragvisor-kvmtool/ramdisk_lemp_4php.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 32768 -c 4 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=4 percpu_alloc=page -no-kvm-pit-reinjection clocksource=tsc\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"

#Experiment for Openlambda Uncoment for using this, please comment others
#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -y 1 -w 4 -i $USER/fragvisor-kvmtool/ramdisk_openlambda.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 32768 -c 4 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=4 percpu_alloc=page -no-kvm-pit-reinjection clocksource=tsc\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"
