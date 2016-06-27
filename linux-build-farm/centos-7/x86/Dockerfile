#FROM zerotier/centos7-32bit
#MAINTAINER Adam Ierymenko <adam.ierymenko@zerotier.com>

#RUN echo 'i686-redhat-linux' >/etc/rpm/platform

#RUN yum update -y
#RUN yum install -y make development-tools rpmdevtools http-parser-devel lz4-devel libnatpmp-devel

#RUN yum install -y gcc-c++
#RUN rpm --install --force https://dl.fedoraproject.org/pub/epel/epel-release-latest-6.noarch.rpm
#RUN rpm --install --force ftp://rpmfind.net/linux/centos/6.8/os/i386/Packages/libffi-3.0.5-3.2.el6.i686.rpm
#RUN yum install -y clang

FROM zerotier/zt1-build-centos-7-x86-base
MAINTAINER Adam Ierymenko <adam.ierymenko@zerotier.com>

RUN yum install -y ruby ruby-devel
RUN gem install ronn

#RUN rpm --erase http-parser-devel lz4-devel libnatpmp-devel

ADD zt1-src.tar.gz /
