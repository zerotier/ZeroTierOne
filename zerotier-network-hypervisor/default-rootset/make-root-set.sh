#!/bin/bash

# This is for internal use by ZeroTier, but obviously users can repurpose it to make their own.

# Usage: make-root-set.sh <identity.secret> [<...>]

for i in $*; do
	echo $i
	../../zerotier-system-service/target/debug/zerotier-system-service rootset sign root.zerotier.com.json $i >tmp.json
	mv -f tmp.json root.zerotier.com.json
	../../zerotier-system-service/target/debug/zerotier-system-service rootset marshal root.zerotier.com.json >root.zerotier.com.bin
done

cat root.zerotier.com.json
