#!/bin/bash

export PATH=/bin:/usr/bin:/usr/local/bin:/sbin:/usr/sbin:/


# --- Test Parameters ---
test_namefile=$(ls *.name)
test_name="${test_namefile%.*}" # test network id
nwconf=$(ls *.conf) # blank test network config file
nwid="${nwconf%.*}" # test network id
netcon_wait_time=35 # wait for test container to come online
app_timeout_time=25 # app-specific timeout
file_path=/opt/results/ # test result output file path (fs shared between host and containers)
file_base="$test_name".txt # test result output file
fail=FAIL. # appended to result file in event of failure
ok=OK. # appended to result file in event of success
tmp_ext=.tmp # temporary filetype used for sharing test data between containers
address_file="$file_path$test_name"_addr"$tmp_ext" # file shared between host and containers for sharing address (optional)
bigfile_name=bigfile # large, random test transfer file
rx_md5sumfile="$file_path"rx_"$bigfile_name"_md5sum"$tmp_ext"
tx_md5sumfile="$file_path"tx_"$bigfile_name"_md5sum"$tmp_ext"


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
echo '*** Curling from intercepted server at' $ncvirtip
rm -rf "$file_path"*."$file_base"
touch "$bigfile_name"

# Perform test
# curl --connect-timeout "$app_timeout_time" -v -o "$file_path$file_base" http://"$ncvirtip"/index.html
# Large transfer test
curl --connect-timeout "$app_timeout_time" -v -o "$bigfile_name" http://"$ncvirtip"/"$bigfile_name"

# Check md5
md5sum < "$bigfile_name" > "$rx_md5sumfile"
rx_md5sum=$(<$rx_md5sumfile)
tx_md5sum=$(<$tx_md5sumfile)

echo '*** Comparing md5: ' "$rx_md5sum" ' and ' "$tx_md5sum"

if [ "$rx_md5sum" != "$tx_md5sum" ]; 
then
	echo 'MD5 FAIL'
	touch "$file_path$fail$test_name.txt"
	printf 'Test: md5 sum did not match!\n' >> "$file_path$fail$test_name.txt"
else
	echo 'MD5 OK'
	touch "$file_path$ok$test_name.txt"
	printf 'Test: md5 sum ok!\n' >> "$file_path$ok$test_name.txt"
	cat "$rx_md5sumfile" >> "$file_path$ok$test_name.txt"
	cat "$tx_md5sumfile" >> "$file_path$ok$test_name.txt"
fi











