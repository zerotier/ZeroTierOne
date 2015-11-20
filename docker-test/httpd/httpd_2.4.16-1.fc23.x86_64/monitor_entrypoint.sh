#!/bin/bash

# Parameters for test
test_name=httpd_bigfile
nwid=e5cd7a9e1c5311ab # test network
netcon_wait_time=45 # wait for test container to come online
app_timeout_time=10 # app-specific timeout
file_path=/opt/results/ # test result output file path (fs shared between host and containers)
file_base="$test_name".txt # test result output file
fail=FAIL. # appended to result file in event of failure
ok=OK. # appended to result file in event of success
address_file="$file_path$test_name"_addr.txt # file shared between host and containers for sharing address (optional)
tmp=/test/ # for storage of large transfer test files, things that shouldn't be shared with host
bigfile=bigfile # large, random test transfer file

export PATH=/bin:/usr/bin:/usr/local/bin:/sbin:/usr/sbin:/

echo '*** ZeroTier Network Containers Test Monitor'
chown -R daemon /var/lib/zerotier-one
chgrp -R daemon /var/lib/zerotier-one
su daemon -s /bin/bash -c '/zerotier-one -d -U -p9993 >>/tmp/zerotier-one.out 2>&1'
echo '*** Waiting for initial identity generation...'

while [ ! -s /var/lib/zerotier-one/identity.secret ]; do
	sleep 0.2
done

echo '*** Waiting for network config...'

virtip4=""
while [ ! -s /var/lib/zerotier-one/networks.d/"$nwid".conf ]; do
	sleep 0.2
done
while [ -z "$virtip4" ]; do
	sleep 0.2
	virtip4=`/zerotier-cli listnetworks | grep -F $nwid | cut -d ' ' -f 9 | sed 's/,/\n/g' | grep -F '.' | cut -d / -f 1`
done

echo '*** Starting Test...'
echo '*** Up and running at' $virtip4
echo '*** Sleeping for 30s while we wait for the Network Container to come online...'
sleep "$netcon_wait_time"s
ncvirtip=$(<$address_file)
echo '*** Curling from intercepted server at' $ncvirtip
rm -rf "$file_path"*."$file_base"
touch "$tmp$bigfile"

# Perform test
# curl --connect-timeout "$app_timeout_time" -v -o "$file_path$file_base" http://"$ncvirtip"/index.html

# Large transfer test
mkdir -p "$tmp"
curl --connect-timeout "$app_timeout_time" -v -o "$tmp$bigfile" http://"$ncvirtip"/"$bigfile"

# Test outcome logic
minimumsize=39550
actualsize=$(wc -c <"$tmp$bigfile")
if [ $actualsize -ge $minimumsize ]; then
	echo 'OK'
	touch "$file_path$ok$file_base"
    #mv "$file_path$file_base" "$file_path$ok$file_base"
else
	echo 'FAIL'
	touch "$file_path$fail$file_base"
    #mv "$file_path$file_base" "$file_path$fail$file_base"
fi