#!/bin/bash

set -uo pipefail

trap 'trap " " SIGTERM; kill 0; wait' SIGTERM SIGQUIT SIGINT

echo "Starting Zerotier-One"
zerotier-one -d

echo "Wait for ZT service to come online before attempting queries..."
MAX_WAIT_SECS="${MAX_WAIT_SECS:-90}"
SLEEP_TIME="${SLEEP_TIME:-15}"
if [[ "$SLEEP_TIME" -le 0 ]]
then
  SLEEP_TIME=1
fi

iterations=$((MAX_WAIT_SECS/SLEEP_TIME))
online=false

for ((s=0; s<=iterations; s++))
do
    online="$(zerotier-cli -j info | jq '.online' 2>/dev/null)"
    if [[ "$online" == "true" ]]
    then
        break
    fi
    sleep "$SLEEP_TIME"
    echo " ."
done

if [[ "$online" != "true" ]]
then
    echo "Waited $MAX_WAIT_SECS for zerotier-one to start, exiting." >&2
    exit 1
fi
echo "done."

(
echo "Starting route helper"
while true
do
    if ! NETWORK_LIST="$(zerotier-cli -j listnetworks)"
    then
      echo "Route helper: $NETWORK_LIST" >&2
      exit 1
    fi
    NETWORK_COUNT="$(jq -r '. | length' <<< "$NETWORK_LIST")"
    if [[ "$NETWORK_COUNT" -gt 0 ]]
    then
        for ((j=0; j<=$((NETWORK_COUNT-1)); j++))
        do
            ALLOW_DEFAULT="$(jq -r '.['$j'].allowDefault' <<< "$NETWORK_LIST")"
            ROUTE_COUNT="$(jq -r '.['$j'].routes | length' <<< "$NETWORK_LIST")"
            for ((k=0; k<=$((ROUTE_COUNT-1)); k++))
            do
                ROUTE="$(jq -r '.['$j'].routes['$k'].target' <<< "$NETWORK_LIST")"
                VIA="$(jq -r '.['$j'].routes['$k'].via' <<< "$NETWORK_LIST")"
                if [[ -n "$ROUTE" ]]
                then
                    # check if route is default and allowDefault enabled for this network
                    if [[ "$ROUTE" == "0.0.0.0/0" && "$ALLOW_DEFAULT" == "false" ]]
                    then
                      continue
                    fi
                    EXIST="$(ip -o route show "$ROUTE")"
                    if [[ -z "${EXIST}" && "$VIA" == "null" ]]
                    then
                        IFNAME="$(jq -r '.['$j'] | .portDeviceName' <<< "$NETWORK_LIST")"
                        echo " Adding route $ROUTE to dev $IFNAME"
                        ip route add "$ROUTE" dev "$IFNAME"
                        # Routes will be deleted when ZT brings the interface down
                    fi
                fi
            done
        done
    fi
    sleep 15
done ) &

wait

