#!/bin/bash
#
# jack_make_ramdisk2.sh
# Copyright (C) 2019 jackchuang <jackchuang@echo5>
#
# Distributed under terms of the MIT license.
#
#RAMDISK_SIZE_MB=2048 # REMEMBER TO CHANGE KERNEL RAMDISK SIZE as well
#RAMDISK_SIZE_MB=4096 # REMEMBER TO CHANGE KERNEL RAMDISK SIZE as well
#RAMDISK_SIZE_MB=6114 # REMEMBER TO CHANGE KERNEL RAMDISK SIZE as well
RAMDISK_SIZE_MB=8192 # REMEMBER TO CHANGE KERNEL RAMDISK SIZE as well
# >= 10G init ramdisk takes very very slow or stuck (this process remote node will touch all ramdisk region as well (growing ram size)). The higher, the worse.
#RAMDISK_SIZE_MB=10240 # REMEMBER TO CHANGE KERNEL RAMDISK SIZE as well
#RAMDISK_SIZE_MB=12288 # REMEMBER TO CHANGE KERNEL RAMDISK SIZE as well - deathstar


ROOTFS_DIR=/tmp/rootfs # disapear while rebooting
DEBUG_ROOTFS_DIR=rootfs_dbg # todo use pwd to make this absolute
DEBUG_ROOTFS_DIR_DIR=rootfs_dir_dbg # todo use pwd to make this absolute


# User input
FORCE_CREATE_ROOTFS=$1
#FORCE_COPY_DB_DATA=$2
FORCE_CREATE_DEBUG_ROOTFS=$2
FORCE_CREATE_DEBOOTSTAP=$3
echo "USAGE: ./$0 <FORCE_CREATE_ROOTFS> <FORCE_CREATE_DEBUG_ROOTFS> <FORCE_CREATE_DEBOOTSTAP>"
#echo "USAGE: ./$0 <FORCE_CREATE_ROOTFS> <FORCE_COPY_DB_DATA> <FORCE_CREATE_DEBOOTSTAP>"
echo -e "\tFORCE_CREATE_ROOTFS: 1/0 (default:0) - $FORCE_CREATE_ROOTFS"
echo -e "\tFORCE_CREATE_DEBUG_ROOTFS: 1/0 (default:1) - $FORCE_CREATE_DEBUG_ROOTFS"
#echo -e "\tFORCE_COPY_DB_DATA: 1/0 (default:0) - $FORCE_COPY_DB_DATA"
echo -e "\tFORCE_CREATE_DEBOOTSTAP: 1/0 (default:0) - $FORCE_CREATE_DEBOOTSTAP" # no need since I can smartly detect it
echo -e "\n This script is not distributed, but ramdisk is."
echo -e "\tend"

HOST_TYPE=`uname -n | sed 's/[0-9]*//g'`

echo "read README" #.md
echo "==="
echo "prerequisite:"
sudo apt install pigz -o Dpkg::Options::="--force-confdef" -y
sudo apt install build-essential zlib1g-dev libncurses5-dev libgdbm-dev libnss3-dev libssl-dev libreadline-dev libffi-dev curl libbz2-dev -o Dpkg::Options::="--force-confdef" -y
sudo apt install libpam0g-dev libselinux1-dev -o Dpkg::Options::="--force-confdef" -y
sudo apt install resolvconf haveged -o Dpkg::Options::="--force-confdef" -y
if [[ $HOST_TYPE == "mir" ]]; then
	export VM_IP=10.1.10.222 # mir
    echo "I'm on mir rack. My VM IP will be $VM_IP"
elif [[ $HOST_TYPE == "echo" ]]; then
	export VM_IP=10.4.4.222 # echo
    echo "I'm on echo rack. My VM IP will be $VM_IP"
fi
echo

# Heads-up
echo "make sure you have a busybox source folder in the same folder of this script"
echo "TODO: lookup busybox name"
echo "TODO: make && make install busybox"
echo

# Time: start
start_time=$( date +%s.%N )

# Clean
#echo "Clean..."
#sudo rm -rf $ROOTFS_DIR
#sudo rm -rf tmpfs
#sudo rm -rf ramdisk*

clean_cache() {
	sudo bash -c "sudo echo 1 > /proc/sys/vm/drop_caches"
	sudo bash -c "sudo echo 2 > /proc/sys/vm/drop_caches"
	sudo bash -c "sudo echo 3 > /proc/sys/vm/drop_caches"
}

clean_cache
echo "Check if $ROOTFS_DIR exists: fast-/slow- path"
if [[ $FORCE_CREATE_ROOTFS != 1 && -d $ROOTFS_DIR ]]; then
	echo -e "Result: fastpath\n\n"
else
	echo "Force to create or empty $ROOTFS_DIR - clean..."
	# Clean
	echo -e "\n\n\n\n============\nClean... (rm $ROOTFS_DIR)\n=============\n\n\n\n"
	sudo rm -rf $ROOTFS_DIR
	#sudo rm -rf tmpfs
	#sudo rm -rf ramdisk*

	# Create
	echo "Create..."
	sudo mkdir -vp $ROOTFS_DIR > /dev/null

	################################################
	################# OpenLambda Specific
	##### TESTING ######
	################################################
	##########
	# openlambda open-lambda
	####
	# Leverage existing x86.img
#	echo "[ Leverage x86.img ]"
#	DISKIMG_MOUNT_POINT=/tmp/iso
#	sudo mkdir $DISKIMG_MOUNT_POINT
#	sudo mount -t auto -o offset=1048576,ro /home/jackchuang/tong/t_x86.img $DISKIMG_MOUNT_POINT
#	#sudo mount -t auto -o offset=1048576,ro /home/jackchuang/c/x86_0.img $DISKIMG_MOUNT_POINT
#	sudo cp -arp $DISKIMG_MOUNT_POINT/* $ROOTFS_DIR/
#	#sudo cp -varp $DISKIMG_MOUNT_POINT/* $ROOTFS_DIR/
#	sudo sync
#	sudo umount $DISKIMG_MOUNT_POINT
#	# shrink size (remove x86.img contents)
#	sudo rm -r $ROOTFS_DIR/lib/modules/
#	sudo rm -r $ROOTFS_DIR/boot
#	sudo rm -r $ROOTFS_DIR/var/lib/apt
#	sudo rm -r $ROOTFS_DIR/var/cache
#	sudo rm -r $ROOTFS_DIR/var/backups
#	sudo rm -r $ROOTFS_DIR/var/log
#	sudo rm -r $ROOTFS_DIR/usr/share
#	sudo rm -r $ROOTFS_DIR/usr/lib/x86_64-linux-gnu
#	sudo rm -r $ROOTFS_DIR/usr/lib/gcc
#	sudo rm -r $ROOTFS_DIR/usr/lib/debug
#	sudo rm -r $ROOTFS_DIR/usr/lib/python2.7
#	#sudo rm -r $ROOTFS_DIR/home/popcorn
#
#	sudo rm -r $ROOTFS_DIR/etc/fstab ######## no disk

	#sudo rm -r $ROOTFS_DIR/etc/init.d # shorter systemd
	#END OpenLambda


	# Real Start
	echo "INFO: retired since from deathstar"
	#echo "TODO: try to remove busybox (can we just rely on debootsrap?)"
	#sudo cp ../busybox-1.30.1/_install/* $ROOTFS_DIR -raf
	sudo mkdir -vp $ROOTFS_DIR/bin/
	sudo mkdir -vp $ROOTFS_DIR/sbin/
	sudo mkdir -vp $ROOTFS_DIR/usr/bin/
	sudo mkdir -vp $ROOTFS_DIR/usr/sbin/
	sudo mkdir -vp $ROOTFS_DIR/usr/local/
	sudo mkdir -vp $ROOTFS_DIR/etc/

	sudo mkdir -vp $ROOTFS_DIR/usr/include
	sudo mkdir -vp $ROOTFS_DIR/proc/
	sudo mkdir -vp $ROOTFS_DIR/sys/
	sudo mkdir -vp $ROOTFS_DIR/tmp/
	sudo chmod 777 -R $ROOTFS_DIR/tmp/
	ls -alh $ROOTFS_DIR | grep tmp
	sudo mkdir -vp $ROOTFS_DIR/root/
	sudo mkdir -vp $ROOTFS_DIR/var/
	sudo mkdir -vp $ROOTFS_DIR/mnt/
	echo "$ROOTFS_DIR/var/run/*: built by runtime (no control here)"
	echo

	# bin & lib
	sudo mkdir -p $ROOTFS_DIR/usr/local/bin
	sudo mkdir -p $ROOTFS_DIR/usr/local/lib/

	echo "Create dev"
	sudo mkdir -p $ROOTFS_DIR/dev/
	sudo mknod $ROOTFS_DIR/dev/tty1 c 4 1
	sudo mknod $ROOTFS_DIR/dev/tty2 c 4 2
	sudo mknod $ROOTFS_DIR/dev/tty3 c 4 3
	sudo mknod $ROOTFS_DIR/dev/tty4 c 4 4
	sudo mknod $ROOTFS_DIR/dev/console c 5 1
	sudo mknod $ROOTFS_DIR/dev/null c 1 3
	#sudo mknod -m 666  $ROOTFS_DIR/dev/ttyS0 c 4 64 # for openlambda

	echo "Attention: leveraging local /etc folder!!!"
	sudo du -sh /etc
	sudo cp /etc $ROOTFS_DIR/ -arf

##########
# openlambda open-lambda
####
#	sudo rm -vr $ROOTFS_DIR/etc/init.d
#	sudo rm -vr $ROOTFS_DIR/etc/fstab
#	sudo rm -vr $ROOTFS_DIR/share
##	sudo rm -vr $ROOTFS_DIR/lib/systemd/system
#done

	echo "Recreate folders"
	sudo mkdir -p $ROOTFS_DIR/usr/lib/
	sudo mkdir -p $ROOTFS_DIR/usr/local/

	# shared something....a lots need
	sudo mkdir -p $ROOTFS_DIR/usr/share/
	sudo mkdir -p $ROOTFS_DIR/share/ # 755

	###########################################################
	# Main dynamic libraries & bin
	echo "Copy localhost dynamic lib and bin... (take a while)"
	echo "TODO try 2. main BUG - cannot boot (I've increaded ram size to 4g, try again)"
	#sudo du -sh /usr/bin/
	#sudo cp -arfd /usr/bin/* $ROOTFS_DIR/bin/
	echo -e "========\n\n"
	sudo mkdir -vp $ROOTFS_DIR/lib
	echo "Copy library"
	echo "cpy /usr/lib/x86_64-linux-gnu/* ..."
	sudo mkdir -vp $ROOTFS_DIR/usr/lib/x86_64-linux-gnu/
	sudo du -sh /usr/lib/x86_64-linux-gnu/
	sudo cp -arfd /usr/lib/x86_64-linux-gnu/* $ROOTFS_DIR/usr/lib/x86_64-linux-gnu # 651M
	# ls /lib/x86_64-linux-gnu/
	# /usr/x86_64-linux-gnu
	echo "cpy /lib/x86_64-linux-gnu/* ..." # apt
	sudo du -sh /lib/x86_64-linux-gnu/
	sudo cp -arfd /lib/x86_64-linux-gnu/ $ROOTFS_DIR/lib/ # may trigger problem
	#sudo cp -arfd /lib/x86_64-linux-gnu/* $ROOTFS_DIR/lib/ # if triggers, put this on
	echo -e "\n\n"
	echo "For \"find\""
	sudo du -sh /usr/share/misc/
	sudo cp -varfd /usr/share/misc $ROOTFS_DIR/usr/share
	sudo cp -varfd /usr/share/file $ROOTFS_DIR/usr/share
	# Copy utils
	################################
	echo "Overwrite resolve"
	sudo rm $ROOTFS_DIR/etc/resolv.conf
	sudo bash -c "sudo echo \"nameserver 198.82.247.69\" > $ROOTFS_DIR/etc/resolv.conf"
	#sudo echo "nameserver 10.4.4.1" > $ROOTFS_DIR/etc/resolv.conf
	################################
	echo "copy some utils bins"
	sudo cp -varf /usr/bin/file $ROOTFS_DIR/usr/bin/
	sudo cp -varf /usr/bin/htop $ROOTFS_DIR/usr/bin/
	sudo cp -varf /usr/bin/killall $ROOTFS_DIR/usr/bin/
	sudo cp -varf /usr/bin/vim $ROOTFS_DIR/usr/bin/
	sudo cp -varf /usr/bin/gawk $ROOTFS_DIR/usr/bin/ # real awk
	sudo cp -varf /usr/bin/awk $ROOTFS_DIR/usr/bin/
	echo -e "\tmake"
	sudo cp -varf /usr/bin/make $ROOTFS_DIR/usr/bin/
	#sudo cp -varf bin_src/dbus-1.12.10 $ROOTFS_DIR/root/ # (testing) move me to other place
	#sudo cp -varf bin_src/systemd-216 $ROOTFS_DIR/root/ # (testing) move me to other place
	

	sudo cp -varf /bin/ps $ROOTFS_DIR/usr/bin # works ($ROOTFS_DIR/bin CRASH)
	#sudo cp -varf /bin/date $ROOTFS_DIR/usr/bin # CRASH!
	#sudo cp -varf /bin/date $ROOTFS_DIR/bin # CRASH!
	sudo cp -varf /usr/sbin/haveged $ROOTFS_DIR/usr/sbin/haveged # works
	sudo cp -varf bin_src/openssh-8.1p1/scp $ROOTFS_DIR/usr/sbin/scp
	sudo cp -varf bin_src/openssh-8.1p1/ssh $ROOTFS_DIR/usr/local/bin/
	#sudo cp -varf /usr/bin/scp $ROOTFS_DIR/usr/sbin/scp # testing
	##sudo cp -varf /usr/bin/scp $ROOTFS_DIR/usr/bin/scp # CRASH
	# TODO copy my ssh key so that I can scp from VM
	###################
	###################Put problematic commands here ##############
	###################

	# Copy include & lib
	############
	# TODO - not sure TODO
	# sudo du -sh /sr/lib
	# sudo cp -arfd /usr/lib $ROOTFS_DIR/usr/lib # 2.2G
	sudo du -sh /usr/local/include
	sudo cp -arfd /usr/local/include/* $ROOTFS_DIR/usr/local/include # 2.1M


	##################
	# Net
	######
	sudo mkdir -vp $ROOTFS_DIR/etc/network/
	echo "TODO auto select"
	if [[ $HOST_TYPE == "mir" ]]; then
		# [mir] net
		sed -i "s/address [0-9]*\.[0-9]*\.[0-9]*\.[0-9]*/address ${VM_IP}/g" config_manual/etc/network/interfaces_mir
		cat config_manual/etc/network/interfaces_mir
		sudo cp -varfd config_manual/etc/network/interfaces_mir $ROOTFS_DIR/etc/network/
	elif [[ $HOST_TYPE == "echo" ]]; then
		# [echo net]
		sed -i "s/address [0-9]*\.[0-9]*\.[0-9]*\.[0-9]*/address ${VM_IP}/g" config_manual/etc/network/interfaces
		cat config_manual/etc/network/interfaces
		sudo cp -varfd config_manual/etc/network/interfaces $ROOTFS_DIR/etc/network/
	fi


	#######################################
	echo "Copy library"
	sudo mkdir -p $ROOTFS_DIR/lib
	#sudo cp -arf /lib/i386-linux-gnu/* $ROOTFS_DIR/lib/ # (empty)
	#sudo cp -arf /usr/lib/gcc
	#sudo cp -arf /usr/lib/gcc/x86_64-linux-gnu/4.9/* $ROOTFS_DIR/lib/ # 80M
	echo "Copy include"
	sudo du -sh /usr/include/
	#sudo cp -arfd /usr/include/* $ROOTFS_DIR/include
	sudo cp -arfd /usr/include/* $ROOTFS_DIR/usr/include

	echo -e "\n\n========\nTESTING\n========\n\n"
	sudo mkdir -vp $ROOTFS_DIR/usr/share/bash-completion/
	sudo cp -varf /usr/share/bash-completion/bash_completion $ROOTFS_DIR/usr/share/bash-completion/
	echo
	echo
	echo
	#####################################
	###
	# Python (requred by apt)
	###
	echo "Support Python3.7"
	# local: /usr/bin/python -> python2.7
	#sudo cp -varfd /usr/bin/python $ROOTFS_DIR/usr/bin/
	# local: /usr/local/bin/python3 -> python3.5
	#sudo cp -varfd /usr/local/bin/python3 $ROOTFS_DIR/usr/local/bin/
	sudo cp -arfd /usr/local/lib/python3.7 $ROOTFS_DIR/usr/local/lib/ # verbose
	sudo cp -varfd /usr/local/bin/python3.7 $ROOTFS_DIR/usr/local/bin/
	sudo cp -varfd /usr/local/bin/pip3.7 $ROOTFS_DIR/usr/local/bin/
	# Do it in ramdisk
	#sudo ln -sv $ROOTFS_DIR/usr/local/bin/python3.7 $ROOTFS_DIR/usr/local/bin/python # testing /usr/local/bin/python -> /tmp/rootfs/usr/local/bin/python3.7
	#sudo ln -sv $ROOTFS_DIR/usr/local/bin/python3.7 $ROOTFS_DIR/usr/local/bin/python3 # testing /usr/local/bin/python -> /tmp/rootfs/usr/local/bin/python3.7
	########### TODO argv 2 and move old 2 to 3
#	if [[ $FORCE_COPY_DB_DATA == 1 ]]; then
		echo -e "==============="
		echo -e "\t\t LEMP"
		echo -e "==============="
		echo "LEMP files copy (sync)"
		sudo mkdir -p $ROOTFS_DIR/var/www
		sudo du -sh /var/www/travel_list/ # watchout /var/www/travel_list/storage
		echo "takes so so long..."
		#time sudo cp -arfd /var/www/travel_list/ $ROOTFS_DIR/var/www/

		echo "aka: time sudo cp -arfd /var/www/travel_list/* $ROOTFS_DIR/var/www/travel_list"
		echo "compress"
		time sudo tar -I pigz -cf /tmp/xxx.tar.gz /var/www/travel_list
		echo "extract"
		#sudo mkdir -p ${ROOTFS_DIR}/var/www/travel_list/ # no need since ls tar = var
		#time sudo tar -I pigz -xf /tmp/xxx.tar.gz -C $ROOTFS_DIR/var/www/travel_list
		time sudo tar -I pigz -xf /tmp/xxx.tar.gz -C ${ROOTFS_DIR}/
		echo "/var/www/travel_list/storage/framework/sessions/ is just a log dir for reqs (1req1file)"
		echo "sanity check size:"
		sudo du -sh /var/www/travel_list/storage/framework/sessions/
		ls /var/www/travel_list/storage/framework/sessions/ | wc -l
		sudo du -sh $ROOTFS_DIR/var/www/travel_list/storage/framework/sessions/
		ls $ROOTFS_DIR/var/www/travel_list/storage/framework/sessions/ | wc -l
		sudo du -sh /var/www/travel_list/
		echo "sanity check size:"
		du -sh $ROOTFS_DIR/var/www/
		du -sh /tmp/xxx.tar.gz
		#cp /tmp/xxx.tar.gz /tmp/123dbg.tar.gz # dbg
		sudo rm /tmp/xxx.tar.gz
		echo "remove $ROOTFS_DIR/var/www/travel_list/storage/framework/sessions/*"
		echo -e "\t\t size: `sudo du -sh /var/www/travel_list/storage/framework/sessions/`"
		sudo rm $ROOTFS_DIR/var/www/travel_list/storage/framework/sessions/*
		####time sudo gcp -r /www/travel_list/ $ROOTFS_DIR/var/www/
		####echo "TODO test: time sudo gcp -r /www/travel_list/ /tmp"
		####echo "TODO test: time sudo cp -arfd /www/travel_list/ /tmp"
		####exit -1;
		## var/www/travel_list/routes/web.php (O) main
		## var/www/travel_list/resources/views/travel_list.blade.php (O) submain !!!!!!!!!!!!!1
		## var/www/travel_list/resources/views/travel_list/blade.php (X) misleading
#	fi
	#### support mir
	sudo cp -varfd /usr/lib/sudo $ROOTFS_DIR/usr/lib

	echo
	echo "Done"
fi


##############################################################################
##############################################################################
##############################################################################
##############################################################################
##############################################################################
##############################################################################
##############################################################################
##############################################################################
##############################################################################
##############################################################################
##############################################################################
##############################################################################
##############################################################################
##############################################################################
##############################################################################
##############################################################################
##############################################################################
##############################################################################
##############################################################################
##############################################################################
##############################################################################
##############################################################################
##############################################################################
##############################################################################
## Long running & stable & no need to change line
##############################################################################
##############################################################################
##############################################################################
##############################################################################
##############################################################################
##############################################################################
##############################################################################
##############################################################################
##############################################################################
##############################################################################
##############################################################################
##############################################################################
##############################################################################
##############################################################################
##############################################################################
##############################################################################
##############################################################################
##############################################################################
##############################################################################
##############################################################################
##############################################################################
##############################################################################
##############################################################################
##############################################################################
##############################################################################




###
# apt
###
echo "To support apt...(take a while)"
#sudo cp -arfd /usr/bin/apt $ROOTFS_DIR/bin/
#sudo cp -arfd /usr/bin/apt-get $ROOTFS_DIR/bin/
#sudo cp -arfd /usr/bin/apt-cache $ROOTFS_DIR/bin/
#sudo mkdir -p $ROOTFS_DIR/var/lib/
#sudo mkdir -p $ROOTFS_DIR/var/cache/
#sudo cp -arfd /var/lib/apt/ $ROOTFS_DIR/var/lib/apt/
#sudo cp -arfd /var/lib/dpkg/ $ROOTFS_DIR/var/lib/dpkg/
#sudo cp -arfd /usr/bin/sudo $ROOTFS_DIR/bin/
#sudo cp -arfd /var/cache/apt/ $ROOTFS_DIR/var/cache/apt/ # LARGE
sudo cp -arfd /usr/lib/apt $ROOTFS_DIR/usr/lib # LARGE
sudo cp -arfd /usr/lib/x86_64-linux-gnu/libcurl-gnutls.* $ROOTFS_DIR/usr/lib
sudo cp -arfd /usr/lib/x86_64-linux-gnu/librtmp* $ROOTFS_DIR/usr/lib

sudo cp -varfd /usr/bin/add-apt-repository $ROOTFS_DIR/usr/bin/
sudo cp -varfd /usr/bin/apt-listchanges $ROOTFS_DIR/usr/bin/
# overwrite busybox dpkg
#sudo cp -varfd /usr/bin/dpkg $ROOTFS_DIR/usr/bin/ # testing
sudo cp -varfd /usr/bin/dpkg $ROOTFS_DIR/usr/sbin/ # testing
# thought: trick: busybox boot and replace newer dpkg at runtime if cannot get in $ROOTFS_DIR (NO NEED)

##########################################
echo "rm $ROOTFS_DIR/lib/*.a (no need but good for space)"
sudo rm -v $ROOTFS_DIR/lib/*.a
#echo "strip -v $ROOTFS_DIR/lib/*"
#sudo strip -v $ROOTFS_DIR/lib/*

# manual ok bandedge
# /usr/lib/x86_64-linux-gnu/libstdc++.so.6
#sudo cp -arfd /usr/lib/x86_64-linux-gnu/libgomp.so.1 \
#				/lib/x86_64-linux-gnu/libm.so.6 \
#				/lib/x86_64-linux-gnu/libgcc_s.so.1 \
#				/lib/x86_64-linux-gnu/libpthread.so.0 \
#				$ROOTFS_DIR/lib/
#

######################################################
echo -e "\n\n\n\n\n"
# move src bin to $ROOTFS_DIR
echo; echo "make clean -C src:"; echo
make clean -C src # fix the bug but bug
#make clean -C src/dex_app # fix the bug but bug (at lease do clean /src/dex_app but CANNOT)
echo; echo "make -C src:"; echo
make -C src # generate bins to ../bin
sudo cp -arf bin/* $ROOTFS_DIR/bin
#echo "ls -al $ROOTFS_DIR/ (check what I add)"
#ls -al $ROOTFS_DIR/
echo -e "\n\n\n\n\n"
######################################################


######################################################
echo "Copy config_manual"
echo -e "\nuse [[ POPHYPE ]] *\n"
echo "config - net"
sudo cp -arf config_manual/etc/udev/rules.d/70-persistent-net.rules \
					$ROOTFS_DIR/etc/udev/rules.d/70-persistent-net.rules
# bashrc
echo "config - profile, bashrc"
sudo cp -arfd config_manual/bashrc $ROOTFS_DIR/root/.bashrc # ssh home = /root
sudo cp -arfd config_manual/bashrc $ROOTFS_DIR/.bashrc # tty home (pwd) = / BUT tty doesn't run it (TODO profile or init/rc)
sudo cp -arfd config_manual/profile $ROOTFS_DIR/etc/profile
sudo cp -arfd config_manual/etc/init.d/rc_pophype $ROOTFS_DIR/etc/init.d/rc
echo "htop config"
sudo mkdir -pv $ROOTFS_DIR/.config/htop/
sudo mkdir -pv $ROOTFS_DIR/root/.config/htop/
sudo cp -arfd config_manual/htoprc $ROOTFS_DIR/.config/htop/
sudo cp -arfd config_manual/htoprc $ROOTFS_DIR/root/.config/htop/

#############################
echo "Copy localhost bin"
#############################
########
# gcc
########
if [[ $HOST_TYPE == "mir" ]]; then
    echo "I'm on mir rack"
	echo "[mir] gcc"
	sudo cp -arf /usr/bin/gcc $ROOTFS_DIR/usr/bin
	sudo cp -arf /usr/bin/gcc-6 $ROOTFS_DIR/usr/bin
	sudo cp -arf /usr/bin/x86_64-linux-gnu-gcc $ROOTFS_DIR/usr/bin
	sudo cp -arf /usr/bin/x86_64-linux-gnu-gcc-6 $ROOTFS_DIR/usr/bin
	sudo cp -arf /usr/bin/*6 $ROOTFS_DIR/usr/bin
elif [[ $HOST_TYPE == "echo" ]]; then
	echo "[echo] gcc"
	sudo cp -arf /usr/bin/gcc $ROOTFS_DIR/usr/bin
	sudo cp -arf /usr/bin/gcc-4.9 $ROOTFS_DIR/usr/bin
	sudo cp -arf /usr/bin/x86_64-linux-gnu-gcc $ROOTFS_DIR/usr/bin
	sudo cp -arf /usr/bin/x86_64-linux-gnu-gcc-4.9 $ROOTFS_DIR/usr/bin
	sudo cp -arf /usr/bin/*4.9 $ROOTFS_DIR/usr/bin
fi


echo "Put PuzzleHype's init (init=/init or init=/init_puzzlehype)"
#sudo cp -arf src/bin/init $ROOTFS_DIR # kill me
#sudo cp -arf src/bin/init_puzzlehype $ROOTFS_DIR # /bin/sh # kill me
sudo cp -rfd bin/init $ROOTFS_DIR

##### Auto modify auto_setup_mir.sh for mir / auto_setup.sh for echo 
if [[ $HOST_TYPE == "mir" ]]; then
    echo "- Customize mir rack specific init -"
	sed -i "s/auto_setup_echo.sh/auto_setup_mir.sh/g" src/init_puzzlehype
elif [[ $HOST_TYPE == "echo" ]]; then
    echo "- Customize echo rack specific init -"
	sed -i "s/auto_setup_mir.sh/auto_setup_echo.sh/g" src/init_puzzlehype
fi
#

sudo cp -rfd src/init_puzzlehype $ROOTFS_DIR # /bin/sh
echo "if init_puzzlehype crashes, it's in silence"
file $ROOTFS_DIR/init_puzzlehype
bash -n $ROOTFS_DIR/init_puzzlehype
ret=$?
echo "bash -n init_puzzlehype: $ret"
if [[ $ret != 0 ]]; then
	echo "err: $ret"
	exit -1;
fi


#########
# LEMP
#########
echo "Copy LEMP..."
####
#compose
#
sudo mkdir -p $ROOTFS_DIR/var/run
sudo mkdir -p $ROOTFS_DIR/usr/local/bin
sudo cp -arf /usr/local/bin/composer $ROOTFS_DIR/usr/local/bin
#sudo cp -arf /usr/bin/run-mailcap $ROOTFS_DIR/usr/bin

####
# php
#
sudo cp -arf /usr/bin/php* $ROOTFS_DIR/usr/bin # both /usr/bin/php -> /etc/alternatives/php are somehow copied
sudo mkdir -p $ROOTFS_DIR/usr/lib
sudo cp -arf /usr/lib/php $ROOTFS_DIR/usr/lib
echo "concern I only need php7.2-fpm.sock not php7.2-fpm.pid"
echo "TODO ps aux |grep php  (/var/run is runtime info can not be predefined)"
sudo cp -arfd /var/run/php $ROOTFS_DIR/var/run/  # as a ref
echo "php-fpm: master process (/etc/php/7.2/fpm/php-fpm.conf)"
echo "php-fpm: pool www"
echo "Confirmd sudo php-fpm7.2 says: /run/php/php7.2-fpm.sock alreasy existing"
sudo cp -arfd /usr/sbin/php-fpm7.2 $ROOTFS_DIR/usr/sbin # sbin looks like stores background services
echo "/etc/php/7.2/fpm/php-fpm.conf"
echo "/etc/php/7.2/fpm/pool.d/www.conf"
sudo cp -arfd /etc/php $ROOTFS_DIR/etc/
sudo cp -arf /usr/share/php* $ROOTFS_DIR/usr/share/

####
# mysql
#
# ATTENTION: I was trying to use debootsrap's logger and it makes me cannot enter $ROOTFS_DIR
# service (in background)
sudo cp -arf /usr/sbin/mysqld* $ROOTFS_DIR/usr/sbin  # sbin looks like stores background services
sudo cp -arf /usr/bin/mysqld_safe* $ROOTFS_DIR/usr/bin
sudo mkdir -p $ROOTFS_DIR/usr/share/ # kill me
sudo cp -arf /usr/share/mysql/ $ROOTFS_DIR/usr/share/
echo "IF NEEDED DO MANUALLY:cp /usr/share/mysql/english/errmsg.sys /usr/share/mysql/errmsg.sys"
echo "for doing: sudo /etc/init.d/mysql start"
sudo cp -arf /usr/bin/sudo $ROOTFS_DIR/usr/bin
sudo mkdir -p $ROOTFS_DIR/usr/lib/sudo/
sudo cp -arf /usr/lib/sudo/sudoers.so $ROOTFS_DIR/usr/lib/sudo/
echo "/usr/bin/mysqld_safe needs it"
sudo cp -arf /usr/bin/my_print_defaults $ROOTFS_DIR/usr/bin
echo "hope I can run service as well"
sudo cp -arf /usr/sbin/service $ROOTFS_DIR/usr/sbin/
#sudo cp -arf /bin/systemctl $ROOTFS_DIR/usr/bin/ # no need but testing # already in /bin
echo
echo -e "============================="
echo -e "\t important db files"
echo -e "============================="
#sudo cp -arf share/mysql/ $ROOTFS_DIR/share/
sudo mkdir -p $ROOTFS_DIR/var/lib/ # kill me
sudo cp -arf /var/lib/mysql $ROOTFS_DIR/var/lib/ # when db data is large, this becomes very large as well
sudo du -sh /var/lib/mysql
# 1.4G	/var/lib/mysql/ibdata1 (this is not dadata)
# 1.4G	/var/lib/mysql/travel_list (THIS IS THE ONLY DBDATA!!!)
#			metadata: db.opt	places.frm
#			main data:
#				sudo du -sh /var/lib/mysql/travel_list/places.ibd
echo
sudo cp -arf /usr/bin/mysql* $ROOTFS_DIR/usr/bin
sudo cp -arf /usr/lib/mysql $ROOTFS_DIR/usr/lib
echo "/etc/init.d/mysql needs"
sudo cp -arf /lib/lsb $ROOTFS_DIR/lib
echo "/etc/init.d/mysql start"
echo "var/run/mysqld this is created by mysql service"
#echo "concern I only need mysqld.sock not mysqld.pid"
#echo "/var/run/mysqld/mysqld.sock is created by mysqld"
#sudo mkdir -p $ROOTFS_DIR/var/run/mysqld  #kill me kill me
#sudo cp -arfd /var/run/mysqld $ROOTFS_DIR/var/run/ # as a ref


####
# Debugging tool - curl
#
echo "cpy debugging tool curl..."
sudo cp -arf /usr/bin/curl $ROOTFS_DIR/usr/bin

#########################################################################################################

#####
# Tools and benchmarks
#####
# Copy vanilla backups
# e.g.
#	scripts: exp1_consolidate.sh exp1_consolidate_auto.sh exp2_consolidate_2t.sh 3sleep.sh
#	vanilla binaries: redis-server_vanilla nginx_vanilla memcached_vanilla
#	.bashrc
echo "Copying pre-built binaries/scripts"
sudo cp -arf bin_manual/* $ROOTFS_DIR/bin
bash -n bin_manual/*.sh
ret=$?
if [[ $ret != 0 ]]; then
	echo -e "\n\n\n\n\n\n\n\t(BUG) bash rotten $ret\n\n\n\n"
	exit -1;
fi

#########################
## inputs for benchmarks
#########################
#sudo mkdir -p $ROOTFS_DIR/input
#sudo cp -arf input/* $ROOTFS_DIR/input # 1.1G

# copy bin (define $ROOTFS_DIR/bin)
sudo cp -arf bin_src/bash-5.0/bash $ROOTFS_DIR/bin
sudo cp -arf bin_src/lscpu-1.8/lscpu $ROOTFS_DIR/bin
sudo cp -arf bin_src/numactl-2.0.11/numactl $ROOTFS_DIR/bin
sudo cp -arf bin_src/numactl-2.0.11/numastat $ROOTFS_DIR/bin
sudo cp -arf bin_src/redis-5.0.6/src/redis-server $ROOTFS_DIR/bin
sudo cp -arf bin_src/memcached-1.5.19/memcached $ROOTFS_DIR/bin

###
# ssh related
echo -e "\n\n==========\nssh related\n=========\n\n"
sudo cp -arf bin_src/openssh-8.1p1/sshd $ROOTFS_DIR/bin
#sudo cp -arf bin_src/openssh-8.1p1/scp $ROOTFS_DIR/bin # testing
sudo cp -arf bin_src/openssh-8.1p1/ssh-keygen $ROOTFS_DIR/bin
#sudo cp -arf bin_src/openssh-8.1p1/sshd_config $ROOTFS_DIR/etc/ # config for sshd
echo "leveraging local /etc/ssh/sshd_config"
sudo cp -arf /etc/ssh/sshd_config $ROOTFS_DIR/etc/ # config for sshd
sudo mkdir -p $ROOTFS_DIR/usr/local/etc/ # just in case
#sudo cp -arf bin_src/openssh-8.1p1/sshd_config $ROOTFS_DIR/usr/local/etc/ # app asks...
echo "leveraging local /etc/ssh/sshd_config"
sudo cp -arf /etc/ssh/sshd_config $ROOTFS_DIR/usr/local/etc/ # app asks...
echo "Danger: modify it in VM"
sudo sed -i 's/#PasswordAuthentication yes/PasswordAuthentication yes/g' $ROOTFS_DIR/usr/local/etc/sshd_config
sudo cp -arf ~/.ssh $ROOTFS_DIR/ # copy ssh keys (home = /)
# copy dynamic lib
sudo cp -arf /usr/lib/x86_64-linux-gnu/libcrypto.so.1.0.0 $ROOTFS_DIR/lib/
sudo cp -arfd /usr/lib/x86_64-linux-gnu/libcrypto.so $ROOTFS_DIR/lib/ # testing especially for mir
sudo cp -arfd /usr/lib/x86_64-linux-gnu/libcrypto.so.1.1 $ROOTFS_DIR/lib/ # testing especially for mir
#sudo cp -arf /usr/lib/x86_64-linux-gnu/libcrypto.a # This is static (X)
# for solving "Missing privilege separation directory: /var/empty"
sudo mkdir -p $ROOTFS_DIR/var/empty/sshd/etc
#sudo ln -s $ROOTFS_DIR/etc/localtime $ROOTFS_DIR/var/empty/sshd/etc/localtime # do ln in ramdisk
# Create ssh key
sudo mkdir -p $ROOTFS_DIR/root/.ssh
sudo touch $ROOTFS_DIR/root/.ssh/authorized_keys
sudo chmod 777 $ROOTFS_DIR/root/.ssh/authorized_keys
#sudo cat ~/.ssh/id_rsa.pub >> $ROOTFS_DIR/.ssh/authorized_keys
#sudo cat ~/.ssh/id_rsa.pub >> $ROOTFS_DIR/etc/ssh/authorized_keys
#sudo cat /root/.ssh/id_rsa.pub >> $ROOTFS_DIR/root/.ssh/authorized_keys
sudo cat /root/.ssh/id_rsa.pub >> $ROOTFS_DIR/root/.ssh/authorized_keys # [increment] # for #.
sudo cat ~/.ssh/id_rsa.pub >> $ROOTFS_DIR/root/.ssh/authorized_keys # [increment] for $. =/home/jackchuang
sudo cp -arfd /root/.ssh/id_rsa $ROOTFS_DIR/root/.ssh/
sudo cp -arfd /root/.ssh/config $ROOTFS_DIR/root/.ssh/
sudo chmod 644 $ROOTFS_DIR/root/.ssh/authorized_keys
#sudo chmod 600 $ROOTFS_DIR/root/.ssh/authorized_keys
#sudo chmod 700 -R $ROOTFS_DIR/root/.ssh @ THIS WILL OVERWRITE WHAT U JUST DID BEFORE
sudo chmod 700 $ROOTFS_DIR/root/.ssh
#sudo cat /.ssh/id_rsa.pub >> $ROOTFS_DIR/etc/ssh/authorized_keys
# finally since interactive ssh interface requires pts
sudo mkdir -p $ROOTFS_DIR/dev/pts # just in case
echo "Do this at runtime (in auto_setup.sh)"
echo "mount -t devpts devpts /dev/pts"
# ssh related done
###

##########################
# cpy lib for ....?
##########################
# for dynamically compiled Redis server (relocate)
sudo mkdir -p $ROOTFS_DIR/lib64
# file $ROOTFS_DIR/lib/ld-linux-x86-64.so.2
sudo cp $ROOTFS_DIR/lib/ld-linux-x86-64.so.2 $ROOTFS_DIR/lib64
sudo cp -arfd $ROOTFS_DIR/lib/x86_64-linux-gnu/ld-2.19.so $ROOTFS_DIR/lib64
# [mir] testing
sudo cp -arfd $ROOTFS_DIR/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 $ROOTFS_DIR/lib64
sudo cp -arfd $ROOTFS_DIR/lib/x86_64-linux-gnu/ld-2.24.so $ROOTFS_DIR/lib64

##########################
# [NPB]  copy exp bin
########################
echo "THIS IS A HACK"
sudo cp -varf ./SNU_NPB-1.0.3-pophype/NPB3.3-OMP-C/bin/ep-ao0-cpu0 $ROOTFS_DIR
sudo cp -varf ./SNU_NPB-1.0.3-pophype/NPB3.3-OMP-C/bin/ep-ao0-cpu1 $ROOTFS_DIR
sudo cp -varf ./SNU_NPB-1.0.3-pophype/NPB3.3-OMP-C/bin/ep-ao0-cpu2 $ROOTFS_DIR
sudo cp -varf ./SNU_NPB-1.0.3-pophype/NPB3.3-OMP-C/bin/ep-ao0-1t-argv $ROOTFS_DIR

sudo cp -varf ./SNU_NPB-1.0.3-pophype/NPB3.3-SER-C/bin/*.ser.van $ROOTFS_DIR
# bt.A.x.ser.van
# cg.A.x.ser.van
# ep.A.x.ser.van
# ft.A.x.ser.van
# is.A.x.ser.van
# lu.A.x.ser.van
# mg.A.x.ser.van
# sp.A.x.ser.van
# ua.A.x.ser.van
# cg.B.x.ser.van
# lu.B.x.ser.van
# sp.B.x.ser.van

# vm dsm test microbenchmark
make -C bin_src/pophype/
sudo cp -varf bin_src/pophype/* $ROOTFS_DIR/root
#sudo cp -varf bin_src/pophype/dsm_generate $ROOTFS_DIR/root

# copy net exp bin (Nginx only)
sudo cp -varf bin_src/nginx-1.16.1/objs/nginx $ROOTFS_DIR/bin # add binary
sudo mkdir -p $ROOTFS_DIR/usr/local/nginx/logs
sudo touch $ROOTFS_DIR/usr/local/nginx/logs/error.log # create log
sudo mkdir -p $ROOTFS_DIR/usr/local/nginx/conf
echo "[[ POPHYPE ]] copy customized Nginx configs (not (/cannot) rely on /usr/local/nginx/*)"
sudo cp -arf bin_src/nginx-1.16.1/conf $ROOTFS_DIR/usr/local/nginx # add conf
sudo cp -arf bin_src/nginx-1.16.1/html $ROOTFS_DIR/usr/local/nginx/ # add html
echo "web page: scp ~/c/pophype_make_ramdisk/bin_src/nginx-1.16.1/html/index.html root@10.4.4.22:/usr/local/nginx/html/index.html" # to change html (vanilla soley Nginx, not LEMP) $ ls ~/c/pophype_make_ramdisk/bin_src/nginx-1.16.1/html/
##
echo
echo
echo "[ATTENTION: OVERWRITE] ngnix pophype custmizations (otherwies default)"
echo
echo -e "\t1. Nginx vanilla test\n"
#sudo cp -arfd pophype_nginx/conf/nginx.conf $ROOTFS_DIR/usr/local/nginx/conf # vanilla working
#sudo cp -arfd pophype_nginx/html/index.html $ROOTFS_DIR/usr/local/nginx/html
#echo "ngnix pophype benchmark files (fix it since pophype_nginx corrupted)"
##sudo cp -arf pophype_nginx/html/*file $ROOTFS_DIR/usr/local/nginx/html
echo "others are done by nginx.conf's include -> /etc/nginx/sites-enabled/*"
echo
echo "Net apps copied...(O)"
echo

##############################
# LEMP related files
####
#echo "LEMP files copy (sync)"
## Copy
## LEMP - Nginx & php
#sudo mkdir -p $ROOTFS_DIR/var/www
#sudo du -sh /var/www/travel_list/ # watchout /var/www/travel_list/storage
#echo "takes so so long..."
#time sudo cp -arfd /var/www/travel_list/ $ROOTFS_DIR/var/www/
## var/www/travel_list/routes/web.php (O) main
## var/www/travel_list/resources/views/travel_list.blade.php (O) submain !!!!!!!!!!!!!!
## var/www/travel_list/resources/views/travel_list/blade.php (X) misleading

# LEMP - Nginx
echo -e "\t2. For Nginx LEMP (moved to below)\n"
sudo mkdir -p $ROOTFS_DIR/var/log/nginx/
sudo mkdir -p $ROOTFS_DIR/etc/nginx/
sudo cp bin_src/nginx-1.16.1/conf/mime.types $ROOTFS_DIR/etc/nginx/
sudo cp /etc/nginx/* $ROOTFS_DIR/etc/nginx/
#sudo cp bin_src/nginx-1.16.1/conf/* $ROOTFS_DIR/etc/nginx/
# pophype customized configs
echo -e "\n\n\n==========================="
echo -e "pophype customized configs [START]"
echo -e "===========================\n\n\n"
#sudo cp config_manual/usr/local/nginx/conf/nginx.conf $ROOTFS_DIR/etc/nginx/
#sudo cp config_manual/usr/local/nginx/conf/* $ROOTFS_DIR/etc/nginx/
echo -e "\nuse [[ POPHYPE ]] /usr/local/nginx/conf/nginx.conf\n" # !!!!!!!!!!!!1
sudo cp -varfd config_manual/usr/local/nginx/conf/nginx.conf_lemp $ROOTFS_DIR/usr/local/nginx/conf/nginx.conf # !!!!!!!!!!!!
# Tong's timestamp
sudo mkdir -p $ROOTFS_DIR/home/popcorn/
sudo touch $ROOTFS_DIR/home/popcorn/nginx.log
sudo chmod 777 -R $ROOTFS_DIR/home/popcorn/
# TODO
echo -e "\nuse [[ LOCALHOST ]] /etc/mysql/my.cnf\n"
# /etc/mysql/my.cnf
echo -e "\nuse [[ LOCALHOST ]] /etc/php/7.2/fpm/pool.d/www.conf\n"
# /etc/php/7.2/fpm/pool.d/www.conf
echo -e "\n\n\n==========================="
echo -e "pophype customized configs [DONE]"
echo -e "===========================\n\n\n"
###

#########################################################################################################
#########################################################################################################
#########################################################################################################
# OpenLambda
#sudo cp -varfd ~/open-lambda $ROOTFS_DIR/root/
#sudo du -sh ~/open-lambda
#sudo cp -varfd ~/open-lambda.tar.gz $ROOTFS_DIR/root/
#sudo du -sh ~/open-lambda.tar.gz
#sudo cp -varfd ~/open-lambda2 $ROOTFS_DIR/root/open-lambda
#sudo du -sh ~/open-lambda2
sudo cp -varfd ~/open-lambda3 $ROOTFS_DIR/root/open-lambda
sudo du -sh ~/open-lambda3
###

#########################################################################################################
#########################################################################################################
#########################################################################################################
# Deathstar
# install_docker.sh
sudo cp -varfd bin_src/install_docker.sh $ROOTFS_DIR/bin # testing
#sudo cp -varfd /usr/bin/docker $ROOTFS_DIR/usr/sbin/ # testing (don't just do cp, service is not running)
#sudo cp -varfd /usr/bin/docker $ROOTFS_DIR/usr/bin/ # testing
#sudo cp -varfd /usr/local/bin/docker-compose $ROOTFS_DIR/usr/local/bin/ # do apt install
#sudo cp -varfd /usr/local/bin/docker-machine $ROOTFS_DIR/usr/local/bin/ # do apt install
# more
sudo cp -varfd /usr/bin/git $ROOTFS_DIR/usr/bin/ # testing
sudo cp -arfd bin_src/DeathStarBench $ROOTFS_DIR/root/
# Or git clone https://github.com/delimitrou/DeathStarBench.git in auto_script.sh

# For Deathstar !!!!!!
sudo mkdir -p $ROOTFS_DIR/etc/apt/sources.list.d/
sudo cp -varfd /etc/apt/sources.list.d/docker.list $ROOTFS_DIR/etc/apt/sources.list.d/ # testing
sudo cp -varfd /etc/apt/sources.list.d/virtualbox.list $ROOTFS_DIR/etc/apt/sources.list.d/
sudo mkdir -p $ROOTFS_DIR/lib/modules/
sudo mkdir -p $ROOTFS_DIR/lib/modules/4.4.137-pop-hype+/misc
sudo cp -arfd /lib/modules/4.4.137-pop-hype+ $ROOTFS_DIR/lib/modules/
sudo cp -arfd /lib/modules/4.4.137-pop-hype+/misc/* $ROOTFS_DIR/lib/modules/4.4.137-pop-hype+/misc
#sudo cp -arfd /lib/modules/4.4.137-pop-hype+/kernel $ROOTFS_DIR/lib/modules/4.4.137-pop-hype+/ # modeprobe modules

# dns problem generated by docker
sudo cp -varfd /sbin/resolvconf $ROOTFS_DIR/sbin/
# + /etc/resolvconf/resolv.conf.d/base # done by cp /etc /$ROOTFS_DIR
# This is in /run/* (/etc/resolvconf/run -> /run/resolvconf)

# predownload file
#sudo cp -varfd bin_src/Python-3.5.0.tgz $ROOTFS_DIR/ ######## TODO do I really need it? try to remove it
sudo cp -varfd bin_src/Python-3.7.3.tar.xz $ROOTFS_DIR/

#========= Clean Up ===========
#sudo rm -v $ROOTFS_DIR/usr/bin/python2.7*
#sudo rm -v $ROOTFS_DIR/usr/bin/python3.4* # looks like
sudo rm -vr $ROOTFS_DIR/usr/lib/python2.7
sudo rm -vr $ROOTFS_DIR/usr/share/doc
sudo rm -vr $ROOTFS_DIR/usr/share/virtualbox

# find /usr -name libpython* | wc -l
# 22

################################################
################# OpenLambda Specific
############ overwrite by using x86.img since user names are different. wee need the user :"popcorn"
################################################
	##### TESTING ######
	##########
	# openlambda open-lambda
	####
	# Leverage existing x86.img
#	echo "[ Leverage x86.img ]"
#	DISKIMG_MOUNT_POINT=/tmp/iso
#	sudo mkdir $DISKIMG_MOUNT_POINT
#	sudo mount -t auto -o offset=1048576 /home/jackchuang/tong/t_x86.img $DISKIMG_MOUNT_POINT
#	# Fix popcorn autologin authentic fail problem!!!
#	sudo cp -varp $DISKIMG_MOUNT_POINT/etc/group $ROOTFS_DIR/etc
#	sudo cp -varp $DISKIMG_MOUNT_POINT/etc/passwd $ROOTFS_DIR/etc
#	sudo cp -varp $DISKIMG_MOUNT_POINT/etc/shadow $ROOTFS_DIR/etc
#	sudo cp -varp $DISKIMG_MOUNT_POINT/etc/gshadow $ROOTFS_DIR/etc
#	sudo cp -varp $DISKIMG_MOUNT_POINT/etc/sudoers $ROOTFS_DIR/etc
#	sudo cp -varp $DISKIMG_MOUNT_POINT/etc/init.d $ROOTFS_DIR/etc
#
#	sudo rm -vr $ROOTFS_DIR/etc # ?????????????
#	sudo rm -vr $ROOTFS_DIR/lib/init
#	sudo rm -vr $ROOTFS_DIR/lib/systemd
#	sudo cp -arp $DISKIMG_MOUNT_POINT/etc $ROOTFS_DIR/
#	sudo cp -arp $DISKIMG_MOUNT_POINT/lib/init $ROOTFS_DIR/lib
#	sudo cp -arp $DISKIMG_MOUNT_POINT/lib/systemd $ROOTFS_DIR/lib
#	#sudo cp -varp $DISKIMG_MOUNT_POINT/* $ROOTFS_DIR/
#	sudo sync
#	sudo umount $DISKIMG_MOUNT_POINT
#	# solve popcorn authentic auto login problem
#	#sudo cp -varp config_manual/serial-getty@.service $ROOTFS_DIR/lib/systemd/system/
#	#sudo cp -varp config_manual/getty@tty1.service $ROOTFS_DIR/etc/systemd/system/getty.target.wants/
#	# $ROOTFS_DIR/etc/init.d/rc
#	#####
#	# trim down ramdisk content (not x86.img)
#	sudo rm -vr $ROOTFS_DIR/root/DeathStarBench
#	sudo rm -vr $ROOTFS_DIR/root/dbus-1.12.10
#	sudo rm -vr $ROOTFS_DIR/root/systemd-216
#	sudo rm -vr $ROOTFS_DIR/root/dsm_generate.asm
#	sudo rm -vr $ROOTFS_DIR/*.van
#	sudo rm -vr $ROOTFS_DIR/ep-*
#	sudo rm -vr $ROOTFS_DIR/Python-3.7.3.tar.xz
#	sudo rm -vr $ROOTFS_DIR/var/www
#	sudo rm -vr $ROOTFS_DIR/var/lib/mysql
#	sudo rm -vr $ROOTFS_DIR/usr/lib/php
#	sudo rm -vr $ROOTFS_DIR/usr/lib/mysql
#	sudo rm -vr $ROOTFS_DIR/usr/local/nginx
#	sudo rm -vr $ROOTFS_DIR/usr/local/lib/python2.7/
#
#	# last make sure ramdisk takes our etc/init.d config
#	sudo cp -arp /dev/ttyS* $ROOTFS_DIR/dev/ # FAIL - dynamically createod
#	##sudo rm -vr $ROOTFS_DIR/etc/init.d
#	#sudo rm -vr $ROOTFS_DIR/lib/systemd/
#	#sudo cp -arp config_manual/init.d/ $ROOTFS_DIR/etc/
#	#sudo cp -arp config_manual/systemd/ $ROOTFS_DIR/lib/
	###
	#END OpenLambda


#========= last setup for mir ===========
echo -e "\n\n= last setup for mir =\n\n"
sudo cp -varp config_manual/hosts $ROOTFS_DIR/etc
sudo cp -varp config_manual/hostname $ROOTFS_DIR/etc

#==============================
#==============================
#==============================
#==============================
#==============================
#==============================
#==============================
echo -e "\n\n=========="
echo "$ROOTFS_DIR created"
sudo du -sh  $ROOTFS_DIR
echo -e "==========\n\n"
#==============================
#==============================
#==============================
#==============================
#==============================
#==============================


PREBUILT_DEBOOTSTRAP=prebuilt-debootstrap
DIR=/tmp/tmpfs # real rootfs for the guest and will be killed
IMG=/tmp/ramdisk # IMG/ramdisk/rd
echo "dd $RAMDISK_SIZE_MB MB $IMG (take a while)"
sudo dd if=/dev/zero of=$IMG bs=1M count=$RAMDISK_SIZE_MB # !!!
echo "mkfs.ext4 -F $IMG"
sudo mkfs.ext4 -F $IMG
echo "mkdir $DIR"
sudo mkdir -p $DIR
sudo mount -t ext4 $IMG $DIR -o loop
#############
echo "1. ramdisk: debootstrap $DIR (install Debian base system)"
# Using this line and not using cp $ROOTFS_DIR/* $DIR can use apt command

# Smart and auto do debootstrap
echo "Check if $PREBUILT_DEBOOTSTRAP exists: fast-/slow- path"
if [[ $FORCE_CREATE_DEBOOTSTAP != 1 && -d $PREBUILT_DEBOOTSTRAP ]]; then
	echo "$PREBUILT_DEBOOTSTRAP: Exist - fast path (cache)"
else
	echo "$PREBUILT_DEBOOTSTRAP: Not found or force to create"
	sudo debootstrap --arch amd64 jessie $PREBUILT_DEBOOTSTRAP #; # sudo cp -arf bin/init_puzzlehype $DIR # /bin/sh
	echo -e "===\ndebootstrap cache created $PREBUILT_DEBOOTSTRAP\n===\n\n"
fi

echo -e "\n\nLeverage debootstrap (always use cache)\n\n"
##########
# openlambda open-lambda
####
##	sudo rm -vr $PREBUILT_DEBOOTSTRAP/etc/init.d
##	sudo rm -vr $PREBUILT_DEBOOTSTRAP/lib/systemd
#	sudo rm -vr $PREBUILT_DEBOOTSTRAP/etc/fstab
#	sudo rm -vr $PREBUILT_DEBOOTSTRAP/share
# done
#
time sudo cp -arfd $PREBUILT_DEBOOTSTRAP/* $DIR
sudo cp -arfd $PREBUILT_DEBOOTSTRAP/dev/* $DIR/dev/


# sudo debootstrap --arch amd64 jessie /tmp/tmp1 http://ftp.us.debian.org/debian/ # from web
echo "2. ramdisk: copying everything we just prepared in the top half of this script..."
sudo du -sh $ROOTFS_DIR
df -h
echo -e "\n\n\n"
echo -e "sudo cp -arfd $ROOTFS_DIR/* $DIR\n===================\n"
sudo cp -arfd $ROOTFS_DIR/* $DIR # use $ROOTFS_DIR to overwrite
echo -e "====================\n\n\n"
echo "cpy tmp_rootfs to disk_file done"

echo "pigz will store all the location info. To use it, I need to fix that first! tar => ls = tmp ...."
##echo "multithreaded cp (aka tar+untar) $DIR"
#echo "aka sudo cp -arfd $ROOTFS_DIR/* $DIR"
#echo "compress"
#time sudo tar -I pigz -cf /tmp/package.tar.gz $ROOTFS_DIR
##time tar -I pigz -cf /tmp/package.tar.gz /path/to/file_or_folder
#echo "extract"
#time sudo tar -I pigz -xf /tmp/package.tar.gz -C $DIR #[-C  /path/to/extracted]
#du -sh $ROOTFS_DIR
#du -sh /tmp/package.tar.gz
#du -sh $DIR
#cp /tmp/package.tar.gz /tmp/456dbg.tar.gz # dbg
#sudo rm /tmp/package.tar.gz

#echo "sleep 30s to check"
#sleep 30








##################################################
##################################################
####### Creating ramdisk_dbg for debugging #######
##################################################
##################################################
if [[ $FORCE_CREATE_DEBUG_ROOTFS == 1 ]]; then
	#echo "copy rootfs source for debugging \"$ROOTFS_DIR/* => $DEBUG_ROOTFS_DIR/\" (runtime)"
	echo "copy rootfs source for debugging \"$ROOTFS_DIR/* => $DEBUG_ROOTFS_DIR/\" (background)"
	sudo rm -r $DEBUG_ROOTFS_DIR # this takw
	sudo mkdir -p $DEBUG_ROOTFS_DIR
	#time sudo cp -arfd $ROOTFS_DIR/* $DEBUG_ROOTFS_DIR/ > /dev/null # (runtime)
	sudo cp -arfd $ROOTFS_DIR/* $DEBUG_ROOTFS_DIR/ > /dev/null & # (background) still copy to share for debugging in the background

	# takes time but real rootfs
	echo "[NOTE] cannot copy after unmount - [symptom] ramdisk.gz size very small and \$DIR $DIR is busy (cannot unmount, exist but empty)"
	echo "copy rootfs source for debugging \"$DIR/* => $DEBUG_ROOTFS_DIR_DIR/\" (runtime)"
	#echo "copy rootfs source for debugging \"$DIR/* => $DEBUG_ROOTFS_DIR_DIR/\" (background"
	sudo rm -r $DEBUG_ROOTFS_DIR_DIR # this takw
	sudo mkdir -p $DEBUG_ROOTFS_DIR_DIR
	time sudo cp -arfd $DIR/* $DEBUG_ROOTFS_DIR_DIR/ > /dev/null # (runtime)
	#sudo cp -arfd $DIR/* $DEBUG_ROOTFS_DIR_DIR/ > /dev/null & # (background) still copy to share for debugging in the background
else
	echo "skip generate rootfs_dbg $DEBUG_ROOTFS_DIR for debugging"

	echo "skip generate rootfs_dbg $DEBUG_ROOTFS_DIR_DIR for debugging"
fi



### Force to use prebuilt_debootstrap (RISKY PROBLEMATIC POINT)
#echo "Force to use $PREBUILT_DEBOOTSTRAP instead of busybox (overwrite)"
#sudo cp -rfd $PREBUILT_DEBOOTSTRAP/usr/bin/logger $DIR/usr/bin/logger
###
echo "\$DIR $DIR looks no use !!!!!!!!!!!!!! 200514"
sudo du -sh $DIR
sudo umount $DIR
sudo rm -r $DIR
echo
echo
#############

################################################################################################
################################################################################################
################################################################################################
################################################################################################
################################################################################################
################################################################################################
###################################### RAMDISK CREATE ##########################################
################################################################################################
################################################################################################
################################################################################################
################################################################################################
################################################################################################
################################################################################################

#
sudo file $IMG
sudo du -sh $IMG
echo "gzip to ~/c/ramdisk.gz (take a while)"
################################################
## slow but stable
#sudo time gzip --best -c $IMG > ~/c/ramdisk.gz
##sudo time gzip -c $IMG > ~/c/ramdisk.gz
################ or ################
# fast but testing
nrcpus=`lscpu | grep "^CPU(s):" | awk '{print $2}'`
echo "pigz --best -p $nrcpus -c $IMG > ~/c/ramdisk.gz (take a while)"
rm ~/c/ramdisk.gz
time pigz --best -p $nrcpus -c $IMG > ~/c/ramdisk.gz
ls -alh ~/c/ramdisk.gz
du -sh ~/c/ramdisk.gz
sudo rm $IMG

#######killme
#file ramdisk
#
#echo "gzip...(take a while)"
#sudo gzip --best -c ramdisk > ramdisk.gz
#echo "cp ramdisk.gz ~/c"
#cp ramdisk.gz ~/c
#
#######killme

#echo "ls -al $ROOTFS_DIR/ (check what I add)"
#ls -al $ROOTFS_DIR/

echo
clean_cache
echo
# Time: end
elapsed_time=$( date +%s.%N --date="$start_time seconds ago" )
echo "$0 elapsed_time: $elapsed_time"
# echo "cat $0 to see what should you check before you launch"
################
# check rootfs_dbg # takes time to sync up
# <LEMP>
# sudo cp -varfd config_manual/usr/local/nginx/conf/nginx.conf_lemp $ROOTFS_DIR/usr/local/nginx/conf/nginx.conf # !!!!!!!!!!!!
# php code:
# /var/www/travel_list/routes/web.php
#
#
#
#





