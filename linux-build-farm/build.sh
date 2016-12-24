#!/bin/bash

export PATH=/bin:/usr/bin:/usr/sbin:/sbin:/usr/local/bin:/usr/local/sbin

subdirs=$*
if [ ! -n "$subdirs" ]; then
	subdirs=`find . -type d -name '*-*' -printf '%f '`
fi

if [ ! -d ./ubuntu-trusty ]; then
	echo 'Must run from linux-build-farm subfolder.'
	exit 1
fi

rm -f zt1-src.tar.gz
cd ..
git archive --format=tar.gz --prefix=ZeroTierOne/ -o linux-build-farm/zt1-src.tar.gz HEAD
cd linux-build-farm

# Note that --privileged is used so we can bind mount VM shares when building in a VM.
# It has no other impact or purpose, but probably doesn't matter here in any case.

for distro in $subdirs; do
	echo
	echo "--- BUILDING FOR $distro ---"
	echo

	cd $distro

	if [ -d x64 ]; then
		cd x64
		mv ../../zt1-src.tar.gz .
		docker build -t zt1-build-${distro}-x64 .
		mv zt1-src.tar.gz ../..
		cd ..
	fi

	if [ -d x86 ]; then
		cd x86
		mv ../../zt1-src.tar.gz .
		docker build -t zt1-build-${distro}-x86 .
		mv zt1-src.tar.gz ../..
		cd ..
	fi

	rm -f *.deb *.rpm

#	exit 0

	if [ ! -n "`echo $distro | grep -F debian`" -a ! -n "`echo $distro | grep -F ubuntu`" ]; then
		if [ -d x64 ]; then
			docker run --rm -v `pwd`:/artifacts --privileged -it zt1-build-${distro}-x64 /bin/bash -c 'cd /ZeroTierOne ; make redhat ; cd .. ; cp `find /root/rpmbuild -type f -name *.rpm` /artifacts ; ls -l /artifacts'
		fi
		if [ -d x86 ]; then
			docker run --rm -v `pwd`:/artifacts --privileged -it zt1-build-${distro}-x86 /bin/bash -c 'cd /ZeroTierOne ; make redhat ; cd .. ; cp `find /root/rpmbuild -type f -name *.rpm` /artifacts ; ls -l /artifacts'
		fi
	else
		if [ -d x64 ]; then
			docker run --rm -v `pwd`:/artifacts --privileged -it zt1-build-${distro}-x64 /bin/bash -c 'cd /ZeroTierOne ; make debian ; cd .. ; cp *.deb /artifacts ; ls -l /artifacts'
		fi
		if [ -d x86 ]; then
			docker run --rm -v `pwd`:/artifacts --privileged -it zt1-build-${distro}-x86 /bin/bash -c 'cd /ZeroTierOne ; make debian ; cd .. ; cp *.deb /artifacts ; ls -l /artifacts'
		fi
	fi

	cd ..
done

rm -f zt1-src.tar.gz
