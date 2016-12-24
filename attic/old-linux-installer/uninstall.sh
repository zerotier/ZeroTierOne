#!/bin/bash

export PATH=/bin:/usr/bin:/sbin:/usr/sbin:/usr/local/bin:/usr/local/sbin

if [ "$UID" -ne 0 ]; then
	echo "Must be run as root; try: sudo $0"
	exit 1
fi

# Detect systemd vs. regular init
SYSTEMDUNITDIR=
if [ -e /bin/systemctl -o -e /usr/bin/systemctl -o -e /usr/local/bin/systemctl -o -e /sbin/systemctl -o -e /usr/sbin/systemctl ]; then
	if [ -e /usr/bin/pkg-config ]; then
		SYSTEMDUNITDIR=`/usr/bin/pkg-config systemd --variable=systemdsystemunitdir`
	fi
	if [ -z "$SYSTEMDUNITDIR" -o ! -d "$SYSTEMDUNITDIR" ]; then
		if [ -d /usr/lib/systemd/system ]; then
			SYSTEMDUNITDIR=/usr/lib/systemd/system
		fi
		if [ -d /etc/systemd/system ]; then
			SYSTEMDUNITDIR=/etc/systemd/system
		fi
	fi
fi

echo "Killing any running zerotier-one service..."
if [ -n "$SYSTEMDUNITDIR" -a -d "$SYSTEMDUNITDIR" ]; then
	systemctl stop zerotier-one.service
	systemctl disable zerotier-one.service
else
	if [ -f /sbin/service -o -f /usr/sbin/service -o -f /bin/service -o -f /usr/bin/service ]; then
		service zerotier-one stop
	fi
fi

sleep 1
if [ -f /var/lib/zerotier-one/zerotier-one.pid ]; then
	kill -TERM `cat /var/lib/zerotier-one/zerotier-one.pid`
	sleep 1
fi
if [ -f /var/lib/zerotier-one/zerotier-one.pid ]; then
	kill -KILL `cat /var/lib/zerotier-one/zerotier-one.pid`
fi

if [ -f /etc/init.d/zerotier-one ]; then
	echo "Removing SysV init items..."
	if [ -f /sbin/chkconfig -o -f /usr/sbin/chkconfig -o -f /bin/chkconfig -o -f /usr/bin/chkconfig ]; then
		chkconfig zerotier-one off
	fi
	rm -f /etc/init.d/zerotier-one
	find /etc/rc*.d -type f -name '???zerotier-one' -print0 | xargs -0 rm -f
fi

if [ -n "$SYSTEMDUNITDIR" -a -d "$SYSTEMDUNITDIR" -a -f "$SYSTEMDUNITDIR/zerotier-one.service" ]; then
	echo "Removing systemd service..."
	rm -f "$SYSTEMDUNITDIR/zerotier-one.service"
fi

echo "Erasing binary and support files..."
if [ -d /var/lib/zerotier-one ]; then
	cd /var/lib/zerotier-one
	rm -rf zerotier-one *.persist identity.public *.log *.pid *.sh updates.d networks.d iddb.d root-topology ui
fi

echo "Erasing anything installed into system bin directories..."
rm -f /usr/local/bin/zerotier-cli /usr/bin/zerotier-cli /usr/local/bin/zerotier-idtool /usr/bin/zerotier-idtool

echo "Done."
echo
echo "Your ZeroTier One identity is still preserved in /var/lib/zerotier-one"
echo "as identity.secret and can be manually deleted if you wish. Save it if"
echo "you wish to re-use the address of this node, as it cannot be regenerated."

echo

exit 0
