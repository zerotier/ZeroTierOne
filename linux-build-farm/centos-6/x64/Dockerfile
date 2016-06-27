FROM centos:6
MAINTAINER Adam Ierymenko <adam.ierymenko@zerotier.com>

RUN yum update -y
RUN yum install -y epel-release
RUN yum install -y make development-tools rpmdevtools http-parser-devel lz4-devel libnatpmp-devel
RUN yum install -y clang
RUN yum install -y gcc-c++
RUN yum install -y tar

ADD zt1-src.tar.gz /
