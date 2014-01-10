#!/bin/bash

export PATH=/bin:/usr/bin:/sbin:/usr/sbin

zthome="/Library/Application Support/ZeroTier/One"
ztapp=`mdfind kMDItemCFBundleIdentifier == 'com.zerotier.ZeroTierOne' | grep -E '.*/ZeroTier One[.]app$' | sort | head -n 1`
if [ -z "$ztapp" -o ! -d "$ztapp" ]; then
	ztapp="/Applications/ZeroTier One.app"
fi

if [ "$UID" -ne 0 ]; then
	echo "Must be run as root; try: sudo $0"
	exit 1
fi

echo "Killing any running zerotier-one service..."
killall -TERM zerotier-one >>/dev/null 2>&1
sleep 3
killall -KILL zerotier-one >>/dev/null 2>&1
sleep 1

echo "Unloading kernel extension..."
kextunload "$zthome/pre10.8/tap.kext" >>/dev/null 2>&1
kextunload "$zthome/tap.kext" >>/dev/null 2>&1

echo "Erasing GUI app (if installed)..."
if [ ! -z "$ztapp" -a -d "$ztapp" ]; then
	rm -rf "$ztapp"
fi

echo "Erasing service and support files..."
rm -f /usr/bin/zerotier-cli
cd "$zthome"
rm -f zerotier-one *.persist identity.public *.log *.pid *.sh shutdownIfUnreadable
rm -rf pre10.8 tap.kext updates.d networks.d

echo "Removing LaunchDaemons item..."
rm -f /Library/LaunchDaemons/com.zerotier.one.plist
launchctl remove com.zerotier.one

echo "Done."
echo
echo "Your ZeroTier One identity is still in: $zthome"
echo "as identity.secret and can be manually deleted if you wish. Save it if"
echo "you wish to re-use the address of this node, as it cannot be regenerated."

echo

exit 0
