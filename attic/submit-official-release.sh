#!/bin/bash

# Completely useless to outsiders. :)

scp ZeroTierOneInstaller-*-*-*_*_* nyarlathotep.zerotier.com:/www/download.zerotier.com/htdocs

if [ -f 'ZeroTier One.dmg' ]; then
	scp 'ZeroTier One.dmg' nyarlathotep.zerotier.com:/www/download.zerotier.com/htdocs/dist
fi
