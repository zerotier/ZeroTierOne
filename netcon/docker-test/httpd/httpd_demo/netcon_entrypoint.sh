#!/bin/bash

export PATH=/bin:/usr/bin:/usr/local/bin:/sbin:/usr/sbin:/


# --- Test Parameters ---
test_namefile=$(ls *.name)
test_name="${test_namefile%.*}" # test network id
nwconf=$(ls *.conf) # blank test network config file
nwid="${nwconf%.*}" # test network id
file_path=/opt/results/ # test result output file path (fs shared between host and containers)
file_base="$test_name".txt # test result output file
tmp_ext=.tmp # temporary filetype used for sharing test data between containers
address_file="$file_path$test_name"_addr"$tmp_ext" # file shared between host and containers for sharing address (optional)
bigfile_name=bigfile
bigfile_size=10M # size of file we want to use for the test
tx_md5sumfile="$file_path"tx_"$bigfile_name"_md5sum"$tmp_ext"


# --- Network Config ---
echo '*** ZeroTier Network Containers Test: ' "$test_name"
chown -R daemon /var/lib/zerotier-one
chgrp -R daemon /var/lib/zerotier-one
su daemon -s /bin/bash -c '/zerotier-one -d -U -p9993 >>/tmp/zerotier-one.out 2>&1'
echo '*** Waiting for initial identity generation...'
while [ ! -s /var/lib/zerotier-one/identity.secret ]; do
	sleep 0.2
done
echo '*** Waiting for network config...'
virtip4=""
while [ ! -s /var/lib/zerotier-one/networks.d/"$nwconf" ]; do
	sleep 0.2
done
while [ -z "$virtip4" ]; do
	sleep 0.2
	virtip4=`/zerotier-cli listnetworks | grep -F $nwid | cut -d ' ' -f 9 | sed 's/,/\n/g' | grep -F '.' | cut -d / -f 1`
done
echo '*** Up and running at' $virtip4 ' on network: ' $nwid
echo '*** Writing address to ' "$address_file"
echo $virtip4 > "$address_file"


# --- Test section ---
# Generate large random file for transfer test, share md5sum for monitor container to check
echo '*** Generating ' "$bigfile_size" ' file'
dd if=/dev/urandom of=/var/www/html/"$bigfile_name"  bs="$bigfile_size"  count=1
#md5sum /var/www/html/"$bigfile_name" >> "$tx_md5sumfile"
md5sum < /var/www/html/"$bigfile_name" >> "$tx_md5sumfile"
echo '*** Wrote MD5 sum to ' "$tx_md5sumfile"

echo '*** Starting application...'
sleep 0.5
rm -rf /run/httpd/* /tmp/httpd*
zerotier-intercept /usr/sbin/httpd -X
