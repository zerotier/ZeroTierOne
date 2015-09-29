#!/bin/bash

export PATH=/bin:/usr/bin:/usr/local/bin:/sbin:/usr/sbin:/

echo '***'
echo '*** ZeroTier Network Containers Preview'
echo '*** https://www.zerotier.com/'
echo '***'
echo '*** Starting ZeroTier network container host...'

chown -R daemon /var/lib/zerotier-one
chgrp -R daemon /var/lib/zerotier-one
su daemon -s /bin/bash -c '/zerotier-one -d -U -p9993 >>/tmp/zerotier-one.out 2>&1'
#/zerotier-one -d -U -p9993 >>/tmp/zerotier-one.out 2>&1

echo '*** Waiting for initial identity generation...'

while [ ! -s /var/lib/zerotier-one/identity.secret ]; do
	sleep 0.2
done

# HACK: permissions model nonexistent yet... make sure httpd's can talk to this...
chmod 0777 /tmp/.zt*

echo '*** Waiting for network config...'

virtip4=""
while [ ! -s /var/lib/zerotier-one/networks.d/8056c2e21c000001.conf ]; do
	sleep 0.2
done
while [ -z "$virtip4" ]; do
	sleep 0.2
	virtip4=`/zerotier-cli listnetworks | grep -F 8056c2e21c000001 | cut -d ' ' -f 9 | sed 's/,/\n/g' | grep -F '.' | cut -d / -f 1`
done

echo '*** Starting Apache...'

echo '***'
echo '*** Up and running at' $virtip4 '-- join network 8056c2e21c000001 and try:'
echo '*** > ping' $virtip4
echo -n '*** > curl http://'
echo -n $virtip4
echo '/'
echo '***'
echo "*** Be (a little) patient. It'll probably take 1-2 minutes to be reachable."
echo '***'
echo '*** Follow https://www.zerotier.com/blog for news and release announcements!'
echo '***'

#while /bin/true; do
#	sleep 1000000
#done

sleep 0.5
rm -rf /run/httpd/* /tmp/httpd*
intercept /usr/sbin/httpd -D FOREGROUND >>/tmp/apache.out 2>&1
