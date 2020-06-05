#!/bin/bash

export PATH=/bin:/usr/bin:/sbin:/usr/sbin:/usr/local/bin

OSX_RELEASE=`sw_vers -productVersion | cut -d . -f 1,2`
DARWIN_MAJOR=`uname -r | cut -d . -f 1`

launchctl unload /Library/LaunchDaemons/com.zerotier.one.plist >>/dev/null 2>&1
sleep 0.5

cd "/Library/Application Support/ZeroTier/One"

if [ "$OSX_RELEASE" = "10.7" ]; then
	# OSX 10.7 cannot use the new tap driver since the new way of kext signing
	# is not backward compatible. Pull the old one for 10.7 users and replace.
	# We use https to fetch and check hash as an extra added measure.
	rm -f tap.kext.10_7.tar.gz
	curl -s https://download.zerotier.com/tap.kext.10_7.tar.gz >tap.kext.10_7.tar.gz
	if [ -s tap.kext.10_7.tar.gz -a "`shasum -a 256 tap.kext.10_7.tar.gz | cut -d ' ' -f 1`" = "e133d4832cef571621d3618f417381b44f51a76ed625089fb4e545e65d3ef2a9" ]; then
		rm -rf tap.kext
		tar -xzf tap.kext.10_7.tar.gz
	fi
	rm -f tap.kext.10_7.tar.gz
fi

rm -rf node.log node.log.old root-topology shutdownIfUnreadable autoupdate.log updates.d ui peers.save

chown -R 0 tap.kext
chgrp -R 0 tap.kext

if [ ! -f authtoken.secret ]; then
	head -c 1024 /dev/urandom | md5 | head -c 24 >authtoken.secret
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

if [ $DARWIN_MAJOR -le 16 ]; then
	cd "/Library/Application Support/ZeroTier/One"
	kextload -r . tap.kext >>/dev/null 2>&1 &
	disown %1
fi

launchctl load /Library/LaunchDaemons/com.zerotier.one.plist >>/dev/null 2>&1

sleep 1

if [ -f /tmp/zt1-gui-restart.tmp ]; then
	for u in `cat /tmp/zt1-gui-restart.tmp`; do
		su $u -c '/Applications/ZeroTier\ One.app/Contents/MacOS/ZeroTier\ One &' >>/dev/null 2>&1 &
	done
fi
rm -f /tmp/zt1-gui-restart.tmp

exit 0
