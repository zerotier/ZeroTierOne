#!/bin/bash

zerotier-one -d

# Wait for ZT service to come online before attempting queries
sleep 15

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
                    # Routes will be deleted when ZT brings the interface down
                fi
            done
        done
        sleep 15
    fi
done
