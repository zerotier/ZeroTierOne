#!/bin/bash

export PATH=/bin:/usr/bin:/sbin:/usr/sbin:/usr/local/bin

cd "/Library/Application Support/ZeroTier/One"

if [ ! -f authtoken.secret ]; then
	head -c 1024 /dev/urandom | md5 | head -c 24 >authtoken.secret
	chown 0 authtoken.secret
	chgrp 0 authtoken.secret
	chmod 0600 authtoken.secret
fi

rm -f zerotier-cli zerotier-idtool
ln -sf zerotier-one zerotier-cli
ln -sf zerotier-one zerotier-idtool
if [ ! -d /usr/local/bin ]; then
	mkdir -p /usr/local/bin
fi
cd /usr/local/bin
rm -f zerotier-cli zerotier-idtool
ln -sf "/Library/Application Support/ZeroTier/One/zerotier-one" zerotier-cli
ln -sf "/Library/Application Support/ZeroTier/One/zerotier-one" zerotier-idtool

launchctl load /Library/LaunchDaemons/com.zerotier.one.plist >>/dev/null 2>&1
sleep 2

if [ -f /tmp/zt1-gui-restart.tmp ]; then
	for u in `cat /tmp/zt1-gui-restart.tmp`; do
		if [ -f '/Applications/ZeroTier One.app/Contents/MacOS/ZeroTier One' ]; then
			su $u -c '/Applications/ZeroTier\ One.app/Contents/MacOS/ZeroTier\ One &' >>/dev/null 2>&1 &
		else
			su $u -c '/Applications/ZeroTier.app/Contents/MacOS/ZeroTier &' >>/dev/null 2>&1 &
		fi
	done
fi
rm -f /tmp/zt1-gui-restart.tmp

exit 0
