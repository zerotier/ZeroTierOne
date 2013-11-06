#!/bin/bash

if [ "$UID" -ne 0 ]; then
	echo "Must be run as root; try: sudo ./uninstall.sh"
	exit 1
fi

ztpath="/Library/Application Support/ZeroTier/One"

echo "Killing any running zerotier-one service..."
killall zerotier-one
sleep 5

echo "Erasing binary and support files..."
cd $ztpath
rm -fv zerotier-one
rm -rfv updates.d
rm -fv *.persist
rm -rfv networks.d
rm -fv authtoken.secret

