#!/bin/bash

zthome="/Library/Application Support/ZeroTier/One"
export PATH="/bin:/usr/bin:/sbin:/usr/sbin:$zthome"
ztapp=`mdfind kMDItemCFBundleIdentifier == 'com.zerotier.ZeroTierOne' | grep -E '.+[.]app$' | sort | head -n 1`

# Clean all other stuff off the system if the user has trashed the .app
if [ -z "$ztapp" -o ! -d "$ztapp" ]; then
	# Double-check default location just in case there is some issue with mdfind
	ztapp="/Applications/ZeroTier One.app"
	if [ ! -d "$ztapp" ]; then
		if [ -e "$zthome/uninstall.sh" ]; then
			cd "$zthome"
			nohup "$zthome/uninstall.sh" >>/tmp/ZeroTierOneUninstall.log 2>&1 &
			disown %1
			exit 0
		fi
	fi
fi

# Create the app deletion notification symlink if it does
# not already exist.
shutdownIfUnreadablePointsTo=`readlink "$zthome/shutdownIfUnreadable"`
if [ -z "$shutdownIfUnreadablePointsTo" -o "$shutdownIfUnreadablePointsTo" != "$ztapp/Contents/Info.plist" ]; then
	rm -f "$zthome/shutdownIfUnreadable"
	ln -sf "$ztapp/Contents/Info.plist" "$zthome/shutdownIfUnreadable"
fi

# Launch ZeroTier One (not as daemon... launchd monitors it)
exec zerotier-one
