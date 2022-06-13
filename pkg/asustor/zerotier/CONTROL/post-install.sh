#!/bin/sh

APKG_PKG_DIR=/usr/local/AppCentral/zerotier

case "$APKG_PKG_STATUS" in

	install)
		modprobe tun
		mkdir -p /usr/local/bin
		mv ${APKG_PKG_DIR}/bin/zerotier-one.${AS_NAS_ARCH} ${APKG_PKG_DIR}/bin/zerotier-one
		ln -s ${APKG_PKG_DIR}/bin/zerotier-one /usr/local/bin/zerotier-cli
		ln -s ${APKG_PKG_DIR}/bin/zerotier-one /usr/local/bin/zerotier-idtool
		ln -s $APKG_PKG_DIR/data /var/lib/zerotier-one
		;;
	upgrade)
		# post upgrade script here (restore data)
		# cp -af $APKG_TEMP_DIR/* $APKG_PKG_DIR/etc/.
		;;
	*)
		;;

esac

exit 0
