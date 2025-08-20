REM Copyright The OpenTelemetry Authors
REM SPDX-License-Identifier: Apache-2.0

REM Build with Visual Studio 2019
set "BUILDTOOLS_VERSION=vs2019"
set ARCH=x64
if NOT "%1"=="" (
  set ARCH=%1
)
if "%ARCH%"=="x64" (
  REM Parameter needed for CMake Visual Studio 2019 generator
  set CMAKE_ARCH=x64
)
cd %~dp0
call setup-buildtools.cmd
call build.cmd
