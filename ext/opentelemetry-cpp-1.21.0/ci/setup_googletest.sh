#!/bin/bash

# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

set -e

export DEBIAN_FRONTEND=noninteractive
apt-get update

if [ -z "${GOOGLETEST_VERSION}" ]; then
  # Version by default. Requires C++14.
  export GOOGLETEST_VERSION=1.14.0
fi

OLD_GOOGLETEST_VERSION_REGEXP="^1\.([0-9]|10|11|12)(\..*)?$"

if [[ ${GOOGLETEST_VERSION} =~ ${OLD_GOOGLETEST_VERSION_REGEXP} ]]; then
  # Old (up to 1.12.x included) download URL format.
  GOOGLETEST_VERSION_PATH="release-${GOOGLETEST_VERSION}"
  GOOGLETEST_FOLDER_PATH="googletest-release-${GOOGLETEST_VERSION}"
else
  # New (since 1.13.0) download URL format.
  GOOGLETEST_VERSION_PATH="v${GOOGLETEST_VERSION}"
  GOOGLETEST_FOLDER_PATH="googletest-${GOOGLETEST_VERSION}"
fi

googletest_install()
{
  # Follows these instructions
  # https://gist.github.com/dlime/313f74fd23e4267c4a915086b84c7d3d
  tmp_dir=$(mktemp -d)
  pushd $tmp_dir
  wget https://github.com/google/googletest/archive/${GOOGLETEST_VERSION_PATH}.tar.gz
  tar -xf ${GOOGLETEST_VERSION_PATH}.tar.gz
  cd ${GOOGLETEST_FOLDER_PATH}/
  mkdir build && cd build
  cmake .. -DBUILD_SHARED_LIBS=ON -DINSTALL_GTEST=ON -DCMAKE_INSTALL_PREFIX:PATH=/usr
  make -j $(nproc)
  make install
  ldconfig
  popd
}

set +e
echo	\
      libbenchmark-dev \
      zlib1g-dev \
      sudo \
      libcurl4-openssl-dev \
      nlohmann-json-dev \
      nlohmann-json3 \
      nlohmann-json3-dev | xargs -n 1 apt-get install --ignore-missing --no-install-recommends --no-install-suggests -y
set -e

googletest_install
