REM Copyright The OpenTelemetry Authors
REM SPDX-License-Identifier: Apache-2.0

REM Build with Visual Studio 2017
set "BUILDTOOLS_VERSION=vs2017"
set ARCH=x64
if NOT "%1"=="" (
  set ARCH=%1
)

REM ### Uncomment below to use Visual Studio MSBuild solution.
REM ### Ninja generator produces much faster builds. But it is
REM ### easier to debug MSBuild solution in vs2017 IDE :
REM
REM set "CMAKE_GEN=Visual Studio 15 2017"
REM

cd %~dp0
call setup-buildtools.cmd
call build.cmd
