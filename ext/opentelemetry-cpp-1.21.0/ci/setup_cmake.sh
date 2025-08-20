#!/bin/bash

# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

set -e

CMAKE_VERSION=${CMAKE_VERSION:-3.31.6}
CMAKE_DIR="cmake-${CMAKE_VERSION}-linux-x86_64"
CMAKE_TAR="${CMAKE_DIR}.tar.gz"
CMAKE_URL="https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/${CMAKE_TAR}"

INSTALL_DIR="/opt/cmake"

echo "Installing CMake version: ${CMAKE_VERSION}..."

apt-get update && apt-get remove --purge -y cmake || true

apt-get install -y wget tar

wget "${CMAKE_URL}"

mkdir -p "${INSTALL_DIR}"
tar --strip-components=1 -xzf "${CMAKE_TAR}" -C "${INSTALL_DIR}"

for executable in "${INSTALL_DIR}/bin/"*; do
    exe_name=$(basename "$executable")
    ln -sf "$executable" "/usr/local/bin/$exe_name"
done

rm -f "${CMAKE_TAR}"

echo "Verifying installed versions..."

for executable in cmake ctest cpack; do
    if command -v "$executable" >/dev/null 2>&1; then
        ACTUAL_VERSION=$("$executable" --version | grep -Eo '[0-9]+(\.[0-9]+)*' | head -n 1)
        echo "$executable version: $ACTUAL_VERSION detected"
        if [ "$ACTUAL_VERSION" != "$CMAKE_VERSION" ]; then
            echo "E: $executable version mismatch. Expected $CMAKE_VERSION, found '$ACTUAL_VERSION'" >&2
            exit 1
        fi
    else
        echo "E: $executable is not installed or not in PATH." >&2
        exit 1
    fi
done

