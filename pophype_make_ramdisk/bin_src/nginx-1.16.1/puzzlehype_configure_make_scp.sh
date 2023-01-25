make clean

#./configure --with-cc-opt="-static -static-libgcc" --with-ld-opt="-static" --with-cpu-opt=generic --with-pcre --with-poll_module --with-select_module --with-select_module --with-poll_module --with-http_realip_module --without-pcre --without-http_rewrite_module --with-http_addition_module --with-http_sub_module --with-http_dav_module --with-http_gunzip_module --with-http_gzip_static_module --with-http_auth_request_module --with-http_random_index_module --with-http_secure_link_module --with-http_degradation_module --with-http_stub_status_module


./configure \
--with-cpu-opt=generic --with-pcre \
--with-poll_module --with-select_module \
--with-select_module --with-poll_module \
--with-http_realip_module \
--with-pcre --without-http_rewrite_module \
--with-http_addition_module --with-http_sub_module --with-http_dav_module \
--with-http_gunzip_module \
--with-http_gzip_static_module --with-http_auth_request_module \
--with-http_random_index_module --with-http_secure_link_module \
--with-http_degradation_module --with-http_stub_status_module 


echo -e "\n\n"

make -j32

echo -e "\n\n"

echo "scp to VM on mir/echo"
#echo
scp objs/nginx root@10.4.4.222:/bin&
# mir
scp objs/nginx root@10.1.10.222:/bin&

