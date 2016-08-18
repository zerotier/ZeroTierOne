echo '*** ZeroTier-Kubernetes self-auth test script'
chown -R daemon /var/lib/zerotier-one
chgrp -R daemon /var/lib/zerotier-one
su daemon -s /bin/bash -c '/zerotier-one -d -U -p9993 >>/tmp/zerotier-one.out 2>&1'
virtip4=""
while [ -z "$virtip4" ]; do
	sleep 0.2
	virtip4=`/zerotier-cli listnetworks | grep -F $nwid | cut -d ' ' -f 9 | sed 's/,/\n/g' | grep -F '.' | cut -d / -f 1`
	dev=`/zerotier-cli listnetworks | grep -F "" | cut -d ' ' -f 8 | cut -d "_" -f 2 | sed "s/^<dev>//" | tr '\n' '\0'`
done
echo '*** Up and running at' $virtip4 ' on network: ' $nwid

echo '*** Self-Authorizing to deployment network'
nwconf=$(ls *.conf)
nwid="${nwconf%.*}"

AUTHTOKEN=$(cat /var/lib/zerotier-one/authtoken.secret)
sed "s|\local_service_auth_token|${AUTHTOKEN}|" .zerotierCliSettings > /root/.zerotierCliSettings

./zerotier-cli join $(nwid).conf
./zerotier-cli net-auth $(nwid) $(dev)

# node server.js