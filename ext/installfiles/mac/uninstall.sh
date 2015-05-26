#!/bin/bash

export PATH=/bin:/usr/bin:/sbin:/usr/sbin

if [ "$UID" -ne 0 ]; then
	echo "Must be run as root; try: sudo $0"
	exit 1
fi

if [ ! -f '/Library/LaunchDaemons/com.zerotier.one.plist' ]; then
	echo 'ZeroTier One does not seem to be installed.'
	exit 1
fi

cd /

echo 'Stopping any running ZeroTier One service...'
launchctl unload '/Library/LaunchDaemons/com.zerotier.one.plist' >>/dev/null 2>&1
sleep 1
killall -TERM zerotier-one >>/dev/null 2>&1
sleep 1
killall -KILL zerotier-one >>/dev/null 2>&1

echo "Making sure kext is unloaded..."
kextunload '/Library/Application Support/ZeroTier/One/tap.kext' >>/dev/null 2>&1

echo "Removing ZeroTier One files..."

rm -rf '/Applications/ZeroTier One.app'
rm -f '/usr/bin/zerotier-one' '/usr/bin/zerotier-idtool' '/usr/bin/zerotier-cli' '/Library/LaunchDaemons/com.zerotier.one.plist'
mkdir -p /tmp/ZeroTierOne_uninstall_tmp
cp "/Library/Application Support/ZeroTier/One/*.secret" /tmp/ZeroTierOne_uninstall_tmp
rm -rf '/Library/Application Support/ZeroTier/One'
mkdir -p '/Library/Application Support/ZeroTier/One'
cp "/tmp/ZeroTierOne_uninstall_tmp/*.secret" '/Library/Application Support/ZeroTier/One'
chmod 0600 "/Library/Application Support/ZeroTier/One/*.secret"
rm -rf /tmp/ZeroTierOne_uninstall_tmp

echo 'Uninstall complete.'
echo
echo 'Your identity and secret authentication token have been preserved in:'
echo '  /Library/Application Support/ZeroTier/One'
echo
echo 'You can delete this folder and its contents if you do not intend to re-use'
echo 'them.'
echo

exit 0
