#!/bin/bash

make -j 4 one file2lz4c

if [ ! -f file2lz4c ]; then
	echo "Build of file2lz4c utility failed, aborting installer build."
	exit 2
fi

if [ ! -f zerotier-one ]; then
	echo "Build of zerotier-one failed, aborting installer build."
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

echo "Packaging common files: zerotier-one"

rm -rf installer-build
mkdir installer-build

./file2lz4c zerotier-one zerotier_one >installer-build/zerotier_one.h

case "$system" in

	Linux)
		case "$machine" in
			i386|i486|i586|i686)
				machine="x86"
				;;
			x86_64|amd64|x64)
				machine="x64"
				;;
			*)
				echo "Unknonwn machine type: $machine"
				exit 2
		esac
		echo "Assembling Linux installer for $machine and ZT1 version $vmajor.$vminor.$revision"

		./file2lz4c installer/linux/uninstall.sh uninstall_sh >installer-build/uninstall_sh.h
		./file2lz4c installer/linux/init.d/zerotier-one linux__init_d__zerotier_one >installer-build/linux__init_d__zerotier_one.h

		ls -l installer-build

		g++ -Os -o "zt1-${vmajor}_${vminor}_${revision}-linux-${machine}-install" installer.cpp ext/lz4/lz4.o ext/lz4/lz4hc.o

		;;

	Darwin)
		echo "Assembling OSX installer for x86/x64 (combined) and ZT1 version $vmajor.$vminor.$revision"

		;;

	*)
		echo "Unsupported platform: $system"
		exit 2

esac

exit 0
