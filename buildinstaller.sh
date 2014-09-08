#!/bin/bash

# This script builds the installer for *nix systems. Windows must do everything
# completely differently, as usual.

export PATH=/bin:/usr/bin:/sbin:/usr/sbin

if [ ! -f zerotier-one ]; then
	echo "Could not find 'zerotier-one' binary, please build before running this script."
	exit 2
fi

machine=`uname -m`
system=`uname -s`

vmajor=`cat version.h | grep -F ZEROTIER_ONE_VERSION_MAJOR | cut -d ' ' -f 3`
vminor=`cat version.h | grep -F ZEROTIER_ONE_VERSION_MINOR | cut -d ' ' -f 3`
revision=`cat version.h | grep -F ZEROTIER_ONE_VERSION_REVISION | cut -d ' ' -f 3`

if [ -z "$vmajor" -o -z "$vminor" -o -z "$revision" ]; then
	echo "Unable to extract version info from version.h, aborting installer build."
	exit 2
fi

rm -rf build-installer
mkdir build-installer

case "$system" in

	Linux)
		# Canonicalize $machine for some architectures... we use x86
		# and x64 for Intel stuff. ARM and others should be fine if
		# we ever ship officially for those.
		case "$machine" in
			i386|i486|i586|i686)
				machine="x86"
				;;
			x86_64|amd64|x64)
				machine="x64"
				;;
			armv6l)
				machine="arm"
				;;
			*)
				echo "Unsupported machine type: $machine"
				exit 2
		esac

		echo "Assembling Linux installer for $machine and version $vmajor.$vminor.$revision"

		mkdir -p 'build-installer/var/lib/zerotier-one'
		cp -fp 'ext/installfiles/linux/uninstall.sh' 'build-installer/var/lib/zerotier-one'
		cp -fp 'zerotier-one' 'build-installer/var/lib/zerotier-one'
		mkdir -p 'build-installer/tmp'
		cp -fp 'ext/installfiles/linux/init.d/zerotier-one' 'build-installer/tmp/init.d_zerotier-one'
		cp -fp 'ext/installfiles/linux/systemd/zerotier-one.service' 'build-installer/tmp/systemd_zerotier-one.service'

		targ="ZeroTierOneInstaller-linux-${machine}-${vmajor}_${vminor}_${revision}"
		# Use gzip in Linux since some minimal Linux systems do not have bunzip2
		rm -f build-installer-tmp.tar.gz
		cd build-installer
		tar -cf - * | gzip -9 >../build-installer-tmp.tar.gz
		cd ..
		rm -f $targ
		cat ext/installfiles/linux/install.tmpl.sh build-installer-tmp.tar.gz >$targ
		chmod 0755 $targ
		rm -f build-installer-tmp.tar.gz
		ls -l $targ

		;;

	Darwin)
		echo "Assembling mac installer for x86/x64 (combined) version $vmajor.$vminor.$revision"

		mkdir -p 'build-installer/Applications'
		cp -a 'build-ZeroTierUI-release/ZeroTier One.app' 'build-installer/Applications'
		mkdir -p 'build-installer/Library/Application Support/ZeroTier/One'
		cp -fp 'ext/installfiles/mac/uninstall.sh' 'build-installer/Library/Application Support/ZeroTier/One'
		cp -fp 'ext/installfiles/mac/launch.sh' 'build-installer/Library/Application Support/ZeroTier/One'
		cp -fp 'zerotier-one' 'build-installer/Library/Application Support/ZeroTier/One'
		cp -fRp ext/bin/tap-mac/* 'build-installer/Library/Application Support/ZeroTier/One'
		mkdir -p 'build-installer/Library/LaunchDaemons'
		cp -fp 'ext/installfiles/mac/com.zerotier.one.plist' 'build-installer/Library/LaunchDaemons'

		targ="ZeroTierOneInstaller-mac-combined-${vmajor}_${vminor}_${revision}"
		rm -f build-installer-tmp.tar.bz2
		cd build-installer
		find . -type f -name .DS_Store -print0 | xargs -0 rm -f
		tar -cf - * | bzip2 -9 >../build-installer-tmp.tar.bz2
		cd ..
		rm -f $targ
		cat ext/installfiles/mac/install.tmpl.sh build-installer-tmp.tar.bz2 >$targ
		chmod 0755 $targ
		rm -f build-installer-tmp.tar.bz2
		ls -l $targ

		;;

	*)
		echo "Unsupported platform: $system"
		exit 2

esac

exit 0
