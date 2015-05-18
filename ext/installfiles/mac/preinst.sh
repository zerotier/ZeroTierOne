#!/bin/bash

export PATH=/bin:/usr/bin:/sbin:/usr/sbin

if [ -f /Library/LaunchDaemons/com.zerotier.one.plist ]; then
	launchctl unload /Library/LaunchDaemons/com.zerotier.one.plist >>/dev/null 2>&1
	sleep 1
	killall zerotier-one
	sleep 1
	killall -9 zerotier-one
fi

cd /Applications
rm -rf "ZeroTier One.app"
