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
rm -f '/usr/local/bin/zerotier-one' '/usr/local/bin/zerotier-idtool' '/usr/local/bin/zerotier-cli' '/Library/LaunchDaemons/com.zerotier.one.plist'

cd '/Library/Application Support/ZeroTier/One'
if [ "`pwd`" = '/Library/Application Support/ZeroTier/One' ]; then
	rm -rf *.d *.sh *.log *.old *.kext *.conf *.pkg *.dmg *.pid *.port *.save *.bin planet zerotier-* devicemap
fi

echo 'Uninstall complete.'
echo
echo 'Your identity and secret authentication token have been preserved in:'
echo '  /Library/Application Support/ZeroTier/One'
echo
echo 'You can delete this folder and its contents if you do not intend to re-use'
echo 'them.'
echo

exit 0
