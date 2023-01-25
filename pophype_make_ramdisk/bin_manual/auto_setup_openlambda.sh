#! /bin/bash
#
# auto_setup.sh
# Copyright (C) 2019 jackchuang <jackchuang@echo>
#
# Distributed under terms of the MIT license.
#
#
#
echo -e "\n\n\n======\n| $0 auto setup start |\n=======\n\n\n\n"
echo
echo
echo
echo "sudo apt-get update"
sudo apt-get update
echo "sudo apt-get install --reinstall build-essential -o Dpkg::Options::="--force-confdef" -y"
sudo apt-get install --reinstall build-essential -o Dpkg::Options::="--force-confdef" -y
echo "sudo apt-get install pkg-config  -o Dpkg::Options::="--force-confdef" -y"
sudo apt-get install pkg-config  -o Dpkg::Options::="--force-confdef" -y
echo "cd ~/dbus-1.12.10"
cd ~/dbus-1.12.10
echo "
./configure --prefix=/usr                       \
            --sysconfdir=/etc                   \
            --localstatedir=/var                \
            --disable-static                    \
            --disable-doxygen-docs              \
            --disable-xml-docs                  \
            --docdir=/usr/share/doc/dbus-1.12.10 \
            --with-console-auth-dir=/run/console"
./configure --prefix=/usr                       \
            --sysconfdir=/etc                   \
            --localstatedir=/var                \
            --disable-static                    \
            --disable-doxygen-docs              \
            --disable-xml-docs                  \
            --docdir=/usr/share/doc/dbus-1.12.10 \
            --with-console-auth-dir=/run/console \
			--enable-systemd
			#--enable-user-session
#echo make
#make
#echo make install
#make install

##### jack
# systemd ./configure
# 
### problem
# configure: error: Your intltool is too old.  You need intltool 0.40.0 or later.
# apt install intltool
### problem
# configure: error: *** gperf not found
# apt install gperf
### problem
#configure: error: *** POSIX caps headers not found
# apt-get install libcap-dev 
### problem
# configure: error: Package requirements (glib-2.0 >= 2.22.0 gobject-2.0 >= 2.22.0 gio-2.0) were not met:
#	No package 'glib-2.0' found
#	No package 'gobject-2.0' found
#	No package 'gio-2.0' found
#
# apt install libgtk-3-dev
#


###### Tong's solution
# apt install intltool libgtk-3-dev libgtop2-dev librsvg2-dev
#


echo
echo -e "$0 Done!!\n\n\n\n"

