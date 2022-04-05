#!/bin/sh

if [ "${SYNOPKG_DSM_VERSION_MAJOR}" -eq "6" ]; then
    PKGDIR="/var/packages/zerotier/var"
else
    PKGDIR="${SYNOPKG_PKGVAR}"
fi

ZTO_PID_FILE="${PKGDIR}/zerotier-one.pid"
WAT_PID_FILE="${PKGDIR}/zerotier-watchdog.pid"
ZTO_LOG_FILE="${PKGDIR}/zerotier-one.log"

log()
{
    local timestamp=$(date --iso-8601=second)
    echo "${timestamp} $1" >> ${ZTO_LOG_FILE}
}

configure_tun()
{
    log "Checking for TUN device"
    # Create /dev/net/tun if needed
    if ( [ ! -c /dev/net/tun ] ); then
        if ( [ ! -d /dev/net ] ); then
            mkdir -m 755 /dev/net
        fi
        log "Adding TUN device"
        mknod /dev/net/tun c 10 200
        chmod 0755 /dev/net/tun
    fi

    # Load TUN kernel module
    if ( !( lsmod | grep -q "^tun\s" ) ); then
        log "Loading TUN kernel module"
        insmod /lib/modules/tun.ko
    fi
}

configure_cli()
{
    # Create ZT CLI symlinks if needed
    mkdir -p /usr/local/bin/
    ln -s ${SYNOPKG_PKGDEST}/bin/zerotier-one /usr/local/bin/zerotier-cli
    ln -s ${SYNOPKG_PKGDEST}/bin/zerotier-one /usr/local/bin/zerotier-idtool
}

apply_routes()
{
    echo $BASHPID >> ${WAT_PID_FILE}
    log "Started Watchdog ($(cat $WAT_PID_FILE))"

    # Wait for ZT service to come online before attempting queries
    sleep 15

    # Loop until killed, every two minutes check for required routes and add if needed
    while true
    do
        NETWORK_COUNT=$(zerotier-cli -j listnetworks | jq -r '. | length')
        if [ "$NETWORK_COUNT" -gt 0 ]; then
            for ((j=0; j<=$((NETWORK_COUNT-1)); j++))
            do
                ROUTE_COUNT=$(zerotier-cli -j listnetworks | jq -r '.['$j'].routes | length')
                for ((k=0; k<=$((ROUTE_COUNT-1)); k++))
                do
                    ROUTE=$(zerotier-cli -j listnetworks | jq -r '.['$j'].routes['$k'].target')
                    EXIST=$(ip route show $ROUTE | wc -l)
                    if [ $EXIST -eq 0 ];
                    then
                        IFNAME=$(zerotier-cli -j listnetworks | jq -r '.['$j'] | .portDeviceName')
                        ip route add $ROUTE dev $IFNAME
                        log "Added route $ROUTE to dev $IFNAME"
                        # Routes will be deleted when ZT brings the interface down
                    fi
                done
            done
            sleep 15
        fi
    done
}

configure_routes()
{
    if [ -r "${WAT_PID_FILE}" ]; then
        exit 0
    else
        apply_routes &
    fi
}

start_daemon()
{
    ${SYNOPKG_PKGDEST}/bin/zerotier-one -d
    echo $(pidof zerotier-one) > ${ZTO_PID_FILE}
    log "Started ZeroTier ($(cat $ZTO_PID_FILE))"
}

stop_daemon() {
    if [ -r "${ZTO_PID_FILE}" ]; then
        local ZTO_PID=$(cat "${ZTO_PID_FILE}")
        log "Stopped ZeroTier ($(cat $ZTO_PID_FILE))"
        kill -TERM $ZTO_PID
        wait_for_status 1 || kill -KILL $PID >> ${LOG_FILE} 2>&1
        rm -f $ZTO_PID_FILE > /dev/null
    fi
    if [ -r "${WAT_PID_FILE}" ]; then
        local WAT_PID=$(cat "${WAT_PID_FILE}")
        log "Stopped Watchdog ($(cat $WAT_PID_FILE))"
        kill -TERM $WAT_PID
        rm -f $WAT_PID_FILE > /dev/null
    fi
}

daemon_status()
{
    if [ -f ${ZTO_PID_FILE} ] && kill -0 `cat ${ZTO_PID_FILE}` > /dev/null 2>&1; then
        return
    fi
    rm -f ${ZTO_PID_FILE}
    return 1
}

wait_for_status()
{
    counter=$2
    while [ ${counter} -gt 0 ]; do
        daemon_status
        [ $? -eq $1 ] && return
        let counter=counter-1
        sleep 1
    done
    return 1
}

case "$1" in
  start)
    if ( pidof zerotier-one ); then
        exit 0
    else
        configure_tun
        configure_cli
        start_daemon
        configure_routes
    fi
    ;;
  stop)
    if ( pidof zerotier-one ); then
        stop_daemon
    else
        exit 0
    fi
    ;;
  status)
    if ( pidof zerotier-one ); then
        exit 0
    else
        exit 1
    fi
    ;;
  *)
    exit 1
    ;;
esac

exit 0
