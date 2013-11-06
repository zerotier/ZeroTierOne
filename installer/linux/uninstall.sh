#!/bin/bash

export PATH=/bin:/usr/bin:/sbin:/usr/sbin:/usr/local/bin:/usr/local/sbin

if [ "$UID" -ne 0 ]; then
	echo "Must be run as root; try: sudo ./uninstall.sh"
	exit 1
fi

echo "Going to uninstall zerotier-one, hit CTRL+C to abort."
echo "Waiting 5 seconds..."
sleep 5

ztpath="/Library/Application Support/ZeroTier/One"

echo "Killing any running zerotier-one service..."
killall -TERM zerotier-one
sleep 3
killall -q -KILL zerotier-one

echo "Erasing binary and support files..."
cd $ztpath
rm -fv zerotier-one
rm -rfv updates.d
rm -fv *.persist
rm -rfv networks.d
rm -fv authtoken.secret
rm -fv identity.public

echo "Removing init items..."
chkconfig zerotier-one off
rm -fv /etc/init.d/zerotier-one

echo "Done. (identity still preserved in $ztpath)"
