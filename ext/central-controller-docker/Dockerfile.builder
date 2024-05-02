# Dockerfile for building ZeroTier Central Controllers
FROM ubuntu:jammy as builder
MAINTAINER Adam Ierymekno <adam.ierymenko@zerotier.com>, Grant Limberg <grant.limberg@zerotier.com>

ARG git_branch=master

RUN apt update && apt upgrade -y
RUN apt -y install \
    build-essential \
    pkg-config \
    bash \
    clang \
    libjemalloc2 \
    libjemalloc-dev \
    libpq5 \
    libpq-dev \
    openssl \
    libssl-dev \
    postgresql-client \
    postgresql-client-common \
    curl \
    google-perftools \
    libgoogle-perftools-dev \
    protobuf-compiler 

RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
