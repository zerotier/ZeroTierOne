#!/bin/bash

# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

set -e

[ -z "${PROTOBUF_VERSION}" ] && export PROTOBUF_VERSION="21.12"

#
# Note
#
# protobuf uses two release number schemes,
# for example 3.21.12 and 21.12,
# and both tags corresponds to the same commit:
#
#   commit f0dc78d7e6e331b8c6bb2d5283e06aa26883ca7c (HEAD -> release-3.21.12, tag: v3.21.12, tag: v21.12)
#   Author: Protobuf Team Bot <protobuf-team-bot@google.com>
#   Date:   Mon Dec 12 16:03:12 2022 -0800
#
#       Updating version.json and repo version numbers to: 21.12
#
# tag v21.12 corresponds to the 'protoc version', or repo version
# tag v3.21.12 corresponds to the 'cpp version'
#
# protobuf-cpp-3.21.12.tar.gz:
# - is provided under releases/download/v21.12
# - is no longer provided under releases/download/v3.21.12,
#
# Use the "repo version number" (PROTOBUF_VERSION=21.12)
# when calling this script
#

CPP_PROTOBUF_BUILD_OPTIONS=(
  "-DCMAKE_POSITION_INDEPENDENT_CODE=ON"
  "-Dprotobuf_BUILD_TESTS=OFF"
  "-Dprotobuf_BUILD_EXAMPLES=OFF"
)

if [ ! -z "${CXX_STANDARD}" ]; then
    CPP_PROTOBUF_BUILD_OPTIONS+=("-DCMAKE_CXX_STANDARD=${CXX_STANDARD}")
    CPP_PROTOBUF_BUILD_OPTIONS+=("-DCMAKE_CXX_STANDARD_REQUIRED=ON")
    CPP_PROTOBUF_BUILD_OPTIONS+=("-DCMAKE_CXX_EXTENSIONS=OFF")
fi

# After protobuf 22/4.22, protobuf depends on absl and we can use
# "-Dprotobuf_ABSL_PROVIDER=package" to tell protobuf to find absl from the
# system. Otherwise, it will build absl from source.
# 4.XX.YY and 3.XX.YY are alias of XX.YY, and source pacakges are moved into the
# tag of XX.YY and without -cpp suffix from protobuf v22.
if [[ ${PROTOBUF_VERSION/.*/} -ge 22 ]]; then
  export CPP_PROTOBUF_VERSION="${PROTOBUF_VERSION}"
  CPP_PROTOBUF_PACKAGE_NAME="protobuf-${CPP_PROTOBUF_VERSION}"
  CPP_PROTOBUF_BUILD_OPTIONS=(${CPP_PROTOBUF_BUILD_OPTIONS[@]} "-Dprotobuf_ABSL_PROVIDER=package")
else
  export CPP_PROTOBUF_VERSION="3.${PROTOBUF_VERSION}"
  CPP_PROTOBUF_PACKAGE_NAME="protobuf-cpp-${CPP_PROTOBUF_VERSION}"
fi

cd /tmp
wget https://github.com/google/protobuf/releases/download/v${PROTOBUF_VERSION}/${CPP_PROTOBUF_PACKAGE_NAME}.tar.gz
tar zxf ${CPP_PROTOBUF_PACKAGE_NAME}.tar.gz --no-same-owner

echo "Building protobuf ${CPP_PROTOBUF_VERSION}"
echo "CMake build options:" "${CPP_PROTOBUF_BUILD_OPTIONS[@]}"

mkdir protobuf-${CPP_PROTOBUF_VERSION}/build && pushd protobuf-${CPP_PROTOBUF_VERSION}/build
if [ -e "../CMakeLists.txt" ]; then
  cmake .. "${CPP_PROTOBUF_BUILD_OPTIONS[@]}"
else
  cmake ../cmake "${CPP_PROTOBUF_BUILD_OPTIONS[@]}"
fi
make -j $(nproc)
make install
popd
ldconfig
