#!/bin/bash

export PATH=/bin:/usr/bin:/usr/local/bin:/sbin:/usr/sbin:/


# --- Test Parameters ---
test_namefile=$(ls *.name)
test_name="${test_namefile%.*}" # test network id
nwconf=$(ls *.conf) # blank test network config file
nwid="${nwconf%.*}" # test network id
netcon_wait_time=25 # wait for test container to come online
app_timeout_time=15 # app-specific timeout
file_path=/opt/results/ # test result output file path (fs shared between host and containers)
file_base="$test_name".txt # test result output file
fail=FAIL. # appended to result file in event of failure
ok=OK. # appended to result file in event of success
tmp_ext=.tmp # temporary filetype used for sharing test data between containers
address_file="$file_path$test_name"_addr"$tmp_ext" # file shared between host and containers for sharing address (optional)


# --- Network Config ---
echo '*** ZeroTier Network Containers Test Monitor'
chown -R daemon /var/lib/zerotier-one
chgrp -R daemon /var/lib/zerotier-one
su daemon -s /bin/bash -c '/zerotier-one -d -U -p9993 >>/tmp/zerotier-one.out 2>&1'
virtip4=""
while [ -z "$virtip4" ]; do
	sleep 0.2
	virtip4=`/zerotier-cli listnetworks | grep -F $nwid | cut -d ' ' -f 9 | sed 's/,/\n/g' | grep -F '.' | cut -d / -f 1`
done
echo '*** Starting Test...'
echo '*** Up and running at' $virtip4 ' on network: ' $nwid
echo '*** Sleeping for ('  "$netcon_wait_time"  's ) while we wait for the Network Container to come online...'
sleep "$netcon_wait_time"s
ncvirtip=$(<$address_file)


# --- Test section ---
echo '*** Running lua script against redis host at' $ncvirtip
redis-cli -h $ncvirtip EVAL "$(cat hello.lua)" 0 > redis_response.txt
response_string=$(<redis_response.txt)

if [[ $response_string == *"welcome to the machine!"* ]]
then
	echo 'REDIS RESPONSE OK'
	touch "$file_path$ok$test_name.txt"
	printf 'Test: redis-server responded!\n' >> "$file_path$ok$test_name.txt"
else
	echo 'REDIS RESPONSE FAIL'
	touch "$file_path$fail$test_name.txt"
	printf 'Test: redis server did NOT respond!\n' >> "$file_path$fail$test_name.txt"
fi




