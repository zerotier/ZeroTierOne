#!/bin/bash

# Parameters for test
test_name=httpd_demo
nwid=e5cd7a9e1c5311ab # test network
file_path=/opt/results/ # test result output file path (fs shared between host and containers)
file_base="$test_name".txt # test result output file
address_file="$file_path$test_name"_addr.txt # file shared between host and containers for sharing address (optional)

export PATH=/bin:/usr/bin:/usr/local/bin:/sbin:/usr/sbin:/

echo '*** ZeroTier Network Containers Test: httpd_demo'
chown -R daemon /var/lib/zerotier-one
chgrp -R daemon /var/lib/zerotier-one
su daemon -s /bin/bash -c '/zerotier-one -d -U -p9993 >>/tmp/zerotier-one.out 2>&1'
echo '*** Waiting for initial identity generation...'

while [ ! -s /var/lib/zerotier-one/identity.secret ]; do
	sleep 0.2
done

echo '*** Waiting for network config...'

virtip4=""
while [ ! -s /var/lib/zerotier-one/networks.d/"$nwid".conf ]; do
	sleep 0.2
done
while [ -z "$virtip4" ]; do
	sleep 0.2
	virtip4=`/zerotier-cli listnetworks | grep -F $nwid | cut -d ' ' -f 9 | sed 's/,/\n/g' | grep -F '.' | cut -d / -f 1`
done

echo '*** Starting Apache...'
echo '*** Up and running at' $virtip4
echo $virtip4 > "$address_file"

sleep 0.5
rm -rf /run/httpd/* /tmp/httpd*
intercept /usr/sbin/httpd -D FOREGROUND >>/tmp/apache.out 2>&1
