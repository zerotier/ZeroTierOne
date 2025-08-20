REM Copyright The OpenTelemetry Authors
REM SPDX-License-Identifier: Apache-2.0

@echo off
set BUILDTOOLS_VERSION=vs2019
set CMAKE_GEN="Visual Studio 16 2019"
echo Building Google Benchmark (test only dependency)...
@setlocal ENABLEEXTENSIONS

echo Auto-detecting Visual Studio version...
call "%~dp0\vcvars.cmd"

pushd "%~dp0\.."
set "ROOT=%CD%"

set MAXCPUCOUNT=%NUMBER_OF_PROCESSORS%
set platform=

if not exist "%ROOT%\third_party\benchmark\" (
  echo "Google Benchmark library is not available, skipping benchmark build."
  call skip_the_build
)

cd "%ROOT%\third_party\benchmark\"
set "GOOGLETEST_PATH=%ROOT%\third_party\googletest"
if not exist "build" (
  mkdir build
)
cd build

REM By default we generate the project for the older Visual Studio 2017 even if we have newer version installed
cmake ../ -G %CMAKE_GEN% -Ax64 -DBENCHMARK_ENABLE_TESTING=OFF
set SOLUTION=%ROOT%\third_party\benchmark\build\benchmark.sln
msbuild %SOLUTION% /maxcpucount:%MAXCPUCOUNT% /p:Configuration=Debug /p:Platform=x64
msbuild %SOLUTION% /maxcpucount:%MAXCPUCOUNT% /p:Configuration=Release /p:Platform=x64
popd

:skip_the_build
