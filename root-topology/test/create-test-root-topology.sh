#!/bin/bash

if [ ! -e ../mktopology ]; then
	echo 'Build ../mktopology first!'
	exit 1
fi

echo 'Populating rootservers/* with all Docker test-rootserver-* container IPs and identities...'

rm -rf rootservers
mkdir rootservers

for cid in `docker ps -f 'name=test-rootserver-*' -q`; do
	id=`docker exec $cid cat /var/lib/zerotier-one/identity.secret | cut -d : -f 1-3`
	ztaddr=`echo $id | cut -d : -f 1`
	ip=`docker exec $cid ifconfig | sed -En 's/127.0.0.1//;s/.*inet (addr:)?(([0-9]*\.){3}[0-9]*).*/\2/p'`
	echo $cid $ztaddr $id $ip
	echo "id=$id" >rootservers/$ztaddr
	echo "udp=$ip/9993" >>rootservers/$ztaddr
done

echo 'Creating test-root-topology...'

rm -f test-root-topology
../mktopology >test-root-topology

echo 'Done!'
echo
cat test-root-topology

exit 0
