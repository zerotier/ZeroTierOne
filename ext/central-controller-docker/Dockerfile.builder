# Dockerfile for building ZeroTier Central Controllers
FROM debian:bookworm

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
    libcurl4-openssl-dev \
    google-perftools \
    libgoogle-perftools-dev \
    protobuf-compiler \
    protobuf-compiler-grpc \
    protobuf-c-compiler \
    grpc-proto \
    libgrpc++1.51 \
    libgrpc++-dev \
    libgrpc-dev \
    libgrpc29 \
    cmake \
    git


RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
