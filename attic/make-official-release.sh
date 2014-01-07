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

if [ "$#" -ne 2 ]; then
	echo "Usage: $0 <path to secret signing identity> <path to destination folder for .nfo and installer>"
	exit 1
fi

secret="$1"
dest="$2"

