#!/bin/bash

# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

set -e

CLANG_VERSION=18
CMAKE_FORMAT_VERSION=0.6.13
BUILDIFIER_VERSION=3.5.0

#
# This script expects ubuntu:24.04
#

apt update

# Install clang-format
apt install -y clang-format-${CLANG_VERSION} python3 python3-pip git curl
# ln /usr/bin/clang-format-${CLANG_VERSION} /usr/bin/clang-format

# Install cmake_format
pip3 install --break-system-packages cmake_format==${CMAKE_FORMAT_VERSION}

# Install buildifier
curl -L -o /usr/local/bin/buildifier https://github.com/bazelbuild/buildtools/releases/download/${BUILDIFIER_VERSION}/buildifier
chmod +x /usr/local/bin/buildifier
