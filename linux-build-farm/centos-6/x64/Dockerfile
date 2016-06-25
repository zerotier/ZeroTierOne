FROM centos:6
MAINTAINER Adam Ierymenko <adam.ierymenko@zerotier.com>

RUN yum update -y
RUN yum install -y epel-release
RUN yum install -y clang make development-tools rpmdevtools http-parser-devel lz4-devel libnatpmp-devel

ADD zt1-src.tar.gz /

#RUN ln -sf /usr/bin/clang++-3.5 /usr/bin/clang++
#RUN ln -sf /usr/bin/clang-3.5 /usr/bin/clang
