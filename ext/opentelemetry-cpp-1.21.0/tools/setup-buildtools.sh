#!/bin/bash

# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0
set -e

# Switch to workspace root directory first
DIR="$( cd "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"

if [ -f /bin/yum ] ; then
# Prefer yum over apt-get
yum -y install automake
yum -y install autoconf
yum -y install libtool
yum -y install make gcc gcc-c++
yum -y install wget
yum -y install libcurl
yum -y install zlib-devel
yum -y install git
yum -y install gperftools-libs
yum -y install libcurl-devel
yum -y install rpm-build

# Install gcc-7
# FIXME: current tooling is CentOS-centric :-/
yum -y install centos-release-scl
yum -y install devtoolset-7
yum -y install devtoolset-7-valgrind

yum-config-manager --enable rhel-server-rhscl-7-rpms

else
# Use apt-get
export DEBIAN_FRONTEND=noninteractive
apt-get update -y
apt-get install -qq automake
apt-get install -qq bc
apt-get install -qq libtool-bin
apt-get install -qq cmake
apt-get install -qq curl
apt-get install -qq libcurl4-openssl-dev
apt-get install -qq zlib1g-dev
apt-get install -qq git
apt-get install -qq build-essential
apt-get install -qq libssl-dev
apt-get install -qq libsqlite3-dev
# Stock sqlite may be too old
#apt install libsqlite3-dev
apt-get install -qq wget
apt-get install -qq clang-format
apt-get install -qq libgtest-dev
apt-get install -qq libbenchmark-dev
apt-get install -qq nlohmann-json-dev
fi

# Build and install latest CMake
$DIR/setup-cmake.sh

## Change owner from root to current dir owner
chown -R `stat . -c %u:%g` *
