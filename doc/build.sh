#!/bin/bash

if [ ! -f zerotier-cli.1.md ]; then
    echo 'This script must be run from the doc/ subfolder of the ZeroTier tree.'
fi

ronn_build() {
    echo "Using ronn"

    ronn -r zerotier-cli.1.md
    ronn -r zerotier-idtool.1.md
    ronn -r zerotier-one.8.md
}

marked_man_build() {
	NODE="$1"
    MARKED_MAN="node_modules/marked-man/bin/marked-man"

    echo "Using marked-man"

    if [ ! -f "$MARKED_MAN.js" ]; then
		echo 'Installing npm package "marked-man" -- MarkDown to ROFF converter...'
        npm install marked-man
	fi

    $NODE "$MARKED_MAN" zerotier-cli.1.md > zerotier-cli.1
    $NODE "$MARKED_MAN" zerotier-idtool.1.md > zerotier-idtool.1
	$NODE "$MARKED_MAN" zerotier-one.8.md > zerotier-one.8
}

command_exists() {
    type "$1" > /dev/null 2>&1
}

rm -f *.1 *.2 *.8
echo "Building man pages..."

# Use 'ronn' which is available as a package on many distros including Debian
if command_exists "ronn"; then
	ronn_build
# Use 'marked-man' from npm
elif command_exists "node"; then
	marked_man_build "node"
elif command_exists "nodejs"; then
    marked_man_build "nodejs"
else
    echo 'Unable to find ronn or node/npm -- cannot build man pages!'
	exit 1
fi
