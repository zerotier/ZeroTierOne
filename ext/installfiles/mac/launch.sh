#!/bin/bash

zthome="/Library/Application Support/ZeroTier/One"
export PATH="/bin:/usr/bin:/sbin:/usr/sbin:$zthome"

# If the app has been deleted, uninstall the service
cd "$zthome"
if [ -L './shutdownIfUnreadable' -a ! -f "`readlink ./shutdownIfUnreadable`" ]; then
	rm -f /tmp/ZeroTierOneUninstall.log
	/bin/bash "$zthome/uninstall.sh" >/tmp/ZeroTierOneUninstall.log 2>&1
	exit 0
fi

# Launch ZeroTier One (not as daemon... launchd monitors it)
exec zerotier-one
