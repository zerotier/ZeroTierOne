REM Copyright The OpenTelemetry Authors
REM SPDX-License-Identifier: Apache-2.0

pushd "%~dp0"
set "PATH=%CD%;%PATH%"
cd ..
bazel.exe build %* //...
bazel.exe test %* //...
popd
