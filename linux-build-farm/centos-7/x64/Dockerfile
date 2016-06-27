FROM centos:7
MAINTAINER Adam Ierymenko <adam.ierymenko@zerotier.com>

RUN yum update -y
RUN yum install -y epel-release
RUN yum install -y make development-tools rpmdevtools http-parser-devel lz4-devel libnatpmp-devel

RUN yum install -y clang gcc-c++

RUN yum install -y ruby ruby-devel
RUN gem install ronn

ADD zt1-src.tar.gz /
