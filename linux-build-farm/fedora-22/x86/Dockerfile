FROM nickcis/fedora-32:22
MAINTAINER Adam Ierymenko <adam.ierymenko@zerotier.com>

RUN mkdir -p /etc/dnf/vars
RUN echo 'i386' >/etc/dnf/vars/basearch
RUN echo 'i386' >/etc/dnf/vars/arch

RUN yum update -y
RUN yum install -y make rpmdevtools gcc-c++ rubygem-ronn json-parser-devel lz4-devel http-parser-devel libnatpmp-devel

ADD zt1-src.tar.gz /
