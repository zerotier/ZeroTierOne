#!/bin/sh

install_path=$1

ln -s $install_path/bin/zerotier-one /usr/sbin/zerotier-one
ln -s $install_path/bin/zerotier-one /usr/sbin/zerotier-cli
ln -s $install_path/bin/zerotier-one /usr/bin/zerotier-one
ln -s $install_path/bin/zerotier-one /usr/bin/zerotier-cli

ln -s $install_path/ /var/lib/zerotier-one

APKG_PATH=$(readlink -f $1)
WEBPATH="/var/www/apps/zerotier/"
mkdir -p $WEBPATH
ln -sf ${APKG_PATH}/web/* $WEBPATH