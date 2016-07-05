#!/bin/bash

export PATH=/bin:/usr/bin:/usr/local/bin:/sbin:/usr/sbin

GPG_KEY=contact@zerotier.com

rm -rf /tmp/zt-rpm-repo
mkdir /tmp/zt-rpm-repo
cp `find . -type f -name '*.rpm'` /tmp/zt-rpm-repo

for rpm in /tmp/zt-rpm-repo/*.rpm; do
	rpmsign --resign --key-id=$GPG_KEY --digest-algo=sha256 $rpm
done

createrepo --database /tmp/zt-rpm-repo

echo Repo created in /tmp/zt-rpm-repo
