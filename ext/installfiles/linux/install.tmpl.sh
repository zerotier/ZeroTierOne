#!/bin/bash

export PATH=/bin:/usr/bin:/sbin:/usr/sbin
shopt -s expand_aliases

dryRun=0

echo "*** ZeroTier One install/update ***"

if [ "$UID" -ne 0 ]; then
	echo "Not running as root so doing dry run (no modifications to system)..."
	dryRun=1
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

if [ $dryRun -gt 0 ]; then
	alias ln="echo '>> dry run: ln'"
	alias rm="echo '>> dry run: rm'"
	alias mv="echo '>> dry run: mv'"
	alias chown="echo '>> dry run: chown'"
	alias chgrp="echo '>> dry run: chgrp'"
	alias chkconfig="echo '>> dry run: chkconfig'"
	alias zerotier-cli="echo '>> dry run: zerotier-cli'"
	alias service="echo '>> dry run: service'"
	alias systemctl="echo '>> dry run: systemctl'"
fi

scriptPath="`dirname "$0"`/`basename "$0"`"
if [ ! -r "$scriptPath" ]; then
	scriptPath="$0"
	if [ ! -r "$scriptPath" ]; then
		echo "Installer cannot determine its own path; $scriptPath is not readable."
		exit 2
	fi
fi

endMarkerIndex=`grep -a -b -E '^################' "$scriptPath" | head -c 16 | cut -d : -f 1`
if [ "$endMarkerIndex" -le 100 ]; then
	echo 'Internal error: unable to find end of script / start of binary data marker.'
	exit 2
fi
blobStart=`expr $endMarkerIndex + 17`
if [ "$blobStart" -le "$endMarkerIndex" ]; then
	echo 'Internal error: unable to find end of script / start of binary data marker.'
	exit 2
fi

echo 'Extracting files...'
if [ $dryRun -gt 0 ]; then
	echo ">> dry run: tail -c +$blobStart \"$scriptPath\" | gunzip -c | tar -xvop -C / -f -"
else
	tail -c +$blobStart "$scriptPath" | gunzip -c | tar -xvop -C / -f -
fi

if [ $dryRun -eq 0 -a ! -d "/var/lib/zerotier-one" ]; then
	echo 'Archive extraction failed, cannot find zerotier-one binary in "/var/lib/zerotier-one".'
	exit 2
fi

echo 'Installing zerotier-cli command line utility...'

rm -f /usr/bin/zerotier-cli
ln -sf /var/lib/zerotier-one/zerotier-one /usr/bin/zerotier-cli

echo 'Installing and (re-)starting zerotier-one daemon...'

# Note: ensure that service restarts are the last thing this script actually
# does, since these may kill the script itself. Also note the & to allow
# them to finish independently.
if [ -n "$SYSTEMDUNITDIR" -a -d "$SYSTEMDUNITDIR" ]; then
	# If this was updated or upgraded from an init.d based system, clean up the old
	# init.d stuff before installing directly via systemd.
	if [ -f /etc/init.d/zerotier-one ]; then
		if [ -e /sbin/chkconfig -o -e /usr/sbin/chkconfig -o -e /bin/chkconfig -o -e /usr/bin/chkconfig ]; then
			chkconfig zerotier-one off
		fi
		rm -f /etc/init.d/zerotier-one
	fi

	cp -f /tmp/systemd_zerotier-one.service "$SYSTEMDUNITDIR/zerotier-one.service"
	rm -f /tmp/systemd_zerotier-one.service /tmp/init.d_zerotier-one

	systemctl enable zerotier-one
	systemctl restart zerotier-one &
else
	cp -f /tmp/init.d_zerotier-one /etc/init.d/zerotier-one
	chmod 0755 /etc/init.d/zerotier-one
	rm -f /tmp/systemd_zerotier-one.service /tmp/init.d_zerotier-one

	chkconfig zerotier-one on
	service zerotier-one restart &
fi

exit 0

# Do not remove the last line or add a carriage return to it! The installer
# looks for an unterminated line beginning with 16 #'s in itself to find
# the binary blob data, which is appended after it.

################