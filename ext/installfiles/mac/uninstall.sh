#!/bin/bash

export PATH=/bin:/usr/bin:/sbin:/usr/sbin

ztpath="/Library/Application Support/ZeroTier/One"
ztapp="/Applications/ZeroTier One.app"

if [ "$UID" -ne 0 ]; then
	echo "Must be run as root; try: sudo $0"
	exit 1
fi

echo

echo "This will uninstall ZeroTier One, hit CTRL+C to abort."
echo "Waiting 5 seconds..."
sleep 5

echo "Unloading and removing LaunchDaemons item..."
launchctl unload /Library/LaunchDaemons/com.zerotier.one.plist
rm -f /Library/LaunchDaemons/com.zerotier.one.plist

echo "Killing any running zerotier-one service..."
killall -TERM zerotier-one >>/dev/null 2>&1
sleep 3
killall -KILL zerotier-one >>/dev/null 2>&1
sleep 1

echo "Unloading kernel extension..."
kextunload "$ztpath/tap.kext"

echo "Erasing UI app, binary, and support files..."
cd $ztpath
rm -rfv "$ztapp" zerotier-one *.persist authtoken.secret identity.public *.log *.pid *.kext *.sh

echo "Done."
echo
echo "Your ZeroTier One identity is still preserved in $ztpath"
echo "as identity.secret and can be manually deleted if you wish. Save it if"
echo "you wish to re-use the address of this node, as it cannot be regenerated."

echo

exit 0
