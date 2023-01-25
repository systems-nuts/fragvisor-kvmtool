#! /bin/bash
#
# auto_setup.sh
# Copyright (C) 2019 jackchuang <jackchuang@echo>
#
# Distributed under terms of the MIT license.
#
echo -e "\n\n\n\n===================== apt install ==========================\n\n\n\n"
apt update
apt upgrade -o Dpkg::Options::="--force-confdef" -y
######################

# For solving main problems
apt install apt-listchanges -y
rm -v /var/lib/dpkg/info/python2.7-minimal.postinst # rotten from host

# For fixing future problems
apt install openssl -y
apt install ca-certificates -y

# Less problem
apt install -y vim
apt install -y software-properties-common

