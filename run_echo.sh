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

#KERNEL_PATH=~/kh
#KERNEL_PATH=~/kh_jack # Jack development
KERNEL_PATH=~/kh_karim # Karim development

USER=/home/jackchuang/
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
	ssh $THISNODE make -j32 -C $KERNEL_PATH/
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
	make -j16
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
	ssh jackchuang@$machine "sudo bash -c \"ls /root/.lkvm/\""

	echo "CEALN kvm temporary sock files"
done

##########################
# compile guest kernel
#########################
### change flags
#echo "set #define POPHYPE_HOST_KERNEL 0 in $KERNEL_PATH/include/popcorn/hype_kvm.h"
#sed -i 's/#define POPHYPE_HOST_KERNEL 1/#define POPHYPE_HOST_KERNEL 0/g' $KERNEL_PATH/include/popcorn/hype_kvm.h
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
#echo "\"cat $KERNEL_PATH/.version\" old_val $old_val new_val $new_val"
#echo "cat $KERNEL_PATH/.version ($old_val)"
#str="'s/[0-9]*/${new_val}/g'"
##sed -i \'s/[0-9]*/${new_val}/g\' $KERNEL_PATH/.version
#echo "sed -i $str $KERNEL_PATH/.version"
#sed -i $str $KERNEL_PATH/.version
#echo "check:"
#cat $KERNEL_PATH/.version
sed -i 's/[0-9]*/0/g' $KERNEL_PATH/.version
###

if [[ $2 != "0" ]]; then
	compile_guest_kernel
fi

### restore flags
#echo "restore #define POPHYPE_HOST_KERNEL 1 in $KERNEL_PATH/include/popcorn/hype_kvm.h"
#sed -i 's/#define POPHYPE_HOST_KERNEL 0/#define POPHYPE_HOST_KERNEL 1/g' $KERNEL_PATH/include/popcorn/hype_kvm.h
echo "restore #define POPHYPE_HOST_KERNEL 1 in $KERNEL_PATH/include/popcorn/debug.h"
sed -i 's/#define POPHYPE_HOST_KERNEL 0/#define POPHYPE_HOST_KERNEL 1/g' $KERNEL_PATH/include/popcorn/debug.h
cat $KERNEL_PATH/.config | grep POPHYPE_HOST_KERNEL

echo "restore CONFIG_RCU_STALL_COMMON=y in $KERNEL_PATH/.config"
# sed -i 's/RCU_STALL_COMMON=n/RCU_STALL_COMMON=y/g' $KERNEL_PATH/.config
sed -i 's/CONFIG_RCU_STALL_COMMON=n/CONFIG_RCU_STALL_COMMON=y/g' $KERNEL_PATH/.config
cat $KERNEL_PATH/.config | grep CONFIG_RCU_STALL_COMMON

#echo "restore $KERNEL_PATH/.version old_val $old_val"
##sed -i \'s/[0-9]*/${old_val}/g\' $KERNEL_PATH/.version
#str="'s/[0-9]*/${old_val}/g'"
#echo "sed -i $str $KERNEL_PATH/.version"
#sed -i $str $KERNEL_PATH/.version
#echo "check:"
#cat $KERNEL_PATH/.version

sed -i 's/[0-9]*/12345/g' $KERNEL_PATH/.version
###


echo "If you change # of vcpu or node, change code as well !!!!!!"
echo "If you change # of vcpu or node, change code as well !!!!!!"
echo "If you change # of vcpu or node, change code as well !!!!!!"
echo
echo -e "egrep -r \"\" . --exclude-dir=pophype_make_ramdisk\n\n\n"
sleep 3

#########################
### RUN
#########################
### ATTENTION -a -b cannot be put in the end since there is a chance when argv > 12, it will not detect -a -b somehow
########################
# doing (GOOD)
#sudo bash -c "./lkvm run -k $KERNEL_PATH/arch/x86/boot/bzImage -m 4096 -c 2 -p "root=/dev/vda1 console=ttyS0" -d $USER/c/x86_0.img -a 1 -b 1"
#sudo bash -c "./lkvm run -k $KERNEL_PATH/arch/x86/boot/bzImage -m 4096 -c 2 -p "root=/dev/vda1" -d $USER/c/x86_0.img -a 1 -b 1"
#looks like "console=ttyS0" will make popcorn crash

#sudo bash -c "./lkvm run -k $KERNEL_PATH/arch/x86/boot/bzImage -m 4096 -c 2 -p "root=/dev/vda1" -d $USER/c/x86_0.img -a 1 -b 1"

# testing # absolute pwd is required for remote nodes to open the fd
#sudo bash -c "./lkvm run -k $KERNEL_PATH/arch/x86/boot/bzImage -m 4096 -c 2 -p "root=/dev/vda1" -d $USER/c/x86_0.img -a 1 -b 1"
#good
#sudo bash -c "./lkvm run -a 1 -b 1 -k $KERNEL_NAME/arch/x86/boot/bzImage -m 4096 -c 2 -p "root=/dev/vda1" -d $USER/c/x86_0.img"

#good
#sudo bash -c "./lkvm run -a 1 -b 1-i $USER/c/initrd.img-4.4.137-pop-hype -k $KERNEL_PATH/arch/x86/boot/bzImage -m 4096 -c 2 -p "root=/dev/vda1" -d $USER/c/x86_0.img"
#good
##sudo bash -c "./lkvm run -a 1 -b 1 -i $USER/c/initrd.img-4.4.137-pop-hype -k $KERNEL_PATH/arch/x86/boot/bzImage -m 4096 -c 2 -p "root=/dev/vda1 console=ttyS0" -d $USER/c/x86_0.img"
#sudo bash -c "./lkvm run -a 1 -b 1 -i $USER/c/initrd.img-4.4.137-pop-hype -k $KERNEL_PATH/arch/x86/boot/bzImage -m 4096 -c 2 -p "root=/dev/vda1" -d $USER/c/x86_0.img"

# may crash 
# 1. at _IO_vfprintf_internal() ( BAD [2674] ->[-1/-1] addr 710000 instr 47908b)
# 2. [8839] do_sys_open: popcorn [OPEN] fd -17 file ffffffffffffffef at remote
#sudo bash -c "./lkvm run -a 1 -b 1 -k $KERNEL_PATH/arch/x86/boot/bzImage -m 4096 -c 2 -p "root=/dev/vda1 console=ttyS0" -d $USER/c/x86_0.img"


# GOOD
#sudo bash -c "./lkvm run -k $KERNEL_PATH/arch/x86/boot/bzImage -m 4096 -c 2 -p "root=/dev/vda1" -d x86_0.img"
###sudo bash -c "gdb --args ./lkvm run -k $KERNEL_PATH/arch/x86/boot/bzImage -m 4096 -c 2 -p "root=/dev/vda1" -d x86_0.img"
#sudo bash -c "./lkvm run -k $KERNEL_PATH/arch/x86/boot/bzImage -m 4096 -c 2 -p "root=/dev/vda1 console=/dev/null" -d x86_0.img"
#sudo bash -c "./lkvm run -k $KERNEL_PATH/arch/x86/boot/bzImage -m 4096 -c 2 -p "root=/dev/vda1 console=null" -d x86_0.img"
#sudo ./lkvm run -d vm1 -k ../linux-4.9/arch/x86/boot/bzImage
#./lkvm run -k ../linux-4.9/arch/x86/boot/bzImage -m 4096 -c 8 -p "root=/dev/vda1" -d x86_0.img


###### testing gooe ramdisk
#cpying
#sudo bash -c "./lkvm run -i $USER/c2/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 4096 -c 2 -p \"root=/dev/vda rw console=ttyS0 init=/init2\""
# adopting
# ext4 /linuxrc
#sudo bash -c "./lkvm run -a 1 -b 1 -i $USER/c2/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 4096 -c 2 -p \"root=/dev/vda rw init=/init2\""
# ram /init2
# doesn't help
#sudo bash -c "./lkvm run -a 1 -b 1 -i $USER/c2/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 4096 -c 2 -p \"root=/dev/ram rw init=/init2 console=ttyS0\""
# good
#sudo bash -c "./lkvm run -a 1 -b 1 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 4096 -c 2 -p \"root=/dev/ram rw init=/init2\""

#multi-core
#good
#sudo bash -c "./lkvm run -a 4 -b 4 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 4096 -c 2 -p \"root=/dev/ram rw init=/init2 fstype=ext4\""
#fail
#sudo bash -c "./lkvm run -a 5 -b 5 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 4096 -c 2 -p \"root=/dev/ram rw init=/init2 fstype=ext4\""

#multi-node
#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -y 1 -w 4  -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 4096 -c 2 -p \"root=/dev/ram rw init=/init2 fstype=ext4\""
#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -w 3  -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 4096 -c 2 -p \"root=/dev/ram rw init=/init2 fstype=ext4\""
#sudo bash -c "./lkvm run -a 1 -b 1 -w 2  -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 4096 -c 2 -p \"root=/dev/ram rw init=/init2 fstype=ext4\""
#sudo bash -c "./lkvm run -a 1 -b 1 -w 2  -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 2048 -c 2 -p \"root=/dev/ram rw init=/init2 fstype=ext4\"" # low mem
#sudo bash -c "./lkvm run -a 1 -w 1  -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 4096 -c 2 -p \"root=/dev/ram rw init=/init2 fstype=ext4\""

#2node4cpu
#sudo bash -c "./lkvm run -a 4 -b 4 -w 2 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 4096 -c 2 -p \"root=/dev/ram rw init=/init2 fstype=ext4\""

# testing 2node1cpu log for solving "term-poll" bug
#sudo bash -c "./lkvm run -a 1 -b 1 -w 2 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 4096 -c 2 -p \"root=/dev/ram rw init=/init2 fstype=ext4\""

#3node1cpu (good)
#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -w 3 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 4096 -c 2 -p \"root=/dev/ram rw init=/init2 fstype=ext4\""

# 3 node 1 cpu running (using)
#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -w 3 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 4096 -c 3 -p \"root=/dev/ram rw init=/init2 fstype=ext4 apic=debug\""

# 2 node 1 cpu running
#sudo bash -c "./lkvm run -a 1 -b 1 -w 2 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 4096 -c 2 -p \"root=/dev/ram rw init=/init2 fstype=ext4 apic=debug\""

# virtio serial port (X)
#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -w 3 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 4096 -c 3 -p \"root=/dev/ram rw init=/init2 fstype=ext4 apic=debug\" --console virtio"

# net --network virtio (good was using)
#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -w 3 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 4096 -c 3 -p \"root=/dev/ram rw init=/init2 fstype=ext4 apic=debug\" --network virtio"

#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -w 3 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 4096 -c 3 -p \"root=/dev/ram rw init=/init2 fstype=ext4 apic=debug\" --network virtio --console virtio"

# ++ no pti spectre_v2=off nopti pti=off
#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -w 3 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 4096 -c 3 -p \"root=/dev/ram rw init=/init2 fstype=ext4 apic=debug spectre_v2=off nopti pti=off\" --network virtio"

############# remove apic=debug for exp data

# no pti, no numa
#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -w 3 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 4096 -c 3 -p \"root=/dev/ram rw init=/init2 fstype=ext4\" --network virtio"

# only numa
#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -w 3 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 4096 -c 3 -p \"root=/dev/ram rw init=/init2 fstype=ext4 numa=fake=3\" --network virtio"

# only pti
# sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -w 3 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 4096 -c 3 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off\" --network virtio"

# pti + numa numa=fake=3
#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -w 3 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 4096 -c 3 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=3\" --network virtio"

# vhost-net e.g. $ lkvm run -kernel /path/to/bzImage -net mode=tap,vhost=1
#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -w 3 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 4096 -c 3 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=3\" --network mode=tap,vhost=1"

# larger mem size
# sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -w 3 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 16384 -c 3 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=3\" --network mode=tap,vhost=1"

# 4 cpu
# sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -y 1 -w 4 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 28672 -c 4 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=4\" --network mode=tap,vhost=1"

# 3 vcpu
# sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -w 3 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 28672 -c 3 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=3\" --network mode=tap,vhost=1"

# 2 vcpu
# sudo bash -c "./lkvm run -a 1 -b 1 -w 2 -c 2 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 28672 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=2\" --network mode=tap,vhost=1"



sleep 120 && echo "sudo brctl addif br0 tap0" && echo "brctl show" && echo "TODO"&
sleep 120 && echo "sudo brctl addif br0 tap0" && sudo brctl addif br0 tap0 && echo "brctl show" &
ret=`brctl show |wc -l`
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
#sleep 300 && echo "sleep 300 done"&
#sleep 360 && echo "sleep 360 done"&
#sleep 480 && echo "sleep 480 done"&
#sleep 600 && echo "sleep 600 done"&
#sleep 60 && sudo brctl addif br0 tap0 && brctl show&
# 4 cpu
# sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -y 1 -w 4 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 28672 -c 4 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=4\" --network mode=tap,vhost=1,guest_ip=192.168.33.2,host_ip=192.168.33.1,guest_mac=00:11:22:33:44:55"

# 2 cpu
# sudo bash -c "./lkvm run -a 1 -b 1 -w 2 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 28672 -c 2 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=2\" --network mode=tap,vhost=1,guest_ip=192.168.33.2,host_ip=192.168.33.1,guest_mac=00:11:22:33:44:55"

# 2 cpu (reduced mem)
#sudo bash -c "./lkvm run -a 1 -b 1 -w 2 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 14336 -c 2 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=2\" --network mode=tap,vhost=1,guest_ip=192.168.33.2,host_ip=192.168.33.1,guest_mac=00:11:22:33:44:55"
#sudo bash -c "./lkvm run -a 1 -b 1 -w 2 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 14336 -c 2 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=2\" --network mode=tap,vhost=1,guest_ip=192.168.33.2,host_ip=192.168.33.1,guest_mac=00:11:22:33:44:55"
#sudo bash -c "./lkvm run -a 1 -b 1 -w 2 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 14336 -c 2 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=2\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"
#sudo bash -c "./lkvm run -a 1 -b 1 -w 2 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 28672 -c 2 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=2\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"
#sudo bash -c "./lkvm run -a 1 -b 1 -w 2 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 8192 -c 2 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=2\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55" # CRASH - out of memory (8G)... !!!!!!!!!!!!!
# sudo bash -c "./lkvm run -a 1 -b 1 -w 2 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 20480 -c 2 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=2\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55" # -m 20480 for deathstar
# sudo bash -c "./lkvm run -a 1 -b 1 -w 2 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 8192 -c 2 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=2\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"

# percpu = page
# sudo bash -c "./lkvm run -a 1 -b 1 -w 2 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 8192 -c 2 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=2 percpu_alloc=page\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55" # working
#sudo bash -c "./lkvm run -a 1 -b 1 -w 2 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 8192 -c 2 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=2\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"

# size
# 12288 12582912
# 14336 14680064
# 16384 16777216

# need more space for db
#sudo bash -c "./lkvm run -a 1 -b 1 -w 2 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 10240 -c 2 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=2 percpu_alloc=page\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"
#sudo bash -c "./lkvm run -a 1 -b 1 -w 2 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 14336 -c 2 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=2 percpu_alloc=page\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"

###############################################################
######################## systro20 #############################
###############################################################
# 4 last (for perf) 18432 (20480) (before systro20 last)
#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -y 1 -w 4 -i $USER/c/ramdisk_balv_200827.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 20480 -c 4 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=4 percpu_alloc=page clocksource=jiffies\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"


#######################################################################
###################### ATC (NPB AND PARSEC) ###########################
#######################################################################
#PARSEC
#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -y 1 -w 4 -i $USER/c/ramdisk_NPB_ATC.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 32768 -c 4 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=4 percpu_alloc=page\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"

#04/06/2021
sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -y 1 -w 4 -i $USER/c/ramdisk_NPB_ATC.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 32768 -c 4 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=4 percpu_alloc=page -no-kvm-pit-reinjection clocksource=tsc\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"

#Unoptimized
#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -y 1 -w 4 -i $USER/c/ramdisk_NPB_ATC.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 32768 -c 4 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=on nopti pti=on numa=fake=4 percpu_alloc=page -no-kvm-pit-reinjection clocksource=tsc\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"
#######################################################################
########################### END #######################################
#######################################################################

#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -y 1 -w 4 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 32768 -c 4 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=4 percpu_alloc=page\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"

#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -y 1 -w 4 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 20480 -c 4 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=4\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"
#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -y 1 -w 4 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 20480 -c 4 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off percpu_alloc=page\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"
#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -y 1 -w 4 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 20480 -c 4 -p \"root=/dev/ram rw init=/init2 fstype=ext4 numa=fake=4 percpu_alloc=page\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"

#sudo bash -c "./lkvm run -a 1 -b 1 -w 2 -i $USER/c/ramdisk_balv_200827.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 10240 -c 2 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=2 percpu_alloc=page\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"
#sudo bash -c "./lkvm run -a 1 -b 1 -w 2 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 20480 -c 2 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=2 percpu_alloc=page\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"
#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -y 1 -w 4 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 22528 -c 4 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=4 percpu_alloc=page\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"
#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -y 1 -w 4 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 14336 -c 4 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=4 percpu_alloc=page\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"
# TODO try no fake
#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -y 1 -w 4 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 14336 -c 4 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=4 percpu_alloc=page\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"

# 3
#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -w 3 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 57344 -c 3 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=3 percpu_alloc=page\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"

# 2 mircro2
#sudo bash -c "./lkvm run -a 1 -b 1 -w 2 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 57344 -c 2 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=2 percpu_alloc=page\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"

#sudo bash -c "./lkvm run -a 1 -b 1 -w 2 -i $USER/c/ramdisk_NPB_ATC.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 32768 -c 2 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=2 percpu_alloc=page\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"

#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -y 1 -w 4 -i $USER/c/ramdisk_bak200501systor20.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 20480 -c 4 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=4 percpu_alloc=page\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"

###############################################################
######################## NGINX  #############################
###############################################################

#2vCPU on 2 node
#sudo bash -c "./lkvm run -a 1 -b 1 -w 2 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 14336 -c 2 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=2 percpu_alloc=page\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"

#same ramdisk for 3/4vcpu setup


###############################################################
######################## LEMP  #############################
###############################################################

#4vCPU on 4 node
#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -y 1 -w 4 -i $USER/c/ramdisk_lemp_4php.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 20480 -c 4 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=4 percpu_alloc=page\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"
# jack adds -no-kvm-pit-reinjection clocksource=tsc
#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -y 1 -w 4 -i $USER/c/ramdisk_lemp_4php.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 20480 -c 4 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=4 percpu_alloc=page -no-kvm-pit-reinjection clocksource=tsc\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"

#Unoptimized Kernel
#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -y 1 -w 4 -i $USER/c/ramdisk_lemp_4php.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 20480 -c 4 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=on nopti pti=on numa=fake=4 percpu_alloc=page -no-kvm-pit-reinjection clocksource=tsc\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"


#3vCPU on 3 node
#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -w 3 -i $USER/c/ramdisk_lemp_3php.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 14336 -c 3 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=3 percpu_alloc=page\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"

#2vCPU on 2 node
#sudo bash -c "./lkvm run -a 1 -b 1 -w 2 -i $USER/c/ramdisk_lemp_2php.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 14336 -c 2 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=2 percpu_alloc=page\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"


#Jack: ramdisks for testing migration
#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -y 1 -w 4 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 14336 -c 4 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=4 percpu_alloc=page\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"
#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -y 1 -w 4 -i $USER/c/ramdisk_lemp_4php.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 14336 -c 4 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=4 percpu_alloc=page\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"

############################################################
############### below for openlambda #######################
############################################################
# 4 for openlambda (no init2)
# 4vcpu on 4 node 20G
#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -y 1 -w 4 -i $USER/c/ramdisk_for_openlambda200625.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 32768 -c 4 -p \"root=/dev/ram rw fstype=ext4 spectre_v2=off nopti pti=off numa=fake=4 percpu_alloc=page\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"

# test this balvansh
#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -y 1 -w 4 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 57344 -c 4 -p \"root=/dev/ram rw fstype=ext4 spectre_v2=off nopti pti=off numa=fake=4 percpu_alloc=page clocksource=jiffies\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"
#No fake numa
#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -y 1 -w 4 -i $USER/c/ramdisk_for_openlambda200625.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 26624 -c 4 -p \"root=/dev/ram rw fstype=ext4 spectre_v2=off nopti pti=off percpu_alloc=page\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"
# 22G
#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -y 1 -w 4 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 22528 -c 4 -p \"root=/dev/ram rw fstype=ext4 spectre_v2=off nopti pti=off numa=fake=4 percpu_alloc=page\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"

#04/05/2021 Open Lambda
#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -y 1 -w 4 -i $USER/c/ramdisk_for_openlambda200625.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 32768 -c 4 -p \"root=/dev/ram rw fstype=ext4 spectre_v2=off nopti pti=off numa=fake=4 percpu_alloc=page -no-kvm-pit-reinjection clocksource=tsc\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"


# 3vcpu on 3 node 20G
#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -w 3 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 57344 -c 3 -p \"root=/dev/ram rw fstype=ext4 spectre_v2=off nopti pti=off numa=fake=3 percpu_alloc=page\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"
# 2vcpu on 2node
#sudo bash -c "./lkvm run -a 1 -b 1 -w 2 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 57344 -c 2 -p \"root=/dev/ram rw fstype=ext4 spectre_v2=off nopti pti=off numa=fake=2 percpu_alloc=page\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"


#######3 16
# 4 nodes
#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -y 1 -w 4 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 16384 -c 4 -p \"root=/dev/ram rw fstype=ext4 spectre_v2=off nopti pti=off numa=fake=4 percpu_alloc=page\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"
# 3 nodes
#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -w 3 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 16384 -c 3 -p \"root=/dev/ram rw fstype=ext4 spectre_v2=off nopti pti=off numa=fake=3 percpu_alloc=page\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"
# 2 nodes
#sudo bash -c "./lkvm run -a 1 -b 1 -w 2 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 16384 -c 2 -p \"root=/dev/ram rw fstype=ext4 spectre_v2=off nopti pti=off numa=fake=2 percpu_alloc=page\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"


####### 18
# 4 nodes
#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -y 1 -w 4 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 18432 -c 4 -p \"root=/dev/ram rw fstype=ext4 spectre_v2=off nopti pti=off numa=fake=4 percpu_alloc=page\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"
# 3 nodes
#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -w 3 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 18432 -c 3 -p \"root=/dev/ram rw fstype=ext4 spectre_v2=off nopti pti=off numa=fake=3 percpu_alloc=page\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"
# 2 nodes
#sudo bash -c "./lkvm run -a 1 -b 1 -w 2 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 18432 -c 2 -p \"root=/dev/ram rw fstype=ext4 spectre_v2=off nopti pti=off numa=fake=2 percpu_alloc=page\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"
#sudo bash -c "./lkvm run -a 1 -w 1 -i $USER/c/ramdisk.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 18432 -c 1 -p \"root=/dev/ram rw fstype=ext4 spectre_v2=off nopti pti=off numa=fake=1 percpu_alloc=page\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"

##FOR JACK####
#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -y 1 -w 4 -i $USER/c/ramdisk_bak200501systor20.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 14336 -c 4 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=4 percpu_alloc=page\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"
#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -y 1 -w 4 -i $USER/c/ramdisk_NPB_ATC.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 14336 -c 4 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=4 percpu_alloc=page\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"

##FOR KARIM####
#sudo bash -c "./lkvm run -a 1 -b 1 -x 1 -y 1 -w 4 -i $USER/c/ramdisk_NPB_ATC.gz -k $KERNEL_PATH/arch/x86/boot/bzImage -m 32768 -c 4 -p \"root=/dev/ram rw init=/init2 fstype=ext4 spectre_v2=off nopti pti=off numa=fake=4 percpu_alloc=page\" --network mode=tap,vhost=1,guest_ip=10.4.4.222,host_ip=10.4.4.221,guest_mac=00:11:22:33:44:55"
