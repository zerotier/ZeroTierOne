# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

$ErrorActionPreference = "Stop"
trap { $host.SetShouldExit(1) }

cd tools/vcpkg
# Lock to specific version of Protobuf port file to avoid build break
./vcpkg "--vcpkg-root=$PWD" install --overlay-ports="$PSScriptRoot\ports" protobuf:x64-windows
