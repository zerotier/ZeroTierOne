#!/bin/bash

export PATH=/bin:/usr/bin:/sbin:/usr/sbin

if [ -f /Library/LaunchDaemons/com.zerotier.one.plist ]; then
	launchctl unload /Library/LaunchDaemons/com.zerotier.one.plist >>/dev/null 2>&1
fi

sleep 1

if [ -d "/Library/Application Support/ZeroTier/One" ]; then
	cd "/Library/Application Support/ZeroTier/One"
	if [ -f "zerotier-one.pid" ]; then
		ztpid=`cat zerotier-one.pid`
		if [ "$ztpid" -gt "0" ]; then
			kill `cat zerotier-one.pid`
		fi
	fi
fi

sleep 1

cd "/Applications"
rm -rf "ZeroTier One.app"

exit 0
