REM Copyright The OpenTelemetry Authors
REM SPDX-License-Identifier: Apache-2.0

REM Build with Visual Studio 2015
set "PATH=%ProgramFiles(x86)%\MSBuild\14.0\Bin;%ProgramFiles(x86)%\Microsoft Visual Studio 14.0\VC\bin;%PATH%"

REM ### Note that vcpkg built with 2019/2017 can't be used with 2015!
REM ### Consider to specify custom VCPKG_ROOT for 2015 as follows:
REM
REM set VCPKG_ROOT=C:\vcpkg.2015
REM

set BUILDTOOLS_VERSION=vs2015
set ARCH=x64
if NOT "%1"=="" (
  set ARCH=%1
)
set "CMAKE_GEN=Ninja"
set "VCPKG_VISUAL_STUDIO_PATH=%ProgramFiles(x86)%\Microsoft Visual Studio 14.0"

REM Building with Windows SDK 8.1
set "PATH=%ProgramFiles(x86)%\Windows Kits\8.1\bin\%ARCH%;%PATH%"
set WINSDK_VERSION=8.1
set CMAKE_SYSTEM_VERSION=8.1
set VCPKG_PLATFORM_TOOLSET=v140

REM ### Replace above Windows SDK 8.1 by Windows 10 SDK if necessary.
REM ### Resulting binaries may not be compatible with Windows 8
REM
REM set WINSDK_VERSION=10.0.19041.0
REM set "PATH=%ProgramFiles(x86)%\Windows Kits\10\bin\10.0.19041.0\%ARCH%\;%PATH%"
REM

cd %~dp0
call setup-buildtools.cmd
call build.cmd -DMSVC_TOOLSET_VERSION=140
