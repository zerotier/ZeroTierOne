#!/bin/bash

export PATH=/bin:/usr/bin:/sbin:/usr/sbin
shopt -s expand_aliases

dryRun=0

echo "*** ZeroTier One install/update ***"

if [ "$UID" -ne 0 ]; then
	echo "Not running as root so doing dry run (no modifications to system)..."
	dryRun=1
fi

if [ $dryRun -gt 0 ]; then
	alias ln="echo '>> dry run: ln'"
	alias rm="echo '>> dry run: rm'"
	alias mv="echo '>> dry run: mv'"
	alias cp="echo '>> dry run: cp'"
	alias chown="echo '>> dry run: chown'"
	alias chgrp="echo '>> dry run: chgrp'"
	alias chmod="echo '>> dry run: chmod'"
	alias launchctl="echo '>> dry run: launchctl'"
	alias zerotier-cli="echo '>> dry run: zerotier-cli'"
fi

zthome="/Library/Application Support/ZeroTier/One"
ztapp=`mdfind kMDItemCFBundleIdentifier == 'com.zerotier.ZeroTierOne' | grep -E '.*ZeroTier One[.]app$' | sort | head -n 1`
if [ -z "$ztapp" -o ! -d "$ztapp" ]; then
	ztapp="/Applications/ZeroTier One.app"
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
	echo ">> dry run: tail -c +$blobStart \"$scriptPath\" | bunzip2 -c | tar -xvop -C / -f -"
else
	rm -rf '/tmp/_zt1tmp'
	mkdir '/tmp/_zt1tmp'
	tail -c +$blobStart "$scriptPath" | bunzip2 -c | tar -xop -C '/tmp/_zt1tmp' -f -
fi

cd '/tmp/_zt1tmp'

if [ $dryRun -eq 0 -a ! -d './Applications/ZeroTier One.app' ]; then
	echo 'Archive extraction failed, cannot find zerotier-one binary.'
	exit 2
fi

echo 'Installing zerotier-one service...'

mkdir -p "$zthome"
chown root:admin "$zthome"
chmod 0750 "$zthome"
cp -fa ./Library/Application\ Support/ZeroTier/One/* "$zthome"
chown -R root:wheel "$zthome/tap.kext"
chown -R root:wheel "$zthome/pre10.8/tap.kext"

echo 'Installing/updating ZeroTier One.app...'

if [ ! -z "$ztapp" -a -d "$ztapp" ]; then
	currentAppOwner=`stat -f '%u' "$ztapp"`
	currentAppGroup=`stat -f '%g' "$ztapp"`

	rm -rf "$ztapp"
	mv -f './Applications/ZeroTier One.app' "$ztapp"

	if [ ! -z "$currentAppOwner" -a ! -z "$currentAppGroup" ]; then
		chown -R $currentAppOwner "$ztapp"
		chgrp -R $currentAppGroup "$ztapp"
	fi
else
	# If there is no existing app, just drop the shipped one into place
	mv -f './Applications/ZeroTier One.app' "/Applications/ZeroTier One.app"
fi

# Set up symlink that watches for app deletion
rm -f "$zthome/shutdownIfUnreadable"
ln -sf "$ztapp/Contents/Info.plist" "$zthome/shutdownIfUnreadable"

echo 'Installing zerotier-cli command line utility...'

ln -sf "/Library/Application Support/ZeroTier/One/zerotier-one" /usr/bin/zerotier-cli

if [ ! -f '/Library/Application Support/ZeroTier/One/authtoken.secret' ]; then
	echo 'Pre-creating authtoken.secret for ZeroTier service...'
	if [ $dryRun -eq 0 ]; then
		rm -f '/Library/Application Support/ZeroTier/One/authtoken.secret'
		head -c 1024 /dev/urandom | md5 | head -c 24 >'/Library/Application Support/ZeroTier/One/authtoken.secret'
		chmod 0600 '/Library/Application Support/ZeroTier/One/authtoken.secret'
	fi
fi

echo 'Installing and (re-)starting zerotier-one service via launchctl...'

mv -f './Library/LaunchDaemons/com.zerotier.one.plist' '/Library/LaunchDaemons/'
if [ ! -z "`launchctl list | grep -F com.zerotier.one`" ]; then
	launchctl unload /Library/LaunchDaemons/com.zerotier.one.plist
fi
launchctl load /Library/LaunchDaemons/com.zerotier.one.plist

sleep 1
/usr/bin/zerotier-cli info

cd /tmp
rm -rf _zt1tmp

exit 0

# Do not remove the last line or add a carriage return to it! The installer
# looks for an unterminated line beginning with 16 #'s in itself to find
# the binary blob data, which is appended after it.

################