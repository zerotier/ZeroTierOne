#!/bin/bash

export PATH=/bin:/usr/bin:/sbin:/usr/sbin
shopt -s expand_aliases

if [ "$UID" -ne 0 ]; then
	echo '*** Auto-updater must be run as root.'
	exit 1
fi

scriptPath="`dirname "$0"`/`basename "$0"`"
if [ ! -s "$scriptPath" ]; then
	scriptPath="$0"
	if [ ! -s "$scriptPath" ]; then
		echo "*** Auto-updater cannot determine its own path; $scriptPath is not readable."
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

rm -f /tmp/ZeroTierOne-update.pkg
tail -c +$blobStart "$scriptPath" >/tmp/ZeroTierOne-update.pkg
chmod 0600 /tmp/ZeroTierOne-update.pkg

if [ -s /tmp/ZeroTierOne-update.pkg ]; then
	rm -f '/Library/Application Support/ZeroTier/One/latest-update.exe' '/Library/Application Support/ZeroTier/One/latest-update.json' /tmp/ZeroTierOne-update.log
	installer -verbose -pkg /tmp/ZeroTierOne-update.pkg -target / >/tmp/ZeroTierOne-update.log 2>&1
	rm -f /tmp/ZeroTierOne-update.pkg
	exit 0
else
	echo '*** Error self-unpacking update!'
	exit 3
fi

# Do not remove the last line or add a carriage return to it! The installer
# looks for an unterminated line beginning with 16 #'s in itself to find
# the binary blob data, which is appended after it.

################