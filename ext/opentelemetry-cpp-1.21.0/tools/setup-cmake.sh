#!/bin/bash

# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

#
# This script installs latest CMake on Linux machine
#
set -e

export PATH=/usr/local/bin:$PATH
# Min required CMake version
export CMAKE_MIN_VERSION=${1:-3.1.0}
# Target version to install if min required is not found
export CMAKE_VERSION=${2:-3.18.4}

UPGRADE_NEEDED=no

function splitVersion {
  pattern='([^0-9]*\([0-9]*\)[.]\([0-9]*\)[.]'
  v1=$(cut -d '.' -f 1 <<< $ver )
  v2=$(cut -d '.' -f 2 <<< $ver )
  v3=$(cut -d '.' -f 3 <<< $ver )
}

function checkVersion {
  # Current CMake version
  currVer=`cmake --version | grep version | cut -d' ' -f 3`
  ver=$currVer splitVersion
  cv1=$v1
  cv2=$v2
  cv3=$v3
  cv=`echo "65536*$v1+256*$v2+$v3" | bc`

  # New CMake version
  ver=$CMAKE_MIN_VERSION splitVersion
  nv=`echo "65536*$v1+256*$v2+$v3" | bc`
  if [ "$cv" -ge "$nv" ]; then
    echo "CMake is already installed: $currVer"
  else
    UPGRADE_NEEDED=yes
  fi
}

checkVersion

if [[ "$UPGRADE_NEEDED" == "no" ]]; then
  echo "Skipping CMake installation"
  exit 0
fi

# Download cmake to /tmp
pushd /tmp
if [[ ! -f "/tmp/cmake.tar.gz" ]]; then
  wget -O /tmp/cmake.tar.gz https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}.tar.gz
  tar -zxvf /tmp/cmake.tar.gz
fi
# Bootstrap CMake
cd cmake-${CMAKE_VERSION}
./bootstrap --prefix=/usr/local
# Build CMake without CMake and without Ninja (slow)
make
make install
popd
