#!/bin/bash

#
# This script signs an installer, creates its .NFO file, and
# copies it to a destination directory. This in turn can be
# uploaded to the official ZeroTier Networks update site for
# auto-updating binary distribution users.
#
# It's in attic/ because end-users won't find it particularly
# useful. You must have the ZeroTier One official signing
# identity secret keys to sign binary releases for auto-update,
# and of course you'd also have to upload it to our servers.
#

# Build the app and the installer, then run this from the root
# of the source tree. It'll need the zerotier-idtool symlink
# that will be placed there after a build, too.

export PATH=/bin:/usr/bin:/sbin:/usr/sbin

if [ "$#" -ne 1 ]; then
	echo "Usage: $0 <path to secret signing identity>"
	exit 1
fi

if [ ! -e zerotier-idtool ]; then
	echo "Unable to find zerotier-idtool in current directory."
	exit 1
fi

secret="$1"

if [ ! -e "$secret" ]; then
	echo "Can't find $secret"
	exit 1
fi

rm -f *.nfo

for inst in `ls ZeroTierOneInstaller-*-*-*_*_*`; do
	echo "Found installer: $inst"
	nfo="$inst.nfo"
	rm -f "$nfo"
	echo "tss=`date`" >>"$nfo"
	echo "vMajor=`echo $inst | cut -d - -f 4 | cut -d _ -f 1`" >>"$nfo"
	echo "vMinor=`echo $inst | cut -d - -f 4 | cut -d _ -f 2`" >>"$nfo"
	echo "vRevision=`echo $inst | cut -d - -f 4 | cut -d _ -f 3`" >>"$nfo"
	echo "signedBy=`cat $secret | cut -d : -f 1`" >>"$nfo"
	echo "ed25519=`./zerotier-idtool sign $secret $inst`" >>"$nfo"
	echo "url=http://download.zerotier.com/$inst" >>"$nfo"
done
