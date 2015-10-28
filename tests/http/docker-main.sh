#!/bin/bash

export PATH=/bin:/usr/bin:/usr/local/bin:/sbin:/usr/sbin

/zerotier-one -d

while [ ! -d "/proc/sys/net/ipv6/conf/zt0" ]; do
	sleep 0.25
done

sleep 2

exec node --harmony /agent.js >>agent.out 2>&1
#exec node --harmony /agent.js
