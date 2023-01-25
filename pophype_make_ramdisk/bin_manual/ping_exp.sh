#! /bin/bash
#
# ping_exp.sh
# Copyright (C) 2020 jackchuang <jackchuang@echo5>
#
# Distributed under terms of the MIT license.
#
set -u
CPU="0x1 0x2 0x4 0x8"
SERVER="iperf.volia.net"
#ping_iter=10
ping_iter=1000
bak_SERVER="
bouygues.iperf.fr
ping.online.net
ping6.online.net
ping-90ms.online.net
ping6-90ms.online.net

iperf.eenet.ee
iperf.volia.net

iperf.it-north.net
iperf.biznetnetworks.com

iperf.scottlinux.com
iperf.he.net
"

echo ""
echo "=== test on ==="
echo "CPU: $CPU"
echo "SERVER: $SERVER"
echo "" 
echo "=== info ==="
echo "Public iPerf3 servers"
echo "https://iperf.fr/iperf-servers.php"
echo

#usr_input_name="$1"
#_log_name_prefix="ab" # just in case
#log_name_prefix=${_log_name_prefix}_${usr_input_name}
_project=pophype_micronet
project=`pwd`/${_project}

for server in $SERVER
do
	for cpu in $CPU
	do
		# 0
		rm output
		TSTAMP=`date +%Y%m%d_%T | sed 's/:/_/g'`
		#_project_sub_folder=${project}/${_project}_${TSTAMP}_${log_name_prefix}_${server}_${cpu}
		_project_sub_folder=${project}/${_project}_${TSTAMP}_${server}_${cpu}
		echo -e "out: ${_project_sub_folder}\n"
		mkdir -p ${_project_sub_folder}
		

		# 1
		echo -e "\n======" | tee -a output
		echo "cpu: $cpu server: $server" | tee -a output
		echo "taskset $cpu ping -c $ping_iter $server" | tee -a output
		echo -e "=======\n" | tee -a output
		taskset $cpu ping -c $ping_iter $server | tee -a output
		#taskset $cpu ping -c $ping_iter $server -A | tee -a output # cannot too fast and thus 0
		echo "sleep 5..."; sleep 5

		echo -e "\n\n\n\n\n===============\n\n\n\n" | tee -a output

		# 2
#		echo -e "\n======" | tee -a output
#		echo "cpu: $cpu server: $server" | tee -a output
#		echo "taskset $cpu iperf3 -c $server" | tee -a output
#		echo -e "=======\n" | tee -a output
#		taskset $cpu iperf3 -c $server | tee -a output
#		echo "sleep 5..."; sleep 5


		#TODO
		# avg py | tee -a output
		# awk '{sum+=$2}END{printf "Sum=%d\nCount=%d\nAve=%.2f\n",sum,NR,sum/NR}' ave.txt

		# copy
		cp output ${_project_sub_folder}
		echo -e "\nout: $_project_sub_folder"
		echo "sleep 5..."; sleep 5
	done
done
