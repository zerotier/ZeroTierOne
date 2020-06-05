#!/bin/bash

export PATH=/bin:/usr/bin:/sbin:/usr/sbin

rm -f /tmp/zt1-gui-restart.tmp
for i in `ps axuwww | tr -s ' ' ',' | grep -F '/Applications/ZeroTier,One.app' | grep -F -v grep | cut -d , -f 1,2 | xargs`; do
	u=`echo $i | cut -d , -f 1`
	p=`echo $i | cut -d , -f 2`
	if [ ! -z "$u" -a "0$p" -gt 0 ]; then
		kill $p >>/dev/null 2>&1
		sleep 0.2
		kill -9 $p >>/dev/null 2>&1
		echo "$u" >>/tmp/zt1-gui-restart.tmp
	fi
done
chmod 0600 /tmp/zt1-gui-restart.tmp

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
