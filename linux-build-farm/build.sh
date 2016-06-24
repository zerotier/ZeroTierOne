#!/bin/bash

export PATH=/bin:/usr/bin:/usr/sbin:/sbin:/usr/local/bin:/usr/local/sbin

if [ $# = 0 ]; then
	echo 'Usage: ./build.sh <distribution-name> [<distribution-name>] ...'
	exit 1
fi

if [ ! -d ./ubuntu-trusty ]; then
	echo 'Must run from linux-build-farm subfolder.'
	exit 1
fi

rm -f zt1-src.tar.gz
cd ..
git archive --format=tar.gz --prefix=ZeroTierOne/ -o linux-build-farm/zt1-src.tar.gz HEAD
cd linux-build-farm

for distro in $*; do
	cd $distro

	rm -f *.deb

	cd x64
	mv ../../zt1-src.tar.gz .
	docker build -t zt1-build-${distro}-x64 .
	mv zt1-src.tar.gz ../..
	cd ..

	docker run --rm -v `pwd`:/artifacts --privileged -it zt1-build-${distro}-x64 /bin/bash -c 'cd /ZeroTierOne ; make debian ; cd .. ; cp *.deb /artifacts ; ls -l /artifacts'

	cd x86
	mv ../../zt1-src.tar.gz .
	docker build -t zt1-build-${distro}-x86 .
	mv zt1-src.tar.gz ../..
	cd ..

	docker run --rm -v `pwd`:/artifacts --privileged -it zt1-build-${distro}-x86 /bin/bash -c 'cd /ZeroTierOne ; make debian ; cd .. ; cp *.deb /artifacts ; ls -l /artifacts'

	cd ..
done

rm -f zt1-src.tar.gz
