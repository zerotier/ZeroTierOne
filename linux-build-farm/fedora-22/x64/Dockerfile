FROM fedora:22
MAINTAINER Adam Ierymenko <adam.ierymenko@zerotier.com>

RUN yum update -y
RUN yum install -y make rpmdevtools gcc-c++ rubygem-ronn json-parser-devel lz4-devel http-parser-devel libnatpmp-devel

ADD zt1-src.tar.gz /
