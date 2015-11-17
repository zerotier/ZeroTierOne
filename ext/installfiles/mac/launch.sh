#!/bin/bash

zthome="/Library/Application Support/ZeroTier/One"
export PATH="$zthome:/bin:/usr/bin:/sbin:/usr/sbin"

# Launch ZeroTier One (not as daemon... launchd monitors it)
exec zerotier-one
