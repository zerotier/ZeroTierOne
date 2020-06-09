# Dockerfile for ZeroTier Central Controllers
FROM centos:8 as builder
MAINTAINER Grant Limberg <grant.limberg@zerotier.com>

RUN yum update -y
RUN yum install -y https://download.postgresql.org/pub/repos/yum/reporpms/EL-8-x86_64/pgdg-redhat-repo-latest.noarch.rpm && dnf -qy module disable postgresql
RUN yum -y install epel-release && yum -y update && yum clean all && \
    yum groupinstall -y "Development Tools" && \
    yum install -y bash cmake wget postgresql10 postgresql10-devel libpqxx-devel clang jemalloc jemalloc-devel hiredis-devel && \
    wget https://dl.google.com/go/go1.14.4.linux-amd64.tar.gz && tar -C /usr/local -xzf go1.14.4.linux-amd64.tar.gz
ADD . /ZeroTierOne
ENV PATH="/usr/local/go/bin:${PATH}"
RUN cd ZeroTierOne && make clean && make central-controller

FROM centos:8
RUN yum install -y https://download.postgresql.org/pub/repos/yum/reporpms/EL-8-x86_64/pgdg-redhat-repo-latest.noarch.rpm && \
    dnf -qy module disable postgresql && \
    yum -y install epel-release && \
    yum -y update && yum clean all && \
    yum install -y jemalloc jemalloc-devel postgresql10 hiredis

COPY --from=builder /ZeroTierOne/build/zerotier /usr/local/bin/zerotier
RUN chmod a+x /usr/local/bin/zerotier

ADD controller/central-docker/main.sh /
RUN chmod a+x /main.sh

ENTRYPOINT /main.sh
