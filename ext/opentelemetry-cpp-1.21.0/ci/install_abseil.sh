#!/bin/bash

# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

set -ex
export DEBIAN_FRONTEND=noninteractive
[ -z "${ABSEIL_CPP_VERSION}" ] && export ABSEIL_CPP_VERSION="20240116.1"

TOPDIR=`pwd`

BUILD_DIR=/tmp/
INSTALL_DIR=/usr/local/
pushd $BUILD_DIR
git clone --depth=1 -b ${ABSEIL_CPP_VERSION} https://github.com/abseil/abseil-cpp.git
cd abseil-cpp
ABSEIL_CPP_BUILD_OPTIONS=(
    "-DBUILD_TESTING=OFF"
    "-DCMAKE_POSITION_INDEPENDENT_CODE=ON"
    "-DCMAKE_INSTALL_PREFIX=$INSTALL_DIR"
)

if [ ! -z "${CXX_STANDARD}" ]; then
    ABSEIL_CPP_BUILD_OPTIONS+=("-DCMAKE_CXX_STANDARD=${CXX_STANDARD}")
    ABSEIL_CPP_BUILD_OPTIONS+=("-DCMAKE_CXX_STANDARD_REQUIRED=ON")
    ABSEIL_CPP_BUILD_OPTIONS+=("-DCMAKE_CXX_EXTENSIONS=OFF")
fi

#
# ABSEIL_CPP_VERSION="20240116.1" fails to build with CMake 3.30
# ABSEIL_CPP_VERSION="20240116.2" fails to build with CMake 3.30
# note that somehow the same builds with CMake 3.29.6
#
# Error reported:
#   CMake Error at CMake/AbseilHelpers.cmake:317 (target_link_libraries):
#    The link interface of target "test_allocator" contains:
#
#      GTest::gmock
#
#    but the target was not found.  Possible reasons include:
#
#      * There is a typo in the target name.
#      * A find_package call is missing for an IMPORTED target.
#      * An ALIAS target is missing.
#
#   Call Stack (most recent call first):
#    absl/container/CMakeLists.txt:206 (absl_cc_library)
#
# Root cause:
#   https://github.com/abseil/abseil-cpp/pull/1536
#
# Applying fix from abseil commit 779a3565ac6c5b69dd1ab9183e500a27633117d5
#
# TODO(marcalff) Cleanup once abseil is upgraded to the next LTS


if [ "${ABSEIL_CPP_VERSION}" = "20240116.1" ] || [ "${ABSEIL_CPP_VERSION}" = "20240116.2" ]; then
  echo "Patching abseil"
  patch -p1 < ${TOPDIR}/ci/fix-abseil-cpp-issue-1536.patch
else
  echo "Not patching abseil"
fi

echo "Building abseil ${ABSEIL_CPP_VERSION}"
echo "CMake build options:" "${ABSEIL_CPP_BUILD_OPTIONS[@]}"

mkdir build && pushd build
cmake "${ABSEIL_CPP_BUILD_OPTIONS[@]}" ..
make -j $(nproc)
make install
popd
popd
export PATH=${INSTALL_DIR}/bin:$PATH  # ensure to use the installed abseil
