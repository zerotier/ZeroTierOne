#!/bin/bash

# This is for internal use by ZeroTier, but obviously users can repurpose it to make their own.

# Usage: make-root-set.sh <identity.secret> [<...>]

for i in $*; do
	echo $i
	../../target/debug/zerotier rootset sign root.zerotier.com.json $i >tmp.json
	mv -f tmp.json root.zerotier.com.json
	../../target/debug/zerotier rootset marshal root.zerotier.com.json >root.zerotier.com.bin
done

cat root.zerotier.com.json
