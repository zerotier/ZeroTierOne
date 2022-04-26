#!/bin/sh

. /etc/script/lib/command.sh

APKG_PKG_DIR=/usr/local/AppCentral/zerotier

case $1 in

	start)
		modprobe tun
		# start script here
		$APKG_PKG_DIR/bin/zerotier-one $APKG_PKG_DIR/data -d
		;;

	stop)
		# stop script here
		pkill zerotier
		;;

	*)
		echo "usage: $0 {start|stop}"
		exit 1
		;;
esac

exit 0
