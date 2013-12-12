#!/bin/bash

export PATH=/bin:/usr/bin:/sbin:/usr/sbin:/usr/local/bin:/usr/local/sbin

if [ "$UID" -ne 0 ]; then
	echo "Must be run as root; try: sudo $0"
	exit 1
fi

echo

echo "This will uninstall ZeroTier One, hit CTRL+C to abort."
echo "Waiting 5 seconds..."
sleep 5

echo "Killing any running zerotier-one service..."
killall -q -TERM zerotier-one
sleep 2
killall -q -KILL zerotier-one

echo "Removing SysV init items..."
rm -fv /etc/init.d/zerotier-one
find /etc/rc*.d -name '???zerotier-one' -print0 | xargs -0 rm -fv

echo "Erasing binary and support files..."
cd /var/lib/zerotier-one
rm -fv zerotier-one *.persist authtoken.secret identity.public *.log *.pid
rm -rfv updates.d networks.d iddb.d

echo "Erasing anything installed into system bin directories..."
rm -fv /usr/local/bin/zerotier-* /usr/bin/zerotier-*

echo "Done."
echo
echo "Your ZeroTier One identity is still preserved in /var/lib/zerotier-one"
echo "as identity.secret and can be manually deleted if you wish. Save it if"
echo "you wish to re-use the address of this node, as it cannot be regenerated."

echo

exit 0
