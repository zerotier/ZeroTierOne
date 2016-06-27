FROM centos:6
MAINTAINER Adam Ierymenko <adam.ierymenko@zerotier.com>

RUN yum update -y
RUN yum install -y epel-release
RUN yum install -y make development-tools rpmdevtools clang gcc-c++ tar

RUN yum install -y nodejs npm

# Stop use of http-parser-devel which is installed by nodejs/npm
RUN rm -f /usr/include/http_parser.h

ADD zt1-src.tar.gz /
