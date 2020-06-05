#!/bin/bash

export PATH=/bin:/usr/bin:/usr/local/bin:/sbin:/usr/sbin:/usr/local/sbin

if [ ! -f zerotier-cli.1.md ]; then
	echo 'This script must be run from the doc/ subfolder of the ZeroTier tree.'
fi

rm -f *.1 *.2 *.8

if [ -e /usr/bin/ronn -o -e /usr/local/bin/ronn ]; then
	# Use 'ronn' which is available as a package on many distros including Debian
	ronn -r zerotier-cli.1.md
	ronn -r zerotier-idtool.1.md
	ronn -r zerotier-one.8.md
else
	# Use 'marked-man' from npm
	NODE=/usr/bin/node
	if [ ! -e $NODE ]; then
		if [ -e /usr/bin/nodejs ]; then
			NODE=/usr/bin/nodejs
		elif [ -e /usr/local/bin/node ]; then
			NODE=/usr/local/bin/node
		elif [ -e /usr/local/bin/nodejs ]; then
			NODE=/usr/local/bin/nodejs
		else
			echo 'Unable to find ronn or node/npm -- cannot build man pages!'
			exit 1
		fi
	fi

	if [ ! -f node_modules/marked-man/bin/marked-man ]; then
		echo 'Installing npm package "marked-man" -- MarkDown to ROFF converter...'
		npm install marked-man
	fi

	$NODE node_modules/marked-man/bin/marked-man zerotier-cli.1.md >zerotier-cli.1
	$NODE node_modules/marked-man/bin/marked-man zerotier-idtool.1.md >zerotier-idtool.1
	$NODE node_modules/marked-man/bin/marked-man zerotier-one.8.md >zerotier-one.8
fi

exit 0
