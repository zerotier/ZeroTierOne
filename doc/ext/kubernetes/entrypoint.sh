echo '*** ZeroTier-Kubernetes self-auth test script'
chown -R daemon /var/lib/zerotier-one
chgrp -R daemon /var/lib/zerotier-one
su daemon -s /bin/bash -c '/zerotier-one -d -U -p9993 >>/tmp/zerotier-one.out 2>&1'
virtip4=""
while [ -z "$virtip4" ]; do
	sleep 0.2
	virtip4=`/zerotier-cli listnetworks | grep -F $nwid | cut -d ' ' -f 9 | sed 's/,/\n/g' | grep -F '.' | cut -d / -f 1`
done
echo '*** Up and running at' $virtip4 ' on network: ' $nwid

echo '*** Self-Authorizing to deployment network'
./zerotier-cli join $(NWID).conf
./zerotier-cli net-auth $(NWID) $(DEVID)

# node server.js