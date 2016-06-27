FROM centos:6
MAINTAINER Adam Ierymenko <adam.ierymenko@zerotier.com>

RUN yum update -y
RUN yum install -y epel-release
RUN yum install -y make development-tools rpmdevtools clang gcc-c++ tar
RUN yum install -y nodejs npm

ADD zt1-src.tar.gz /
