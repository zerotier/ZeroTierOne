#!/usr/bin/env bash

# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0
set -e

# Switch to workspace root directory first
DIR="$( cd "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
WORKSPACE_ROOT=$DIR/..
pushd $WORKSPACE_ROOT

BUILD_ROOT=${BUILD_ROOT:-/tmp/build}
[[ ! -d "${BUILD_ROOT}"           ]] && mkdir -p "${BUILD_ROOT}"           || echo "Output directory already exists: BUILD_ROOT=${BUILD_ROOT}"

BENCHMARK_BUILD_ROOT=${BUILD_ROOT}/benchmark
[[ ! -d "${BENCHMARK_BUILD_ROOT}" ]] && mkdir -p "${BENCHMARK_BUILD_ROOT}" || echo "Output directory already exists: BENCHMARK_BUILD_ROOT=${BENCHMARK_BUILD_ROOT}"

# Build Google Benchmark with given Google Test
export BENCHMARK_SRC_PATH=${WORKSPACE_ROOT}/third_party/benchmark
if [ ! -d "${BENCHMARK_SRC_PATH}" ]; then
  echo "Google Benchmark not found!"
  exit 1
fi

pushd ${BENCHMARK_BUILD_ROOT}
cmake $BENCHMARK_SRC_PATH -DBENCHMARK_ENABLE_TESTING=OFF -DCMAKE_INSTALL_PREFIX=$BUILD_ROOT
make install
popd

popd
