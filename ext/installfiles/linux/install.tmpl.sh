#!/bin/bash

export PATH=/bin:/usr/bin:/sbin:/usr/sbin:/usr/local/bin:/usr/local/sbin
shopt -s expand_aliases

dryRun=0

echo "*** ZeroTier One install/update ***"
echo

if [ "$UID" -ne 0 ]; then
	echo "Not running as root so doing dry run (no modifications to system)..."
	dryRun=1
fi

if [ $dryRun -gt 0 ]; then
	alias ln="echo '>> ln'"
	alias rm="echo '>> rm'"
	alias mv="echo '>> mv'"
	alias cp="echo '>> cp'"
	alias chown="echo '>> chown'"
	alias chgrp="echo '>> chgrp'"
	alias chmod="echo '>> chmod'"
	alias chkconfig="echo '>> chkconfig'"
	alias zerotier-cli="echo '>> zerotier-cli'"
	alias service="echo '>> service'"
	alias systemctl="echo '>> systemctl'"
fi

scriptPath="`dirname "$0"`/`basename "$0"`"
if [ ! -r "$scriptPath" ]; then
	scriptPath="$0"
	if [ ! -r "$scriptPath" ]; then
		echo "Installer cannot determine its own path; $scriptPath is not readable."
		exit 2
	fi
fi

# Check for systemd vs. old school SysV init
SYSTEMDUNITDIR=
if [ -e /bin/systemctl -o -e /usr/bin/systemctl -o -e /usr/local/bin/systemctl -o -e /sbin/systemctl -o -e /usr/sbin/systemctl ]; then
	# Second check: test if systemd appears to actually be running. Apparently Ubuntu
	# thought it was a good idea to ship with systemd installed but not used. Issue #133
	if [ -d /var/run/systemd/system -o -d /run/systemd/system ]; then
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
fi

# Find the end of this script, which is where we have appended binary data.
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

echo -n 'Getting version of existing install... '
origVersion=NONE
if [ -x /var/lib/zerotier-one/zerotier-one ]; then
	origVersion=`/var/lib/zerotier-one/zerotier-one -v`
fi
echo $origVersion

echo 'Extracting files...'
if [ $dryRun -gt 0 ]; then
	echo ">> tail -c +$blobStart \"$scriptPath\" | gunzip -c | tar -xvop -C / -f -"
	tail -c +$blobStart "$scriptPath" | gunzip -c | tar -t -f - | sed 's/^/>>   /'
else
	tail -c +$blobStart "$scriptPath" | gunzip -c | tar -xvop --no-overwrite-dir -C / -f -
fi

if [ $dryRun -eq 0 -a ! -x "/var/lib/zerotier-one/zerotier-one" ]; then
	echo 'Archive extraction failed, cannot find zerotier-one binary in "/var/lib/zerotier-one".'
	exit 2
fi

echo -n 'Getting version of new install... '
newVersion=`/var/lib/zerotier-one/zerotier-one -v`
echo $newVersion

echo 'Creating symlinks...'

rm -f /usr/bin/zerotier-cli /usr/bin/zerotier-idtool
ln -sf /var/lib/zerotier-one/zerotier-one /usr/bin/zerotier-cli
ln -sf /var/lib/zerotier-one/zerotier-one /usr/bin/zerotier-idtool

echo 'Installing zerotier-one service...'

if [ -n "$SYSTEMDUNITDIR" -a -d "$SYSTEMDUNITDIR" ]; then
	# SYSTEMD

	# If this was updated or upgraded from an init.d based system, clean up the old
	# init.d stuff before installing directly via systemd.
	if [ -f /etc/init.d/zerotier-one ]; then
		if [ -e /sbin/chkconfig -o -e /usr/sbin/chkconfig -o -e /bin/chkconfig -o -e /usr/bin/chkconfig ]; then
			chkconfig zerotier-one off
		fi
		rm -f /etc/init.d/zerotier-one
	fi

	cp -f /tmp/systemd_zerotier-one.service "$SYSTEMDUNITDIR/zerotier-one.service"
	chown 0 "$SYSTEMDUNITDIR/zerotier-one.service"
	chgrp 0 "$SYSTEMDUNITDIR/zerotier-one.service"
	chmod 0644 "$SYSTEMDUNITDIR/zerotier-one.service"
	rm -f /tmp/systemd_zerotier-one.service /tmp/init.d_zerotier-one

	systemctl enable zerotier-one.service

	echo
	echo 'Done! Installed and service configured to start at system boot.'
	echo
	echo "To start now or restart the service if it's already running:"
	echo '  sudo systemctl restart zerotier-one.service'
else
	# SYSV INIT -- also covers upstart which supports SysVinit backward compatibility

	cp -f /tmp/init.d_zerotier-one /etc/init.d/zerotier-one
	chmod 0755 /etc/init.d/zerotier-one
	rm -f /tmp/systemd_zerotier-one.service /tmp/init.d_zerotier-one

	if [ -f /sbin/chkconfig -o -f /usr/sbin/chkconfig -o -f /usr/bin/chkconfig -o -f /bin/chkconfig ]; then
		chkconfig zerotier-one on
	else
		# Yes Virginia, some systems lack chkconfig.
		if [ -d /etc/rc0.d ]; then
			rm -f /etc/rc0.d/???zerotier-one
			ln -sf /etc/init.d/zerotier-one /etc/rc0.d/K89zerotier-one
		fi
		if [ -d /etc/rc1.d ]; then
			rm -f /etc/rc1.d/???zerotier-one
			ln -sf /etc/init.d/zerotier-one /etc/rc1.d/K89zerotier-one
		fi
		if [ -d /etc/rc2.d ]; then
			rm -f /etc/rc2.d/???zerotier-one
			ln -sf /etc/init.d/zerotier-one /etc/rc2.d/S11zerotier-one
		fi
		if [ -d /etc/rc3.d ]; then
			rm -f /etc/rc3.d/???zerotier-one
			ln -sf /etc/init.d/zerotier-one /etc/rc3.d/S11zerotier-one
		fi
		if [ -d /etc/rc4.d ]; then
			rm -f /etc/rc4.d/???zerotier-one
			ln -sf /etc/init.d/zerotier-one /etc/rc4.d/S11zerotier-one
		fi
		if [ -d /etc/rc5.d ]; then
			rm -f /etc/rc5.d/???zerotier-one
			ln -sf /etc/init.d/zerotier-one /etc/rc5.d/S11zerotier-one
		fi
		if [ -d /etc/rc6.d ]; then
			rm -f /etc/rc6.d/???zerotier-one
			ln -sf /etc/init.d/zerotier-one /etc/rc6.d/K89zerotier-one
		fi
	fi

	echo
	echo 'Done! Installed and service configured to start at system boot.'
	echo
	echo "To start now or restart the service if it's already running:"
	echo '  sudo service zerotier-one restart'
fi

exit 0

# Do not remove the last line or add a carriage return to it! The installer
# looks for an unterminated line beginning with 16 #'s in itself to find
# the binary blob data, which is appended after it.

################