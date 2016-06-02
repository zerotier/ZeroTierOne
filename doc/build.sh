#!/bin/bash

export PATH=/bin:/usr/bin:/usr/local/bin:/sbin:/usr/sbin:/usr/local/sbin

if [ ! -f zerotier-cli.1.md ]; then
	echo 'This script must be run from the doc/ subfolder of the ZeroTier tree.'
fi

if [ ! -f node_modules/ronn/bin/ronn.js ]; then
	echo 'Installing MarkDown to ROFF converter...'
	npm install ronn
	echo
fi

rm -f zerotier-cli.1 *.roff

node node_modules/ronn/bin/ronn.js --build --roff zerotier-cli.1.md
if [ -f zerotier-cli.1.roff ]; then
	mv zerotier-cli.1.roff zerotier-cli.1
else
	echo 'Conversion of zerotier-cli.1.md failed!'
	exit 1
fi

exit 0
