#!/bin/bash

export PATH=/bin:/usr/bin:/usr/local/bin:/sbin:/usr/sbin

if [ ! -c "/dev/net/tun" ]; then
	echo 'FATAL: must be docker run with: --device=/dev/net/tun --cap-add=NET_ADMIN'
	exit 1
fi

if [ -z "$ZEROTIER_IDENTITY_SECRET" ]; then
	echo 'FATAL: ZEROTIER_IDENTITY_SECRET not set -- aborting!'
	exit 1
fi

if [ -z "$ZEROTIER_NETWORK" ]; then
	echo 'Warning: ZEROTIER_NETWORK not set, you will need to docker exec zerotier-cli to join a network.'
else
	# The existence of a .conf will cause the service to "remember" this network
	touch /var/lib/zerotier-one/networks.d/$ZEROTIER_NETWORK.conf
fi

rm -f /var/lib/zerotier-one/identity.*
echo "$ZEROTIER_IDENTITY_SECRET" >identity.secret

/var/lib/zerotier-one/zerotier-one
