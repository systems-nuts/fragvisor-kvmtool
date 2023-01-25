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
echo -e "\n\n\n======\n$0 auto setup LEMP start\n=======\n\n\n\n"
echo -e "\nTODO taskset to all\n"
echo -e "\nTODO taskset to all\n\n"
echo
echo "For Tong's timestamp"
# Tong's timestamp
#sudo mkdir -p rootfs/home/popcorn/
#sudo touch rootfs/home/popcorn/nginx.log
#sudo chmod 777 -R rootfs/home/popcorn/
echo
echo -e "======\nLEMP - nginx\n======="
echo
nginx -t # test conf files
ret=$?
if [[ $ret != 0 ]]; then
	echo "Nginx conf error!"; exit -1;
fi
###
#taskset 0x1 nginx
nginx
#nginx -c <conf>
###
echo "TODO - printf process ID"
echo "Use access.log to ebug"
echo -e "\tcat /var/log/nginx/access.log\n"
echo "Use err.log to debug"
echo -e "\tcat /var/log/nginx/error.log\n"
echo
echo -e "======\nLEMP - mysql\n======="
echo
#####
# service mysql restart
#####
echo
echo
echo "sudo mysqld_safe"
###
#taskset 0x1 mysqld_safe&
mysqld_safe&
echo
echo "mysqld_safe creates /var/run/mysqld/mysqld.sock and mysqld"
echo
# creates /var/run/mysqld/mysqld.sock
###
#echo "sudo mysqld --basedir=/usr --datadir=/var/lib/mysql --plugin-dir=/usr/lib/mysql/plugin --user=mysql --skip-log-error --pid-file=/var/run/mysqld/mysqld.pid --socket=/var/run/mysqld/mysqld.sock --port=3306 --thread-pool-size=4&"
###
#mysqld --basedir=/usr --datadir=/var/lib/mysql --plugin-dir=/usr/lib/mysql/plugin --user=mysql --skip-log-error --pid-file=/var/run/mysqld/mysqld.pid --socket=/var/run/mysqld/mysqld.sock --port=3306 --thread-pool-size=4&
#taskset 0x2 mysqld --basedir=/usr --datadir=/var/lib/mysql --plugin-dir=/usr/lib/mysql/plugin --user=mysql --skip-log-error --pid-file=/var/run/mysqld/mysqld.pid --socket=/var/run/mysqld/mysqld.sock --port=3306 --thread-pool-size=4&
###
# default argv
# --thread-handling=one-thread-per-connection
# --thread-pool-size=20
#### opt argv
# --thread-handling=pool-of-threads
#########
echo "(TODO) sudo /etc/init.d/mysql restart"
echo "TODO try to use service"
echo
echo -e "======\nLEMP - php\n======="
echo
echo "Problem: curl http://192.168.33.2 gives me a page saying: Cannot open source device ?"
echo "Why: Most likely PHP can't access to /dev/random. Check"
echo "Solv:"
echo -e "\tphp-fpm will use /dev/(u)radom (created at runtime)\n"
echo "chmod 666 /dev/urandom"
chmod 666 /dev/urandom
ls -l /dev/urandom
echo "chmod 666 /dev/random"
chmod 666 /dev/random
ls -l /dev/random
#root@(none):~# lsof |grep random
#87	/usr/bin/php7.2	/dev/urandom
#87	/usr/bin/php7.2	/dev/random
#250	/usr/sbin/php-fpm7.2	/dev/urandom
#250	/usr/sbin/php-fpm7.2	/dev/random
echo
echo
#start_time=$( date +%s.%N )
echo "sleep 5 (in case (!sure hack) for /run/php/php7.2-fpm.sock...)"
sleep 5
echo
echo "Manually increase/cheat the entropy for /dev/(u)random"
echo
echo "trying increase entropy...(take a while)"
echo
#magic
echo "/usr/sbin/haveged --Foreground --verbose=1 --write=1024&"
haveged --Foreground --verbose=1 --write=1024&
sleep 2 # give it time to reflect
##########
## Solved by haveged (so removing, testing)
#####
##auto
#b=0
#cnt=0
## keep manual command just in case....
## 2000 ~ 3000 works 1000 !work
## for kk in `seq 10000`; do cat /proc/sys/kernel/random/entropy_avail; done
#while [[ $b -lt 60 ]]; do
#	for iter in `seq 4`; do
#		a=$( cat /proc/sys/kernel/random/entropy_avail )
#	done
#	#echo $a
#	b=`echo $a | bc`
#	# TODO - time out skip
#	let "cnt+=1"
#	if let "$cnt%6000 == 0"; then
#		echo "$cnt times => write to /dev/(u)random, hope it helps"
#		for _k in `seq 1`; do
#			cat /bin/* > /dev/random 2> /dev/null
#			cat /var/* > /dev/random 2> /dev/null
#			cat /usr/* > /dev/random 2> /dev/null
#			cat /bin/* > /dev/urandom 2> /dev/null
#			cat /var/* > /dev/urandom 2> /dev/null
#			cat /usr/* > /dev/urandom 2> /dev/null
#		done
#	fi
#	while [[ $cnt -gt 600000 ]]; do
#		echo "Timeout: cannot fix entropy"
#		exit -1;
#	done
#done # 1*2000=40000

mkdir -p /run/php
echo "hack: chmod 777 /run/php"
chmod 777 /run/php # not helpful
chown www-data:www-data /run/php # not helpful
start=`date +%s`
# if a sock file
while [[ ! -S /run/php/php7.2-fpm.sock && ! -d /run/php/php7.2-fpm.sock ]]; do
	echo "killall php7.2 php-fpm7.2"
	killall php7.2 php-fpm7.2
	echo "sleep 2"; sleep 2
	echo "php7.2&" # debug
	###
	#service php7.2-fpm restart
	###
	echo "no taskset!!!!!!!!!!!!!!!!!!!!!!!"
	echo "no taskset!!!!!!!!!!!!!!!!!!!!!!!"
	echo "no taskset!!!!!!!!!!!!!!!!!!!!!!!"
	#taskset 0x1 php7.2 -e&
	php7.2 -e&
	###
	echo "sleep 3s make sure php7.2 is ready and running"
	echo "TODO - how to run php7.2 as demon (also nginx)"
	echo "WARRNING - this may take super long since printk, PLZ MANUALLY DO \"php-fpm7.2\" if ! -f /run/php/php7.2-fpm.sock"
	sleep 3
	echo "php-fpm7.2 (will create /run/php/php7.2-fpm.sock)"
	###
	#taskset 0x1 php-fpm7.2&
	php-fpm7.2&
	###
	echo "sleep 2"; sleep 2
	file /run/php/php7.2-fpm.sock
	echo "cat /proc/sys/kernel/random/entropy_avail"
	cat /proc/sys/kernel/random/entropy_avail
	echo "Wait sleep 1..."; sleep 1
	ls -al /run/php/php7.2-fpm.sock
	echo "if last one is this => BAD"
	#echo "if ls works and still kill -> increase sleep 2 time"
	#echo "killall php7.2 php-fpm7.2"
done
end=`date +%s`
runtime=$((end-start))
#elapsed_time=$( date +%s.%N --date="$start_time seconds ago" )
echo
echo "php SUCESSFUL elapsed_time: $runtime s" # vm time is slower
echo
echo "Use laravel access.log to ebug"
echo -e "\tcat /var/log/nginx/laravel-access.log\n"
echo "Use laravel err.log to debug"
echo -e "\tcat /var/log/nginx/laravel-error.log\n"
echo
echo "==============="
echo "Check points"
echo "==============="
# nginx (Lemp version)
echo "cat /usr/local/nginx/conf/nginx.conf"
file /usr/local/nginx/conf/nginx.conf
# php
echo "cat /etc/nginx/sites-available/travel_list"
file /etc/nginx/sites-available/travel_list
echo "cat /etc/php/7.2/fpm/php-fpm.conf"
file /etc/php/7.2/fpm/php-fpm.conf # process.max =
echo "cat /etc/php/7.2/fpm/php.ini"
file /etc/php/7.2/fpm/php.ini
echo "cat /etc/php/7.2/cli/php.ini"
file /etc/php/7.2/cli/php.ini
echo
echo "========================="
echo "==== PERFORMANCE ===="
echo "========================="
echo "php"
# https://serversforhackers.com/c/php-fpm-process-management
echo "cat /etc/php/7.2/fpm/pool.d/www.conf"
echo "cat /etc/php/7.2/fpm/pool.d/www.conf |grep "pm\.""
echo -e "pm = dynamic \tor\t pm = static"
echo "pm.max_children = 5"
cat /etc/php/7.2/fpm/pool.d/www.conf |grep "pm\."
echo "nginx"
echo "cat /usr/local/nginx/conf/nginx.conf |grep worker"
cat /usr/local/nginx/conf/nginx.conf |grep worker
echo "= how nginx php works ="
echo -e "\t\t- php code-"
echo -e "\tcat /var/www/travel_list/resources/views/travel_list.blade.php" # php code
echo "how mysql (db) works"
echo -e "\t\t- db code-"
echo -e "\tcat /var/www/travel_list/routes/web.php" # db code
echo "= db data generate ="
echo "https://www.generatedata.com"
echo "check ./tools/*.sql"
echo "./generate_ramdisk.sh:sudo cp -arfd /var/www/travel_list/ rootfs/var/www/"
echo "search travel_list in ./generate_ramdisk.sh"
echo "(my auto fill data is in my own doc)"
echo
echo "config files"
echo "PHP-FPM (FastCGI Process Manager)"
echo
echo -e "nginx - /usr/local/nginx/conf/nginx.conf"
echo -e "\t\t - (NOT /etc/nginx/nginx.conf"
echo -e "\t\t - (from \"sudo cp -varfd config_manual/usr/local/nginx/conf/nginx.conf_lemp rootfs/usr/local/nginx/conf/nginx.conf\")"
echo -e "\t\t - include /etc/nginx/mime.types;"
echo -e "\t\t - include /etc/nginx/conf.d/*.conf"
echo -e "\t\t - include /etc/nginx/sites-enabled/*;"
echo -e "ll /etc/nginx/sites-enabled/travel_list -> /etc/nginx/sites-available/travel_list =>"
echo -e "nginx -> /etc/nginx/sites-available/travel_list"
echo -e "\t\t - root /var/www/travel_list/public; =>"
echo -e "php - /var/www/travel_list/public/index.php"
echo -e "\t\t - fastcgi_index index.php;"
echo -e "php conf - /etc/php/7.2/fpm/php.ini (master will read this and init env)"
echo -e "\t\t php-master will dispatch work to worker threads"
echo -e "php conf - /etc/php/7.2/fpm/php-fpm.conf"
echo -e "\t\t - php-fpm: master process (/etc/php/7.2/fpm/php-fpm.conf)"
echo -e "\t\t\t - php-fpm: pool www"
echo -e "\t\t\t - include=/etc/php/7.2/fpm/pool.d/*.conf"
echo -e "php conf - /etc/php/7.2/fpm/pool.d/www.conf"
echo -e ""
echo -e "vi /etc/php/7.2/fpm/pool.d/www.conf"
echo -e "Check /etc/php/7.2/fpm/pool.d/www.conf for [[ more php real-time FPM status ]]"
echo -e "Check /etc/php/7.2/fpm/pool.d/www.conf for [[ access log file ]]"
echo -e "\t; Note: There is a real-time FPM status monitoring sample web page available"
echo -e "\t;       It's available in: /usr/share/php/7.2/fpm/status.html"
echo -e ""
echo -e "By default PHP-FPM is listening on the socket configured in */www.conf"
echo
echo "mysql"
echo "cat /etc/mysql/my.cnf"
echo "db data look like stored in /var/www/travel_list/storage"
echo -e "\t\tls /var/www/travel_list/storage/framework/sessions/ |wc -l => 371626"
echo
echo
echo
echo "===== config cheatsheet (src) ===="
echo "bin_src/nginx-1.16.1/conf"
echo
echo
echo "===== config cheatsheet (in VM) ===="
echo "conf: thread(all)/cache(db)"
echo "/usr/local/nginx/conf/nginx.conf"
echo "/etc/php/7.2/fpm/pool.d/www.conf"
echo "/etc/mysql/my.cnf"
echo "minor"
echo "/etc/php/7.2/fpm/php-fpm.conf"
echo ""
echo ""
echo "code:"
echo "/var/www/travel_list/resources/views/travel_list.blade.php"
echo "/var/www/travel_list/routes/web.php"
echo ""
echo "runtime:"
echo "databases directory (often - /var/lib/mysql)"
echo ""

# thread pool is hidden argvs
# https://www.cnblogs.com/zhoujinyi/p/5227462.html
#
# /etc/mysql/debian.cnf
# /etc/mysql/conf.d/*
echo
echo
echo "===== cpu affinity ===="
echo "ps -FeLf | tail -n 40"
echo "TODO -ps -FeLf | tail -n 40"
#ps -FeLf | tail -n 40
echo "ps -FeLf |grep sql | head -n 5"
ps -FeLf |grep sql | head -n 5
echo "ps -FeLf |grep sql | tail -n 5"
ps -FeLf |grep sql | tail -n 5
echo "TODO show 368 & 369"
echo "for i in \`seq 255 298\`; do echo \"\$i:\"; taskset -p \$i; done"
for i in `seq 255 298`; do echo "$i:"; taskset -p $i; done
#echo "for i in \`seq 368 369\`; do echo \"$\i:\"; taskset -p \$i; done"
#for i in `seq 368 369`; do echo "$i:"; taskset -p $i; done
echo "TODO how to get the dynamically created mysql threads"
echo
echo
echo "ps aux |egrep \"nginx|php|mysql\""
echo "TODO - invoke"
#ps aux |egrep "nginx|php|mysql"
echo
echo
echo -e "$0 Done!!\n\n\n\n"
########
# host
########
#$ ps aux |egrep "nginx|php|mysql"
#root      1246  0.0  0.0 340936 23164 ?        Ss   15:21   0:00 php-fpm: master process (/etc/php/7.2/fpm/php-fpm.conf)
#www-data  1249  0.0  0.0 340936  7028 ?        S    15:21   0:00 php-fpm: pool www
#www-data  1250  0.0  0.0 340936  7028 ?        S    15:21   0:00 php-fpm: pool www
#root      1585  0.0  0.0  21852  3452 ?        S    15:21   0:00 /bin/bash /usr/bin/mysqld_safe
#mysql     1747  0.0  0.1 628380 62336 ?        Sl   15:21   0:01 /usr/sbin/mysqld --basedir=/usr --datadir=/var/lib/mysql --plugin-dir=/usr/lib/mysql/plugin --user=mysql --skip-log-error --pid-file=/var/run/mysqld/mysqld.pid --socket=/var/run/mysqld/mysqld.sock --port=3306
#root      1748  0.0  0.0  23720  2444 ?        S    15:21   0:00 logger -t mysqld -p daemon.error
#root     30434  0.0  0.0  24788  1700 ?        Ss   16:14   0:00 nginx: master process objs/nginx
#www-data 30435  0.0  0.0  25196  2448 ?        S    16:14   0:00 nginx: worker process
#www-data 30436  0.0  0.0  25196  2448 ?        S    16:14   0:00 nginx: worker process
#www-data 30437  0.0  0.0  25196  2448 ?        S    16:14   0:00 nginx: worker process
#www-data 30438  0.0  0.0  25196  2448 ?        S    16:14   0:00 nginx: worker process
#
##########
# guest (nginx 4 workers)
##########
#bash-5.0# ps aux |egrep "nginx|php|mysql"
#   79 root      0:00 {mysqld_safe} /bin/bash /usr/bin/mysqld_safe
#  218 mysql     0:00 /usr/sbin/mysqld --basedir=/usr --datadir=/var/lib/mysql --plugin-dir=/usr/lib/mysql/plugin --user=mysql --skip-log-error --pid-file=/var/run/mysqld/mysqld.pid --socket=/var/run/mysqld/mysqld.sock --port=3306
#  219 root      0:00 logger -t mysqld -p daemon.error
#  330 root      0:00 nginx: master process nginx
#  331 www-data  0:00 nginx: worker process
#  332 www-data  0:00 nginx: worker process
#  333 www-data  0:00 nginx: worker process
#  334 www-data  0:00 nginx: worker process
#  353 root      0:00 {php-fpm7.2} php-fpm: master process (/etc/php/7.2/fpm/php-fpm.conf)
#  354 www-data  0:00 {php-fpm7.2} php-fpm: pool www
#  355 www-data  0:00 {php-fpm7.2} php-fpm: pool www
#  359 root      0:00 egrep nginx|php|mysql

