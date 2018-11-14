# Dockerfile for ZeroTier Central Controllers
FROM centos:7
MAINTAINER Adam Ierymekno <adam.ierymenko@zerotier.com>, Grant Limberg <grant.limberg@zerotier.com>

RUN yum update -y
RUN yum install -y https://download.postgresql.org/pub/repos/yum/10/redhat/rhel-7-x86_64/pgdg-centos10-10-2.noarch.rpm
RUN yum install -y postgresql10 libpqxx-devel

RUN yum -y install epel-release && yum -y update && yum clean all
RUN yum -y install clang jemalloc jemalloc-devel


ADD zerotier-one /usr/local/bin/zerotier-one
RUN chmod a+x /usr/local/bin/zerotier-one

ADD docker/main.sh /
RUN chmod a+x /main.sh

ENTRYPOINT /main.sh 
