#!/bin/sh

path=$1
rm -f /usr/bin/zerotier-one  2> /dev/null
rm -f /usr/sbin/zerotier-one  2> /dev/null

rm -f /usr/bin/zerotier-cli  2> /dev/null
rm -f /usr/sbin/zerotier-cli  2> /dev/null

rm -rf $path
