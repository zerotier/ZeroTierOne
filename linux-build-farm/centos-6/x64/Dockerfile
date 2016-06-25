FROM centos:6
MAINTAINER Adam Ierymenko <adam.ierymenko@zerotier.com>

RUN yum update -y
RUN yum install -y epel-release
RUN yum install -y make development-tools rpmdevtools http-parser-devel lz4-devel libnatpmp-devel

RUN yum install -y centos-release-scl
RUN yum install -y devtoolset-4-gcc-c++

RUN ln -sf /opt/rh/devtoolset-4/root/usr/bin/gcc /usr/bin/gcc
RUN ln -sf /opt/rh/devtoolset-4/root/usr/bin/g++ /usr/bin/g++

ADD zt1-src.tar.gz /
