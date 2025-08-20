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

GTEST_BUILD_ROOT=${BUILD_ROOT}/gtest
[[ ! -d "${GTEST_BUILD_ROOT}"     ]] && mkdir -p "${GTEST_BUILD_ROOT}"     || echo "Output directory already exists: GTEST_BUILD_ROOT=${GTEST_BUILD_ROOT}"

# Path to Google Test source. Prefer Google Test from submodule if available:
export GTEST_SRC_PATH=${WORKSPACE_ROOT}/third_party/googletest
if [ ! -d "${GTEST_SRC_PATH}" ]; then
  # If not available, then use Google Test that is installed by OS package:
  export GTEST_SRC_PATH=/usr/src/gtest
  if [ ! -d "${GTEST_SRC_PATH}" ]; then
    echo "GTest not found!"
    exit 1
  fi
  echo Building GTest from source: ${GTEST_SRC_PATH} ...
fi

pushd $GTEST_BUILD_ROOT
cmake -Dgtest_build_samples=OFF \
      -Dgmock_build_samples=OFF \
      -Dgtest_build_tests=OFF \
      -Dgmock_build_tests=OFF \
      -DCMAKE_CXX_FLAGS="-fPIC $CXX_FLAGS -Wno-return-type" \
      -DCMAKE_INSTALL_PREFIX=$BUILD_ROOT \
      "${GTEST_SRC_PATH}"
make install
popd

popd
