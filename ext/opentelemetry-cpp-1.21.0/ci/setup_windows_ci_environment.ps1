# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

$ErrorActionPreference = "Stop"
trap { $host.SetShouldExit(1) }

git submodule update -f "tools/vcpkg"
Push-Location -Path "tools/vcpkg"
$VCPKG_DIR = (Get-Item -Path ".\").FullName
./bootstrap-vcpkg.bat
./vcpkg integrate install

# Google Benchmark
./vcpkg "--vcpkg-root=$VCPKG_DIR" install benchmark:x64-windows

# Google Test
./vcpkg "--vcpkg-root=$VCPKG_DIR" install gtest:x64-windows

# nlohmann-json
./vcpkg "--vcpkg-root=$VCPKG_DIR" install nlohmann-json:x64-windows

# grpc
./vcpkg "--vcpkg-root=$VCPKG_DIR" install grpc:x64-windows

# curl
./vcpkg "--vcpkg-root=$VCPKG_DIR" install curl:x64-windows

# prometheus-cpp
./vcpkg "--vcpkg-root=$VCPKG_DIR" install prometheus-cpp:x64-windows

Pop-Location
