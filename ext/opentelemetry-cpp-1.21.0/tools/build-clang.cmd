REM Copyright The OpenTelemetry Authors
REM SPDX-License-Identifier: Apache-2.0

@echo off
pushd %~dp0
set "PATH=%ProgramFiles%\LLVM\bin;%PATH%"
set BUILDTOOLS_VERSION=clang
set CMAKE_GEN=Ninja
call build.cmd %*
popd
