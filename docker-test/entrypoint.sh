#!/bin/bash

export PATH=/bin:/usr/bin:/usr/local/bin:/sbin:/usr/sbin

echo '***'
echo '*** ZeroTier Network Containers Preview'
echo '*** https://www.zerotier.com/'
echo '***'
echo '*** Starting ZeroTier network container host...'

/zerotier-one -d -U -p9993 >>/zerotier-one.out 2>&1

echo '*** Waiting for initial identity generation...'

while [ ! -s /var/lib/zerotier-one/identity.secret ]; do
	sleep 0.5
done

cat /var/lib/zerotier-one/identity.public

echo '*** Waiting for network configuration...'

virtip4=""
while [ ! -s /var/lib/zerotier-one/networks.d/e5cd7a9e1c5311ab.conf ]; do
	sleep 0.5
done
while [ -z "$virtip4" ]; do
	virtip4=`/zerotier-cli listnetworks | grep -F e5cd7a9e1c5311ab | cut -d ' ' -f 9 | sed 's/,/\n/g' | grep -F '.' | cut -d / -f 1`
done

echo '***'
echo '*** Up and running at ' $virtip4 ' -- join network e5cd7a9e1c5311ab and try:'
echo '*** > ping ' $virtip4
echo -n '*** > curl http://'
echo -n $virtip4
echo '/'
echo '***'

rm -rf /run/httpd/* /tmp/httpd*
exec intercept /usr/sbin/httpd -D FOREGROUND >>/apache.out 2>&1
