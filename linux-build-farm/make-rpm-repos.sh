#!/bin/bash

export PATH=/bin:/usr/bin:/usr/local/bin:/sbin:/usr/sbin

GPG_KEY=contact@zerotier.com

rm -rf /tmp/zt-rpm-repo
mkdir /tmp/zt-rpm-repo

for distro in centos-* fedora-* amazon-*; do
	dname=`echo $distro | cut -d '-' -f 1`
	if [ "$dname" = "centos" ]; then
		dname=el
	fi
	if [ "$dname" = "fedora" ]; then
		dname=fc
	fi
	if [ "$dname" = "amazon" ]; then
		dname=amzn1
	fi
	dvers=`echo $distro | cut -d '-' -f 2`

	mkdir -p /tmp/zt-rpm-repo/$dname/$dvers

	cp -v $distro/*.rpm /tmp/zt-rpm-repo/$dname/$dvers
done

rpmsign --resign --key-id=$GPG_KEY --digest-algo=sha256 `find /tmp/zt-rpm-repo -type f -name '*.rpm'`

for db in `find /tmp/zt-rpm-repo -mindepth 2 -maxdepth 2 -type d`; do
	createrepo --database $db
done

echo
echo Repo created in /tmp/zt-rpm-repo
