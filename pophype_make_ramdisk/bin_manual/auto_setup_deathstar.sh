#! /bin/bash
#
# auto_setup.sh
# Copyright (C) 2019 jackchuang <jackchuang@echo>
#
# Distributed under terms of the MIT license.
#
#
# Main ref (Ubuntu): https://www.digitalocean.com/community/tutorials/how-to-install-and-configure-laravel-with-lemp-on-ubuntu-18-04#step-2-%E2%80%94-creating-a-database-for-the-application
# with ref (Debian9 my host): https://linux4one.com/how-to-install-laravel-php-framework-with-nginx-on-debian-9/
#
echo -e "\n\n\n======\n$0 auto setup deathstar\n=======\n\n\n\n"
echo
echo
apt-get install libssl-dev libz-dev -o Dpkg::Options::="--force-confdef" -y
apt-get install luarocks -o Dpkg::Options::="--force-confdef" -y
luarocks install luasocket

install_docker.sh

##########
##########
##########
#jessie to stretch 
##########
##########
##########
# TODO clean python stuff......why they are on the system
# sed -i 's/jessie/stretch/g' /etc/apt/sources.list
# apt update
# apt upgrade -o Dpkg::Options::="--force-confdef" -y
# apt-get -f install
# apt install python3-pip -f -o Dpkg::Options::="--force-confdef" -y

########################################################################



#########
# Python3 needed by deathstart (copied prebuilt bins&libs)
#########
echo "Python3.7 copied prebuilt bins & libs"
#apt install -y python
#apt install -y python3 -o Dpkg::Options::="--force-confdef" #
#rm -v /var/lib/dpkg/info/python2.7-minimal.postinst # rotten from host
sudo apt-get install libssl-dev openssl -o Dpkg::Options::="--force-confdef" -y
#wget https://www.python.org/ftp/python/3.5.0/Python-3.5.0.tgz # predownloaded
#tar xzf Python-3.5.0.tgz
#cd Python-3.5.0
##
## - compile
##
##./configure
##make
##sudo make install
##
#rm -rv Python-3.5.0.tgz Python-3.5.0

##
## Python-3.7.3
##
## https://linuxize.com/post/how-to-install-python-3-7-on-debian-9/
sudo apt install build-essential zlib1g-dev libncurses5-dev libgdbm-dev libnss3-dev libssl-dev libreadline-dev libffi-dev wget -o Dpkg::Options::="--force-confdef" -y
#
## curl -O https://www.python.org/ftp/python/3.7.3/Python-3.7.3.tar.xz
#tar xf Python-3.7.3.tar.xz
##
## - compile
##
#rm /usr/bin/python3
##cd Python-3.7.3
#./configure --enable-optimizations
##./configure --enable-optimizations --without-tests
##./configure --enable-optimizations --disable-tests
##./configure --enable-optimizations --skip-tests
### --disable-tests, --skip-tests
#make build_all # -j32
## TODO skip this test.....
## TODO skip this test.....
## TODO skip this test.....
#sudo make altinstall
#python3.7 --version
#
#ln -s /usr/local/bin/python3.7 /usr/bin/python3
#
## apt install python3-pip -o Dpkg::Options::="--force-confdef" -y
##apt install python3-pip -o Dpkg::Options::="--force-confdef" -y # install
##apt install python-pip -o Dpkg::Options::="--force-confdef" -y # TODO try

#pip3 install --upgrade pip3==3.5 # upgrade to3.5
#pip install aiohttp # TODO try
pip3.7 install aiohttp
pip3.7 install asyncio
echo
echo
echo
echo
# Problem: 0 ssl:default [Connect call failed (‘10.68.39.88’, 8080)]
echo "cd /root/DeathStarBench/socialNetwork"
cd /root/DeathStarBench/socialNetwork
echo "sed -i 's/10.68.39.88:8080/10.4.4.222:8082/g' ./scripts/init_social_graph.py"
sed -i 's/10.68.39.88:8080/10.4.4.222:8082/g' ./scripts/init_social_graph.py
echo
echo -e "Running docker-compose up (\e[25mtake a while...\e[39m)"
while [[ `docker-compose up -d 2>&1 | wc -l` == 30 ]]; do
	sleep 1
done
echo
echo -e "docker-compose...\e[32m(O)\e[39m"
echo
echo -e "python3 scripts/init_social_graph.py (\e[25mtake a while...\e[39m)"
python3 scripts/init_social_graph.py
echo
echo
echo -e "[Deathstar] \e[32mReady to run ./wrk on host/clients !\e[39m"
echo
