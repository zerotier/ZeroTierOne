#!/bin/bash

#
# This probably won't be useful to anyone outside ZeroTier itself.
#

#
# This bumps the revision in version.h, which triggers a build and deploy
# to the now/ subfolder on update.zerotier.com. This allows nodes tracking
# the bleeding edge to track the bleedingest of the bleeding edge.
#

cur_rev=`grep -F ZEROTIER_ONE_VERSION_REVISION version.h | cut -d ' ' -f 3`
next_rev=`expr $cur_rev + 1`

echo Current revision: $cur_rev
echo Next revision: $next_rev

rm -f version.h.new
cat version.h | sed "s/ZEROTIER_ONE_VERSION_REVISION $cur_rev/ZEROTIER_ONE_VERSION_REVISION $next_rev/g" >>version.h.new

new_cur_rev=`grep -F ZEROTIER_ONE_VERSION_REVISION version.h.new | cut -d ' ' -f 3`

if [ "$new_cur_rev" = "$next_rev" ]; then
	mv -f version.h.new version.h
	echo Done.
else
	echo Error: version.h.new updated incorrectly, leaving in place.
	exit 1
fi

exit 0
