#! /bin/bash
#
# auto_setup.sh
# Copyright (C) 2019 jackchuang <jackchuang@echo>
#
# Distributed under terms of the MIT license.
#
echo -e "\n\n\n\n\n\n\n\n\n\n\n\n\n"
SUBNET="10.1.10"
# easy solution...
export PATH=$PATH:/usr/local/bin:/usr/bin:/bin:/usr/local/games:/usr/games:/sbin:/usr/sbin/:/opt/DIS/bin:/opt/DIS/bin/socket:/usr/local/bin:/usr/bin:/bin:/usr/local/games:/usr/games:/sbin:/usr/sbin/:/sbin
# Debug mode : turn this off
echo "If debug mode, turn this off"
echo "echo 1 1 1 1 > /proc/sys/kernel/printk"
echo 1 1 1 1 > /proc/sys/kernel/printk
# sudo bash -c "sudo echo 1 1 1 1 > /proc/sys/kernel/printk"
#
echo "auto_setup - net setup"
echo
echo "ifconfig lo 127.0.0.1"
ifconfig lo 127.0.0.1
taskset 0x1 ping -c 1 127.0.0.1
taskset 0x2 ping -c 1 127.0.0.1
taskset 0x4 ping -c 1 127.0.0.1
taskset 0x1 ping -c 1 localhost
taskset 0x2 ping -c 1 localhost
taskset 0x4 ping -c 1 localhost
#ifconfig eth0 $SUBNET.135
echo "ifconfig eth0 $SUBNET.222"
#ifconfig eth0 192.168.33.2
ifconfig eth0 $SUBNET.222
#taskset 0x1 ping -c 1 $SUBNET.115
#taskset 0x1 ping -c 1 $SUBNET.115
taskset 0x1 ping -c 1 $SUBNET.15
taskset 0x2 ping -c 1 $SUBNET.15
taskset 0x4 ping -c 1 $SUBNET.15
taskset 0x1 ping -c 1 $SUBNET.1
taskset 0x2 ping -c 1 $SUBNET.1
taskset 0x4 ping -c 1 $SUBNET.1
#taskset 0x1 ping -c 1 192.168.33.1 #?
#taskset 0x2 ping -c 1 192.168.33.1 #?
#taskset 0x4 ping -c 1 192.168.33.1 #?
echo -e "\n\n\n"
echo "= DNS ="
echo "fix DNS by \"sudo resolvconf -u\""
resolvconf -u
echo "\n\n\n\n"
echo "= route ="
echo "tap dev is not used!!!!! (very confusing)"
echo "route add default gw $SUBNET.1"
route add default gw $SUBNET.1
#echo "route add default gw 192.168.33.1"
#route add default gw 192.168.33.1
echo ""
echo "auto_setup - net Done!!"
echo ""
echo ""
echo "auto_setup - ssh"
echo "[mir] configure bin_src/openssh-8.1p1 with --with-pam on mir. (./configure --with-pam)"
echo "[mir] ssh-keygen -A (mir needs it)"
taskset 0x1 ssh-keygen -A
echo "auto do \"taskset 0x1 /bin/sshd -D&\""
taskset 0x1 /bin/sshd -D& # ssh root@192.168.33.2 <commnad>
#taskset 0x1 /bin/sshd -Dd& # -d: debug
echo "mount -t devpts devpts /dev/pts (required for interactive ssh)"
mkdir -p /dev/pts # just in case
mount -t devpts devpts /dev/pts
mkdir -p /dev/shm # for ipc shm program (my usr dsm fault test)
echo "host:~$ ssh root@$SUBNET.222 <command>" #usage outside
echo
echo
echo "auto_setup (Net is up) Done!!"
echo
echo "examples:"
echo -e "\ttaskset -p 0x1 <pid>"
echo -e "\ttaskset -p 0x2 <pid>"
echo
#taskset -p 0x1 6
#taskset -p 0x1 17
#taskset -p 0x1 17
#taskset -p 0x1 18
#taskset -p 0x1 19
#taskset -p 0x1 20
#taskset -p 0x1 25
#taskset -p 0x1 26
#taskset -p 0x1 27
#taskset -p 0x1 28
#taskset -p 0x1 32
#taskset -p 0x1 33
#taskset -p 0x1 38
#taskset -p 0x1 47
#taskset -p 0x1 48
echo
echo
echo
echo
echo "Run auto main scripts"
echo
echo
#auto_setup_apt_install.sh

#either one
#auto_setup_nginx.sh # micro
#auto_setup_lemp.sh # lemp

#auto_setup_openlambda.sh # openlambda

#auto_setup_deathstar.sh
#auto_setup_deathstar.sh
#auto_taskset.sh # cpu affinity show and reconfig

# Auto install docker
#install_docker.sh

echo "mir cannot find hostname, so # hostname vm"
hostname vm

echo
echo
echo "- Selfcheck -"
export
ps aux |grep sshd
ln -sv /usr/local/bin/python3.7 /usr/local/bin/python
ln -sv /usr/local/bin/python3.7 /usr/local/bin/python3
ln -sv /etc/localtime /var/empty/sshd/etc/localtime
python --version
python3 --version
echo -e "\n\n\n================\n"
echo "DON'T USE sudo, OTHERWISE init_puzzlehype will crash in silence on mir !!!!!!!!"
echo -e "============\n\n\n"
#python3.7 --version # Only for DEADSTART
echo
echo "- Cheat sheet -"
echo "DSM traffic debuging (host)"
echo "sudo bash -c \"echo > /sys/kernel/debug/tracing/trace\"" #clean
echo "sudo bash -c \"echo 1000000 > /sys/kernel/debug/tracing/buffer_size_kb\""
echo "sudo bash -c \" echo 1 > /sys/kernel/debug/tracing/events/popcorn/vmdsm_traffic/enable\""
echo "<run>"
echo sudo bash -c "cat /sys/kernel/debug/tracing/trace > /tmp/my_trace"
echo sudo chown jackchuang:jackchuang /tmp/my_trace
echo
echo -e "\e[32mInitstrap done (dont run bash here) !\e[39m"
echo
echo "<cpu>"
echo "echo 1 > /sys/devices/system/cpu/cpu0/online"
echo
ps aux |grep php
echo "taskset -p 0x2 "
#echo "enter bash (sudo -i)" # enter colorful bash
#sudo -i
echo "========================"
echo "| $0 ALL DONE |"
echo "========================"

