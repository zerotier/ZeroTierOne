GO_VERSION:=1.14.4

.PHONY: all

all:	alpine centos debian ubuntu kali-rolling

alpine:
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-arm64.tar.gz" --platform linux/arm64 -f Dockerfile.alpine . -t ztbuild/alpine-arm64 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-386.tar.gz" --platform linux/386 -f Dockerfile.alpine . -t ztbuild/alpine-i386 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-amd64.tar.gz" --platform linux/amd64 -f Dockerfile.alpine . -t ztbuild/alpine-amd64 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-armv6l.tar.gz" --platform linux/arm/v7 -f Dockerfile.alpine . -t ztbuild/alpine-armhf --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-ppc64le.tar.gz" --platform linux/ppc64le -f Dockerfile.alpine . -t ztbuild/alpine-ppc64le --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-s390x.tar.gz" --platform linux/s390x -f Dockerfile.alpine . -t ztbuild/alpine-s390x --load

centos:
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-amd64.tar.gz" --platform linux/amd64 -f Dockerfile.centos7 . -t ztbuild/centos7-amd64 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-386.tar.gz" --platform linux/386 -f Dockerfile.centos7-i386 . -t ztbuild/centos7-i386 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-amd64.tar.gz" --platform linux/amd64 -f Dockerfile.centos8 . -t ztbuild/centos8-amd64 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-arm64.tar.gz" --platform linux/arm64 -f Dockerfile.centos8 . -t ztbuild/centos8-arm64 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-ppc64le.tar.gz" --platform linux/ppc64le -f Dockerfile.centos8 . -t ztbuild/centos8-ppc64le --load

debian:	debian-wheezy debian-jessie debian-buster debian-stretch debian-bullseye debian-sid

debian-wheezy:
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-amd64.tar.gz" --platform linux/amd64 -f Dockerfile.debian-wheezy . -t ztbuild/debian-wheezy-amd64 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-armv6l.tar.gz" --platform linux/arm/v6 -f Dockerfile.debian-wheezy . -t ztbuild/debian-wheezy-armel --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-386.tar.gz" --platform linux/386 -f Dockerfile.debian-wheezy . -t ztbuild/debian-wheezy-i386 --load

debian-jessie:
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-amd64.tar.gz" --platform linux/amd64 -f Dockerfile.debian-jessie . -t ztbuild/debian-jessie-amd64 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-armv6l.tar.gz" --platform linux/arm/v6 -f Dockerfile.debian-jessie . -t ztbuild/debian-jessie-armel --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-386.tar.gz" --platform linux/386 -f Dockerfile.debian-jessie . -t ztbuild/debian-jessie-i386 --load

debian-buster:
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-amd64.tar.gz" --platform linux/amd64 -f Dockerfile.debian-buster . -t ztbuild/debian-buster-amd64 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-arm64.tar.gz" --platform linux/arm64 -f Dockerfile.debian-buster . -t ztbuild/debian-buster-arm64 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-armv6l.tar.gz" --platform linux/arm/v6 -f Dockerfile.debian-buster . -t ztbuild/debian-buster-armel --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-armv6l.tar.gz" --platform linux/arm/v7 -f Dockerfile.debian-buster . -t ztbuild/debian-buster-armhf --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-386.tar.gz" --platform linux/386 -f Dockerfile.debian-buster . -t ztbuild/debian-buster-i386 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-ppc64le.tar.gz" --platform linux/ppc64le -f Dockerfile.debian-buster . -t ztbuild/debian-buster-ppc64le --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-s390x.tar.gz" --platform linux/s390x -f Dockerfile.debian-buster . -t ztbuild/debian-buster-s390x --load
	docker buildx build --platform linux/mips64le -f Dockerfile.debian-buster . -t ztbuild/debian-buster-mips64le --load

debian-stretch:
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-amd64.tar.gz" --platform linux/amd64 -f Dockerfile.debian-stretch-x64 . -t ztbuild/debian-stretch-amd64 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-arm64.tar.gz" --platform linux/arm64 -f Dockerfile.debian-stretch . -t ztbuild/debian-stretch-arm64 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-armv6l.tar.gz" --platform linux/arm/v6 -f Dockerfile.debian-stretch . -t ztbuild/debian-stretch-armel --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-armv6l.tar.gz" --platform linux/arm/v7 -f Dockerfile.debian-stretch . -t ztbuild/debian-stretch-armhf --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-386.tar.gz" --platform linux/386 -f Dockerfile.debian-stretch . -t ztbuild/debian-stretch-i386 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-ppc64le.tar.gz" --platform linux/ppc64le -f Dockerfile.debian-stretch . -t ztbuild/debian-stretch-ppc64le --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-s390x.tar.gz" --platform linux/s390x -f Dockerfile.debian-stretch . -t ztbuild/debian-stretch-s390x --load
	# docker buildx build --platform linux/mips64le -f Dockerfile.debian-stretch . -t ztbuild/debian-stretch-mips64le --load

debian-bullseye:
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-amd64.tar.gz" --platform linux/amd64 -f Dockerfile.debian-bullseye . -t ztbuild/debian-bullseye-amd64 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-arm64.tar.gz" --platform linux/arm64 -f Dockerfile.debian-bullseye . -t ztbuild/debian-bullseye-arm64 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-armv6l.tar.gz" --platform linux/arm/v6 -f Dockerfile.debian-bullseye . -t ztbuild/debian-bullseye-armel --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-armv6l.tar.gz" --platform linux/arm/v7 -f Dockerfile.debian-bullseye . -t ztbuild/debian-bullseye-armhf --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-386.tar.gz" --platform linux/386 -f Dockerfile.debian-bullseye . -t ztbuild/debian-bullseye-i386 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-ppc64le.tar.gz" --platform linux/ppc64le -f Dockerfile.debian-bullseye . -t ztbuild/debian-bullseye-ppc64le --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-s390x.tar.gz" --platform linux/s390x -f Dockerfile.debian-bullseye . -t ztbuild/debian-bullseye-s390x --load
	docker buildx build --platform linux/mips64le -f Dockerfile.debian-bullseye . -t ztbuild/debian-bullseye-mips64le --load

debian-sid:
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-amd64.tar.gz" --platform linux/amd64 -f Dockerfile.debian-sid . -t ztbuild/debian-sid-amd64 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-arm64.tar.gz" --platform linux/arm64 -f Dockerfile.debian-sid . -t ztbuild/debian-sid-arm64 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-armv6l.tar.gz" --platform linux/arm/v6 -f Dockerfile.debian-sid . -t ztbuild/debian-sid-armel --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-armv6l.tar.gz" --platform linux/arm/v7 -f Dockerfile.debian-sid . -t ztbuild/debian-sid-armhf --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-386.tar.gz" --platform linux/386 -f Dockerfile.debian-sid . -t ztbuild/debian-sid-i386 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-ppc64le.tar.gz" --platform linux/ppc64le -f Dockerfile.debian-sid . -t ztbuild/debian-sid-ppc64le --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-s390x.tar.gz" --platform linux/s390x -f Dockerfile.debian-sid . -t ztbuild/debian-sid-s390x --load
	docker buildx build --platform linux/mips64le -f Dockerfile.debian-sid . -t ztbuild/debian-sid-mips64le --load

ubuntu: ubuntu-trusty ubuntu-xenial ubuntu-bionic ubuntu-eoan ubuntu-focal

ubuntu-trusty:
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-amd64.tar.gz" --platform linux/amd64 -f Dockerfile.ubuntu-trusty . -t ztbuild/ubuntu-trusty-amd64 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-arm64.tar.gz" --platform linux/arm64 -f Dockerfile.ubuntu-trusty . -t ztbuild/ubuntu-trusty-arm64 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-armv6l.tar.gz" --platform linux/arm/v7 -f Dockerfile.ubuntu-trusty . -t ztbuild/ubuntu-trusty-armhf --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-386.tar.gz" --platform linux/386 -f Dockerfile.ubuntu-trusty . -t ztbuild/ubuntu-trusty-i386 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-ppc64le.tar.gz" --platform linux/ppc64le -f Dockerfile.ubuntu-trusty . -t ztbuild/ubuntu-trusty-ppc64le --load

ubuntu-xenial:
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-amd64.tar.gz" --platform linux/amd64 -f Dockerfile.ubuntu-xenial-x64 . -t ztbuild/ubuntu-xenial-amd64 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-arm64.tar.gz" --platform linux/arm64 -f Dockerfile.ubuntu-xenial . -t ztbuild/ubuntu-xenial-arm64 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-armv6l.tar.gz" --platform linux/arm/v7 -f Dockerfile.ubuntu-xenial . -t ztbuild/ubuntu-xenial-armhf --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-386.tar.gz" --platform linux/386 -f Dockerfile.ubuntu-xenial . -t ztbuild/ubuntu-xenial-i386 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-ppc64le.tar.gz" --platform linux/ppc64le -f Dockerfile.ubuntu-xenial . -t ztbuild/ubuntu-xenial-ppc64le --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-s390x.tar.gz" --platform linux/s390x -f Dockerfile.ubuntu-xenial . -t ztbuild/ubuntu-xenial-s390x --load

ubuntu-bionic:
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-amd64.tar.gz" --platform linux/amd64 -f Dockerfile.ubuntu-bionic . -t ztbuild/ubuntu-bionic-amd64 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-arm64.tar.gz" --platform linux/arm64 -f Dockerfile.ubuntu-bionic . -t ztbuild/ubuntu-bionic-arm64 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-armv6l.tar.gz" --platform linux/arm/v7 -f Dockerfile.ubuntu-bionic . -t ztbuild/ubuntu-bionic-armhf --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-386.tar.gz" --platform linux/386 -f Dockerfile.ubuntu-bionic . -t ztbuild/ubuntu-bionic-i386 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-ppc64le.tar.gz" --platform linux/ppc64le -f Dockerfile.ubuntu-bionic . -t ztbuild/ubuntu-bionic-ppc64le --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-s390x.tar.gz" --platform linux/s390x -f Dockerfile.ubuntu-bionic . -t ztbuild/ubuntu-bionic-s390x --load

ubuntu-eoan:
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-amd64.tar.gz" --platform linux/amd64 -f Dockerfile.ubuntu-eoan . -t ztbuild/ubuntu-eoan-amd64 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-arm64.tar.gz" --platform linux/arm64 -f Dockerfile.ubuntu-eoan . -t ztbuild/ubuntu-eoan-arm64 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-armv6l.tar.gz" --platform linux/arm/v7 -f Dockerfile.ubuntu-eoan . -t ztbuild/ubuntu-eoan-armhf --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-386.tar.gz" --platform linux/386 -f Dockerfile.ubuntu-eoan . -t ztbuild/ubuntu-eoan-i386 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-ppc64le.tar.gz" --platform linux/ppc64le -f Dockerfile.ubuntu-eoan . -t ztbuild/ubuntu-eoan-ppc64le --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-s390x.tar.gz" --platform linux/s390x -f Dockerfile.ubuntu-eoan . -t ztbuild/ubuntu-eoan-s390x --load

ubuntu-focal:
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-amd64.tar.gz" --platform linux/amd64 -f Dockerfile.ubuntu-focal . -t ztbuild/ubuntu-focal-amd64 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-arm64.tar.gz" --platform linux/arm64 -f Dockerfile.ubuntu-focal . -t ztbuild/ubuntu-focal-arm64 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-ppc64le.tar.gz" --platform linux/ppc64le -f Dockerfile.ubuntu-focal . -t ztbuild/ubuntu-focal-ppc64le --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-s390x.tar.gz" --platform linux/s390x -f Dockerfile.ubuntu-focal . -t ztbuild/ubuntu-focal-s390x --load

kali-rolling:
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-amd64.tar.gz" --platform linux/amd64 -f Dockerfile.kali-rolling . -t ztbuild/kali-rolling-amd64 --load


x86:
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-386.tar.gz" --platform linux/386 -f Dockerfile.alpine . -t ztbuild/alpine-i386 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-amd64.tar.gz" --platform linux/amd64 -f Dockerfile.alpine . -t ztbuild/alpine-amd64 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-amd64.tar.gz" --platform linux/amd64 -f Dockerfile.centos7 . -t ztbuild/centos7-amd64 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-386.tar.gz" --platform linux/386 -f Dockerfile.centos7-i386 . -t ztbuild/centos7-i386 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-amd64.tar.gz" --platform linux/amd64 -f Dockerfile.centos6 . -t ztbuild/centos6-amd64 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-386.tar.gz" --platform linux/386 -f Dockerfile.centos6-i386 . -t ztbuild/centos6-i386 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-amd64.tar.gz" --platform linux/amd64 -f Dockerfile.centos8 . -t ztbuild/centos8-amd64 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-amd64.tar.gz" --platform linux/amd64 -f Dockerfile.debian-wheezy . -t ztbuild/debian-wheezy-amd64 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-386.tar.gz" --platform linux/386 -f Dockerfile.debian-wheezy . -t ztbuild/debian-wheezy-i386 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-amd64.tar.gz" --platform linux/amd64 -f Dockerfile.debian-jessie . -t ztbuild/debian-jessie-amd64 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-386.tar.gz" --platform linux/386 -f Dockerfile.debian-jessie . -t ztbuild/debian-jessie-i386 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-amd64.tar.gz" --platform linux/amd64 -f Dockerfile.debian-buster . -t ztbuild/debian-buster-amd64 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-386.tar.gz" --platform linux/386 -f Dockerfile.debian-buster . -t ztbuild/debian-buster-i386 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-amd64.tar.gz" --platform linux/amd64 -f Dockerfile.debian-stretch . -t ztbuild/debian-stretch-amd64 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-386.tar.gz" --platform linux/386 -f Dockerfile.debian-stretch . -t ztbuild/debian-stretch-i386 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-amd64.tar.gz" --platform linux/amd64 -f Dockerfile.debian-bullseye . -t ztbuild/debian-bullseye-amd64 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-386.tar.gz" --platform linux/386 -f Dockerfile.debian-bullseye . -t ztbuild/debian-bullseye-i386 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-amd64.tar.gz" --platform linux/amd64 -f Dockerfile.debian-sid . -t ztbuild/debian-sid-amd64 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-386.tar.gz" --platform linux/386 -f Dockerfile.debian-sid . -t ztbuild/debian-sid-i386 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-amd64.tar.gz" --platform linux/amd64 -f Dockerfile.ubuntu-trusty . -t ztbuild/ubuntu-trusty-amd64 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-386.tar.gz" --platform linux/386 -f Dockerfile.ubuntu-trusty . -t ztbuild/ubuntu-trusty-i386 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-amd64.tar.gz" --platform linux/amd64 -f Dockerfile.ubuntu-xenial . -t ztbuild/ubuntu-xenial-amd64 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-386.tar.gz" --platform linux/386 -f Dockerfile.ubuntu-xenial . -t ztbuild/ubuntu-xenial-i386 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-amd64.tar.gz" --platform linux/amd64 -f Dockerfile.ubuntu-bionic . -t ztbuild/ubuntu-bionic-amd64 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-386.tar.gz" --platform linux/386 -f Dockerfile.ubuntu-bionic . -t ztbuild/ubuntu-bionic-i386 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-amd64.tar.gz" --platform linux/amd64 -f Dockerfile.ubuntu-eoan . -t ztbuild/ubuntu-eoan-amd64 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-386.tar.gz" --platform linux/386 -f Dockerfile.ubuntu-eoan . -t ztbuild/ubuntu-eoan-i386 --load
	docker buildx build --build-arg go_pkg_url="https://dl.google.com/go/go${GO_VERSION}.linux-amd64.tar.gz" --platform linux/amd64 -f Dockerfile.kali-rolling . -t ztbuild/kali-rolling-amd64 --load
