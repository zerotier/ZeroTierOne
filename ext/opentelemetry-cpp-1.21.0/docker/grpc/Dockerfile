# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

ARG BASE_IMAGE=ubuntu:latest
FROM ${BASE_IMAGE} AS grpc

ARG CORES=${nproc}
ARG GRPC_GIT_TAG=v1.45.2

RUN mkdir mkdir -p /opt/third_party/grpc

WORKDIR /opt/third_party/grpc

ADD CMakeLists.txt /opt/third_party/grpc

RUN mkdir build \
    && cd build \
    && cmake -DCMAKE_INSTALL_PREFIX=/opt/third_party/install \
    -DGRPC_GIT_TAG=${GRPC_GIT_TAG} /opt/third_party/grpc \
    && cmake --build . -j ${CORES} --target install

FROM scratch as final

COPY --from=grpc /opt/third_party/install /
