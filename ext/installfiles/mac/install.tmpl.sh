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
	alias chown="echo '>> dry run: chown'"
	alias chgrp="echo '>> dry run: chgrp'"
	alias launchctl="echo '>> dry run: launchctl'"
	alias zerotier-cli="echo '>> dry run: zerotier-cli'"
fi

zthome="/Library/Application Support/ZeroTier/One"
ztapp=`mdfind kMDItemCFBundleIdentifier == 'com.zerotier.ZeroTierOne' | grep -E '.+[.]app$' | sort | head -n 1`
if [ ! -d "$ztapp" ]; then
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
	tail -c +$blobStart "$scriptPath" | bunzip2 -c | tar -xvop -C / -f -
fi

if [ $dryRun -eq 0 -a ! -d "/Applications/ZeroTier One_app.LATEST" ]; then
	echo 'Archive extraction failed, cannot find zerotier-one binary.'
	exit 2
fi

echo 'Installing/updating ZeroTier One.app...'

if [ -d "$ztapp" ]; then
	# Preserve ownership of existing .app and install new version in the
	# same location.
	currentAppOwner=`stat -f '%u' "$ztapp"`
	currentAppGroup=`stat -f '%g' "$ztapp"`
	if [ ! -z "$currentAppOwner" -a ! -z "$currentAppGroup" ]; then
		rm -rf "$ztapp"
		mv -f "/Application/ZeroTier One_app.LATEST" "$ztapp"
		chown -R $currentAppOwner "$ztapp"
		chgrp -R $currentAppGroup "$ztapp"
	else
		rm -rf "$ztapp"
		mv -f "/Application/ZeroTier One_app.LATEST" "$ztapp"
	fi
else
	# If there is no existing app, just drop the shipped one into place
	mv -f "/Applications/ZeroTier One_app.LATEST" "/Applications/ZeroTier One.app"
fi

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

if [ ! -z "`launchctl list | grep -F com.zerotier.one`" ]; then
	launchctl unload /Library/LaunchDaemons/com.zerotier.one.plist
fi
launchctl load /Library/LaunchDaemons/com.zerotier.one.plist

sleep 1
zerotier-cli info

exit 0

# Do not remove the last line or add a carriage return to it! The installer
# looks for an unterminated line beginning with 16 #'s in itself to find
# the binary blob data, which is appended after it.

################