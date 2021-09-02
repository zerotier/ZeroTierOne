#!/bin/bash
export PATH="/Library/Application Support/ZeroTier/One:/bin:/usr/bin:/sbin:/usr/sbin"
/usr/bin/killall MacEthernetTapAgent >>/dev/null 2>&1
exec zerotier-one
