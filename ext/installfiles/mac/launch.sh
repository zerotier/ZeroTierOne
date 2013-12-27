#!/bin/bash

zthome="/Library/Application Support/ZeroTier/One"
ztapp="/Applications/ZeroTier One.app"

export PATH="/bin:/usr/bin:/sbin:/usr/sbin:$zthome"

# Uninstall if the .app has been thrown away
if [ ! -d "$ztapp" ]; then
	if [ -e "$zthome/uninstall.sh" ]; then
		cd "$zthome"
		./uninstall.sh -q
		exit
	fi
fi

# Create the app deletion notification symlink if it does
# not already exist.
if [ ! -L "$zthome/shutdownIfUnreadable" ]; then
	rm -f "$zthome/shutdownIfUnreadable"
	ln -sf "$ztapp/Contents/Info.plist" "$zthome/shutdownIfUnreadable"
fi

# Launch ZeroTier One (not as daemon... launchd monitors it)
exec zerotier-one
