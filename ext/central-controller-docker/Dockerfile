# Dockerfile for ZeroTier Central Controllers
FROM centos:8 as builder
MAINTAINER Adam Ierymekno <adam.ierymenko@zerotier.com>, Grant Limberg <grant.limberg@zerotier.com>

ARG git_branch=master

RUN yum update -y
RUN yum install -y https://download.postgresql.org/pub/repos/yum/reporpms/EL-8-x86_64/pgdg-redhat-repo-latest.noarch.rpm
RUN dnf -qy module disable postgresql
RUN yum -y install epel-release && yum -y update && yum clean all
RUN yum groupinstall -y "Development Tools"
RUN yum install -y bash cmake postgresql10 postgresql10-devel clang jemalloc jemalloc-devel
RUN curl -sL https://github.com/jtv/libpqxx/archive/refs/tags/6.4.5.tar.gz -o libpqxx.tar.gz
RUN tar -xzf libpqxx.tar.gz && \
    pushd libpqxx-6.4.5/ && \
    mkdir build && pushd build/ && \
    cmake .. && \
    make install -j8 && \
    popd && popd


# RUN git clone http://git.int.zerotier.com/zerotier/ZeroTierOne.git
# RUN if [ "$git_branch" != "master" ]; then cd ZeroTierOne && git checkout -b $git_branch origin/$git_branch; fi
ADD . /ZeroTierOne
RUN cd ZeroTierOne && make clean && make central-controller -j8

FROM centos:8
RUN yum install -y https://download.postgresql.org/pub/repos/yum/reporpms/EL-8-x86_64/pgdg-redhat-repo-latest.noarch.rpm
RUN dnf -qy module disable postgresql 
RUN yum -y install epel-release && yum -y update && yum clean all
RUN yum install -y jemalloc jemalloc-devel postgresql10

COPY --from=builder /usr/local/lib64/libpqxx.so /usr/local/lib64/libpqxx.so
COPY --from=builder /usr/local/lib64/libpqxx.a /usr/local/lib64/libpqxx.a
COPY --from=builder /ZeroTierOne/zerotier-one /usr/local/bin/zerotier-one
RUN chmod a+x /usr/local/bin/zerotier-one
RUN echo "/usr/local/lib64" > /etc/ld.so.conf.d/usr-local-lib64.conf && ldconfig

ADD ext/central-controller-docker/main.sh /
RUN chmod a+x /main.sh

ENTRYPOINT /main.sh
