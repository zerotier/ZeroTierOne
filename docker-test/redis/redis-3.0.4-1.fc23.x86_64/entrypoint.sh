#!/bin/bash

export PATH=/bin:/usr/bin:/usr/local/bin:/sbin:/usr/sbin:/

echo '***'
echo '*** ZeroTier Network Containers Test Image'
echo '*** https://www.zerotier.com/'
echo '***'

./zerotier-one &
./zerotier-cli join e5cd7a9e1c5311ab
./zerotier-cli listnetworks


