#!/bin/sh
CONF=/etc/config/qpkg.conf
QPKG_NAME="zerotier"
QPKG_ROOT=`/sbin/getcfg $QPKG_NAME Install_Path -f ${CONF}`
APACHE_ROOT=/share/`/sbin/getcfg SHARE_DEF defWeb -d Qweb -f /etc/config/def_share.info`
case "$1" in
  start)
    modprobe tun
    ln -s $QPKG_ROOT/zerotier-one /usr/sbin/zerotier-cli
    ln -s $QPKG_ROOT/zerotier-one /usr/bin/zerotier-cli
    ln -s $QPKG_ROOT /var/lib/zerotier-one
    ENABLED=$(/sbin/getcfg $QPKG_NAME Enable -u -d FALSE -f $CONF)
    if [ "$ENABLED" != "TRUE" ]; then
        echo "$QPKG_NAME is disabled."
        exit 1
    fi
    $QPKG_ROOT/zerotier-one $QPKG_ROOT -d
    ;;

  stop)
    killall zerotier-one
    ;;

  restart)
    $0 stop
    $0 start
    ;;

  *)
    echo "Usage: $0 {start|stop|restart}"
    exit 1
esac

exit 0
