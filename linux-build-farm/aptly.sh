#!/bin/bash

# This builds a series of Debian repositories for each distribution.

export PATH=/bin:/usr/bin:/usr/local/bin:/sbin:/usr/sbin

for distro in debian-* ubuntu-*; do
	if [ -n "`find ${distro} -name '*.deb' -type f`" ]; then
		arches=`ls ${distro}/*.deb | cut -d _ -f 3 | cut -d . -f 1 | xargs | sed 's/ /,/g'`
		distro_name=`echo $distro | cut -d '-' -f 2`
		echo '---' $distro / $distro_name / $arches
		aptly repo create -architectures=${arches} -comment="ZeroTier, Inc. Debian Packages" -component="main" -distribution=${distro_name} zt-release-${distro_name}
		aptly repo add zt-release-${distro_name} ${distro}/*.deb
		aptly publish repo zt-release-${distro_name} $distro_name
	fi
done
