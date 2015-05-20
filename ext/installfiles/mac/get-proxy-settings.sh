#!/bin/bash

# Outputs host and port for system HTTP proxy or zeroes if none or not
# configured.

export PATH=/bin:/usr/bin:/usr/sbin:/sbin:/usr/local/bin:/usr/local/sbin

enabled=`system_profiler SPNetworkDataType|grep "HTTP Proxy Enabled"|awk {'sub(/^.*:[ \t]*/, "", $0); print $0;'} 2>/dev/null`
port=`system_profiler SPNetworkDataType|grep "HTTP Proxy Port"|awk {'sub(/^.*:[ \t]*/, "", $0); print $0;'} 2>/dev/null`
serv=`system_profiler SPNetworkDataType|grep "HTTP Proxy Server"|awk {'sub(/^.*:[ \t]*/, "", $0); print $0;'} 2>/dev/null`

if [ "$enabled" = "Yes" ]; then
	if [ "$serv" ]; then
		if [ ! "$port" ]; then
			port=80
		fi

		echo $serv $port
	else
		echo 0.0.0.0 0
	fi
else
	echo 0.0.0.0 0
fi

exit 0
