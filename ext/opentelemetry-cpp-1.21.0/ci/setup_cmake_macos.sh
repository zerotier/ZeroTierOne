#!/bin/bash

# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

set -e

CMAKE_VERSION="${CMAKE_VERSION:-3.31.6}"
CMAKE_PKG="cmake-${CMAKE_VERSION}-macos-universal"
CMAKE_TAR="${CMAKE_PKG}.tar.gz"
CMAKE_URL="https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/${CMAKE_TAR}"

INSTALL_DIR="/opt/cmake"

echo "Installing CMake version ${CMAKE_VERSION}..."

brew uninstall cmake || true

if ! command -v wget >/dev/null 2>&1; then
  echo "Installing wget..."
  brew install wget
fi

wget -q "${CMAKE_URL}"
sudo mkdir -p "${INSTALL_DIR}"
sudo tar --strip-components=1 -xzf "${CMAKE_TAR}" -C "${INSTALL_DIR}"

BINARY_DIR="${INSTALL_DIR}/CMake.app/Contents/bin"

for executable in "${BINARY_DIR}/"*; do
    exe_name=$(basename "$executable")
    sudo ln -sf "$executable" "/usr/local/bin/$exe_name"
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
