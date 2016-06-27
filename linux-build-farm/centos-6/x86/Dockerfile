FROM toopher/centos-i386:centos6
MAINTAINER Adam Ierymenko <adam.ierymenko@zerotier.com>

RUN yum update -y
RUN yum install -y epel-release
RUN yum install -y make development-tools rpmdevtools http-parser-devel lz4-devel libnatpmp-devel

#RUN rpm --import http://ftp.scientificlinux.org/linux/scientific/5x/x86_64/RPM-GPG-KEYs/RPM-GPG-KEY-cern
#RUN curl http://linuxsoft.cern.ch/cern/devtoolset/slc6-devtoolset.repo >/etc/yum.repos.d/slc6-devtoolset.repo
#RUN yum install -y devtoolset-2-gcc-c++

#RUN ln -sf /opt/rh/devtoolset-2/root/usr/bin/gcc /usr/bin/gcc
#RUN ln -sf /opt/rh/devtoolset-2/root/usr/bin/g++ /usr/bin/g++

RUN yum install -y clang
RUN yum install -y gcc-c++

ADD zt1-src.tar.gz /

#RUN mv /ZeroTierOne/make-linux.mk /ZeroTierOne/make-linux.mk.old
#RUN echo 'CFLAGS=-O3 -fstack-protector' >/ZeroTierOne/make-linux.mk
#RUN echo 'CXXFLAGS=-O3 -fstack-protector' >>/ZeroTierOne/make-linux.mk
#RUN cat /ZeroTierOne/make-linux.mk.old >>/ZeroTierOne/make-linux.mk
