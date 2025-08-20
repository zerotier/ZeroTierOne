REM Copyright The OpenTelemetry Authors
REM SPDX-License-Identifier: Apache-2.0

@echo off
pushd %~dp0
set "PATH=%ProgramFiles%\LLVM-12\bin;%PATH%"
set BUILDTOOLS_VERSION=clang-12
set CMAKE_GEN=Ninja
call build.cmd %*
popd
