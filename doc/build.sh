#!/bin/bash

export PATH=/bin:/usr/bin:/usr/local/bin:/sbin:/usr/sbin:/usr/local/sbin

if [ ! -f zerotier-cli.1.md ]; then
	echo 'This script must be run from the doc/ subfolder of the ZeroTier tree.'
fi

if [ ! -f node_modules/marked-man/bin/marked-man ]; then
	echo 'Installing MarkDown to ROFF converter...'
	npm install marked-man
	echo
fi

rm -f *.1 *.8

NODE=/usr/bin/node
if [ -e /usr/bin/nodejs ]; then
	NODE=/usr/bin/nodejs
fi

$NODE node_modules/marked-man/bin/marked-man zerotier-cli.1.md >zerotier-cli.1
$NODE node_modules/marked-man/bin/marked-man zerotier-idtool.1.md >zerotier-idtool.1
$NODE node_modules/marked-man/bin/marked-man zerotier-one.8.md >zerotier-one.8

exit 0
