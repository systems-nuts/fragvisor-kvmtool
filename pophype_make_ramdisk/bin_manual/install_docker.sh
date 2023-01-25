#!/bin/bash
# Ref: https://gist.github.com/asimzeeshan/4f9891b2c9630678267fbe1b0556f9ea
# Author: horenc@vt.edu
# Jack: this is for VM. Manually run it on host!!!!!

#####
# Pre-required virtualbox
###
# Ref: https://www.virtualbox.org/wiki/Linux_Downloads
wget -q https://www.virtualbox.org/download/oracle_vbox_2016.asc -O- | sudo apt-key add -
wget -q https://www.virtualbox.org/download/oracle_vbox.asc -O- | sudo apt-key add -
sudo apt-get update
sudo apt-get install virtualbox-5.0 -o Dpkg::Options::="--force-confdef" -y
#sudo apt-get install virtualbox-6.0 -o Dpkg::Options::="--force-confdef" -y
#sudo apt-get install virtualbox-6.0 -y

modprobe vboxdrv
modprobe vboxpci
modprobe vboxnetflt
modprobe vboxnetadp

############################################################
# Get the pre-requisite packages installed
############################################################
#apt install -y htop iotop iftop
#apt install -y curl apt-transport-https
apt install htop iotop iftop -o Dpkg::Options::="--force-confdef" -y
apt install curl apt-transport-https -o Dpkg::Options::="--force-confdef" -y

echo "uname -s `uname -s`" # Linux
echo "uname -m `uname -m`" # x86_64

# Install docker
curl -kfsSL https://download.docker.com/linux/debian/gpg | sudo apt-key add -
#echo "deb [arch=amd64] https://download.docker.com/linux/debian jessie stable" > /etc/apt/sources.list.d/docker.list # now
#echo "deb [arch=x86_64] https://download.docker.com/linux/debian jessie stable" > /etc/apt/sources.list.d/docker.list
##cat > /etc/apt/sources.list.d/docker.list <<END \
##deb [arch=x86_64] https://download.docker.com/linux/debian jessie stable
##END
##deb [arch=amd64] https://download.docker.com/linux/debian jessie stable
#apt update && apt install -y docker-ce
apt update && apt install docker-ce -o Dpkg::Options::="--force-confdef" -y

# Install docker-compose from github releases
curl -kL https://github.com/docker/compose/releases/download/1.23.2/docker-compose-`uname -s`-`uname -m` -o /usr/local/bin/docker-compose
chmod +x /usr/local/bin/docker-compose
# Create a symblink for applications going somewhere else
ln -s /usr/local/bin/docker-compose /usr/bin/docker-compose
ls -lha /usr/bin/docker-compose

# get the versions information
docker --version && docker-compose --version


# machine
curl -kL https://github.com/docker/machine/releases/download/v0.16.0/docker-machine-`uname -s`-`uname -m` >/tmp/docker-machine
chmod +x /tmp/docker-machine
sudo cp /tmp/docker-machine /usr/local/bin/docker-machine

