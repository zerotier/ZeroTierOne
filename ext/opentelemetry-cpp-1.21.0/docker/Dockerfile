# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

ARG BASE_IMAGE=ubuntu:latest
ARG GRPC_IMAGE=grpc-${BASE_IMAGE}

FROM ${GRPC_IMAGE} as grpc

FROM base-${BASE_IMAGE}-dev AS otel-cpp

ARG CORES=${nproc}
ARG OTEL_GIT_TAG=v1.3.0

COPY --from=grpc / /usr/local

#install opentelemetry-cpp
RUN git clone --recurse-submodules -j ${CORES} --depth=1 \
    -b ${OTEL_GIT_TAG} https://github.com/open-telemetry/opentelemetry-cpp.git \
    && cd opentelemetry-cpp \
    && mkdir -p build && cd build \
    && cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_POSITION_INDEPENDENT_CODE=TRUE  \
        -DWITH_ZIPKIN=ON \
        -DCMAKE_INSTALL_PREFIX=/opt/third_party/install \
        -DBUILD_TESTING=OFF \
        -DWITH_OTLP_GRPC=ON \
        -DWITH_OTLP_HTTP=ON \
        -DBUILD_SHARED_LIBS=ON \
        .. \
    && cmake --build . -j ${CORES} --target install

FROM scratch as final

COPY --from=otel-cpp /opt/third_party/install /
