FROM ubuntu:14.04
MAINTAINER Adam Ierymenko <adam.ierymenko@zerotier.com>

RUN apt-get update
RUN apt-get install -y build-essential debhelper libhttp-parser-dev liblz4-dev libnatpmp-dev dh-systemd ruby-ronn g++ make devscripts clang-3.6

RUN ln -sf /usr/bin/clang++-3.6 /usr/bin/clang++
RUN ln -sf /usr/bin/clang-3.6 /usr/bin/clang

RUN dpkg --purge libhttp-parser-dev

ADD zt1-src.tar.gz /
