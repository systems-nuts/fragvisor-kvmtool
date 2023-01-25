#! /bin/bash
#
# auto_setup.sh
# Copyright (C) 2019 jackchuang <jackchuang@echo>
#
# Distributed under terms of the MIT license.
#
#
#
echo -e "\n\n\n======\n$0 auto setup Nginx start\n=======\n\n\n\n"
echo "src location: ~/c/pophype_make_ramdisk/bin_src/nginx-1.16.1/conf" 
echo
echo "ls /usr/local/nginx/conf/nginx.conf_nginx"
echo "cp /usr/local/nginx/conf/nginx.conf_nginx /usr/local/nginx/conf/nginx.conf # testing access.log error.log nginx.pid"
echo "cp /usr/local/nginx/conf/nginx.conf_nginx1 /usr/local/nginx/conf/nginx.conf"
echo "cp /usr/local/nginx/conf/nginx.conf_nginx2 /usr/local/nginx/conf/nginx.conf"
echo "cp /usr/local/nginx/conf/nginx.conf_nginx3 /usr/local/nginx/conf/nginx.conf"
echo "cp /usr/local/nginx/conf/nginx.conf_nginx4 /usr/local/nginx/conf/nginx.conf"
echo
echo 
echo "nginx -t # test conf files"
echo "nginx -c /usr/local/nginx/conf/nginx.conf_nginx # run with conf"
echo "nginx -c /usr/local/nginx/conf/nginx.conf_nginx1 # run with conf"
echo "nginx -c /usr/local/nginx/conf/nginx.conf_nginx2 # run with conf"
echo "nginx -c /usr/local/nginx/conf/nginx.conf_nginx3 # run with conf"
echo "nginx -c /usr/local/nginx/conf/nginx.conf_nginx4 # run with conf"
echo "nginx # run"
echo "nginx -s reload"
echo
echo
echo
echo
echo
nginx -c /usr/local/nginx/conf/nginx.conf_nginx # auto run nginx
echo
echo
echo
echo
echo "recap"
echo -e "<start>"
echo "nginx -c /usr/local/nginx/conf/nginx.conf_nginx" # start
echo -e "\n<reload>"
echo "cp /usr/local/nginx/conf/nginx.conf_nginx1 /usr/local/nginx/conf/nginx.conf"
echo "nginx -s reload"
echo
echo
echo -e "$0 Done!!\n\n\n\n"

