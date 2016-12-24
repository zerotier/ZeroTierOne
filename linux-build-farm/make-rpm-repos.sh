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

# Stupid RHEL stuff
cd /tmp/zt-rpm-repo/el
ln -sf 6 6Client
ln -sf 6 6Workstation
ln -sf 6 6Server
ln -sf 6 6.0
ln -sf 6 6.1
ln -sf 6 6.2
ln -sf 6 6.3
ln -sf 6 6.4
ln -sf 6 6.5
ln -sf 6 6.6
ln -sf 6 6.7
ln -sf 6 6.8
ln -sf 6 6.9
ln -sf 7 7Client
ln -sf 7 7Workstation
ln -sf 7 7Server
ln -sf 7 7.0
ln -sf 7 7.1
ln -sf 7 7.2
ln -sf 7 7.3
ln -sf 7 7.4
ln -sf 7 7.5
ln -sf 7 7.6
ln -sf 7 7.7
ln -sf 7 7.8
ln -sf 7 7.9

echo
echo Repo created in /tmp/zt-rpm-repo
