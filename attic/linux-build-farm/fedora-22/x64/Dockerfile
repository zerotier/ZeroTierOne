FROM fedora:22
MAINTAINER Adam Ierymenko <adam.ierymenko@zerotier.com>

RUN yum update -y
RUN yum install -y make rpmdevtools gcc-c++ rubygem-ronn json-parser-devel lz4-devel http-parser-devel libnatpmp-devel

RUN rpm --erase http-parser-devel
RUN yum install -y rubygem-ronn ruby

ADD zt1-src.tar.gz /
