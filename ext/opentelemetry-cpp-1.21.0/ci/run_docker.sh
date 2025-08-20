#!/bin/bash

# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

set -e

BUILD_IMAGE=opentelemetry-cpp-build
docker image inspect "$BUILD_IMAGE" &> /dev/null || {
  docker build -t "$BUILD_IMAGE" -f .devcontainer/Dockerfile.dev .
}

if [[ $# -ge 1 ]]; then
  docker run --user "$(id -u):$(id -g)" -v "$PWD":/src -w /src -it "$BUILD_IMAGE" "$@"
else
  docker run -v "$PWD":/src -w /src --privileged -it "$BUILD_IMAGE" /bin/bash -l
fi
