#!/bin/bash

if [ -z "$1" -o -z "$2" ]; then
	echo 'Usage: maketestenv.sh <output file e.g. test-01.env> <network ID>'
	exit 1
fi

newid=`../../zerotier-idtool generate`

echo "ZEROTIER_IDENTITY_SECRET=$newid" >$1
echo "ZEROTIER_NETWORK=$2" >>$1
