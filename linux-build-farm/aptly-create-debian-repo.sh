#!/bin/bash

# This builds a series of Debian repositories for each distribution.

export PATH=/bin:/usr/bin:/usr/local/bin:/sbin:/usr/sbin

for distro in debian-* ubuntu-*; do
	if [ -n "`find ${distro} -name '*.deb' -type f`" ]; then
		arches=`ls ${distro}/*.deb | cut -d _ -f 3 | cut -d . -f 1 | xargs | sed 's/ /,/'`
		aptly repo create -architectures=${arches} -comment="ZeroTier, Inc. Debian Packages" -component="main" -distribution=${distro} zt-release-${distro}
		aptly repo add zt-release-${distro} ${distro}/*.deb
		aptly publish repo zt-release-${distro} ${distro}
	fi
done
