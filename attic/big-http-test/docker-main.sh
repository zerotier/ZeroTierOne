#!/bin/bash

export PATH=/bin:/usr/bin:/usr/local/bin:/sbin:/usr/sbin

/zerotier-one -d >>zerotier-one.out 2>&1

# Wait for ZeroTier to start and join the network
while [ ! -d "/proc/sys/net/ipv6/conf/zt0" ]; do
	sleep 0.25
done

# Wait just a bit longer for stuff to settle
sleep 5

exec node --harmony /agent.js >>agent.out 2>&1
#exec node --harmony /agent.js
