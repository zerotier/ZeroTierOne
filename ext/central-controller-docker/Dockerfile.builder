# Dockerfile for building ZeroTier Central Controllers
FROM centos:8 as builder
MAINTAINER Adam Ierymekno <adam.ierymenko@zerotier.com>, Grant Limberg <grant.limberg@zerotier.com>

ARG git_branch=master

RUN yum update -y
RUN yum install -y https://download.postgresql.org/pub/repos/yum/reporpms/EL-8-x86_64/pgdg-redhat-repo-latest.noarch.rpm
RUN dnf -qy module disable postgresql
RUN yum -y install epel-release && yum -y update && yum clean all
RUN yum groupinstall -y "Development Tools" && yum clean all
RUN yum install -y bash cmake postgresql10 postgresql10-devel clang jemalloc jemalloc-devel libpqxx libpqxx-devel openssl-devel && yum clean all
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
