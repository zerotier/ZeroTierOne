#!/bin/bash

export PATH=/bin:/usr/bin:/sbin:/usr/sbin:/usr/local/bin

launchctl unload /Library/LaunchDaemons/com.zerotier.one.plist >>/dev/null 2>&1

cd "/Library/Application Support/ZeroTier/One"
rm -rf node.log node.log.old root-topology shutdownIfUnreadable autoupdate.log updates.d
chown -R 0 tap.kext
chgrp -R 0 tap.kext
if [ ! -f authtoken.secret ]; then
	head -c 4096 /dev/urandom | md5 | head -c 24 >authtoken.secret
	chown 0 authtoken.secret
	chgrp 0 authtoken.secret
	chmod 0600 authtoken.secret
fi
rm -f zerotier-cli zerotier-idtool
ln -sf zerotier-one zerotier-cli
ln -sf zerotier-one zerotier-idtool

mkdir -p /usr/local/bin
cd /usr/local/bin
rm -f zerotier-cli zerotier-idtool
ln -sf "/Library/Application Support/ZeroTier/One/zerotier-one" zerotier-cli
ln -sf "/Library/Application Support/ZeroTier/One/zerotier-one" zerotier-idtool

launchctl load /Library/LaunchDaemons/com.zerotier.one.plist >>/dev/null 2>&1

exit 0
