REM Copyright The OpenTelemetry Authors
REM SPDX-License-Identifier: Apache-2.0

@echo off
REM +-------------------------------------------------------------------+
REM | Autodetect and set up the build environment.                      |
REM | Build Tools version may be specified as 1st argument.             |
REM +-------------------------------------------------------------------+
REM | Description                             | Argument value          |
REM +-----------------------------------------+-------------------------+
REM | Autodetect Visual Studio 2022           | vs2022                  |
REM | Visual Studio 2022 Enterprise           | vs2022_enterprise       |
REM | Visual Studio 2022 Professional         | vs2022_professional     |
REM | Visual Studio 2022 Community            | vs2022_community        |
REM | Visual Studio 2022 Build Tools (no IDE) | vs2022_buildtools       |
REM |                                         |                         |
REM | Autodetect Visual Studio 2019           | vs2019                  |
REM | Visual Studio 2019 Enterprise           | vs2019_enterprise       |
REM | Visual Studio 2019 Professional         | vs2019_professional     |
REM | Visual Studio 2019 Community            | vs2019_community        |
REM | Visual Studio 2019 Build Tools (no IDE) | vs2019_buildtools       |
REM |                                         |                         |
REM | Autodetect Visual Studio 2017           | vs2017                  |
REM | Visual Studio 2017 Enterprise           | vs2017_enterprise       |
REM | Visual Studio 2017 Professional         | vs2017_professional     |
REM | Visual Studio 2017 Community            | vs2017_community        |
REM | Visual Studio 2017 Build Tools (no IDE) | vs2017_buildtools       |
REM |                                         |                         |
REM | Visual Studio 2015 Build Tools (no IDE) | vs2015                  |
REM |                                         |                         |
REM | LLVM Clang (any version)                | clang                   |
REM | LLVM Clang 9                            | clang-9                 |
REM | LLVM Clang 10                           | clang-10                |
REM | LLVM Clang 11                           | clang-11                |
REM | LLVM Clang 11                           | clang-12                |
REM +-----------------------------------------+-------------------------+
set "VSCMD_START_DIR=%CD%"

if not defined ARCH (
  set ARCH=x64
)

if "%1" neq "" (
  goto %1
)

if defined BUILDTOOLS_VERSION (
  goto %BUILDTOOLS_VERSION%
)

:vs2022
:vs2022_enterprise
set TOOLS_VS2022_ENTERPRISE="%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"
if exist %TOOLS_VS2022_ENTERPRISE% (
  echo Building with vs2022 Enterprise...
  call %TOOLS_VS2022_ENTERPRISE% %ARCH%
  goto tools_configured
)

:vs2022_professional
set TOOLS_VS2022_PRO="%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat"
if exist %TOOLS_VS2022_PRO% (
  echo Building with vs2022 Professional...
  call %TOOLS_VS2022_PRO% %ARCH%
  goto tools_configured
)

:vs2022_community
set TOOLS_VS2022_COMMUNITY="%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
if exist %TOOLS_VS2022_COMMUNITY% (
  echo Building with vs2022 Community...
  call %TOOLS_VS2022_COMMUNITY% %ARCH%
  goto tools_configured
)

:vs2022_buildtools
set TOOLS_VS2022="%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
if exist %TOOLS_VS2022% (
  echo Building with vs2022 BuildTools...
  call %TOOLS_VS2022% %ARCH%
  goto tools_configured
)

:vs2019
:vs2019_enterprise
set TOOLS_VS2019_ENTERPRISE="%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"
if exist %TOOLS_VS2019_ENTERPRISE% (
  echo Building with vs2019 Enterprise...
  call %TOOLS_VS2019_ENTERPRISE% %ARCH%
  goto tools_configured
)

:vs2019_professional
set TOOLS_VS2019_PRO="%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvarsall.bat"
if exist %TOOLS_VS2019_PRO% (
  echo Building with vs2019 Professional...
  call %TOOLS_VS2019_PRO% %ARCH%
  goto tools_configured
)

:vs2019_community
set TOOLS_VS2019_COMMUNITY="%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat"
if exist %TOOLS_VS2019_COMMUNITY% (
  echo Building with vs2019 Community...
  call %TOOLS_VS2019_COMMUNITY% %ARCH%
  goto tools_configured
)

:vs2019_buildtools
set TOOLS_VS2019="%ProgramFiles(x86)%\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
if exist %TOOLS_VS2019% (
  echo Building with vs2019 BuildTools...
  call %TOOLS_VS2019% %ARCH%
  goto tools_configured
)

:vs2017
:vs2017_enterprise
set TOOLS_VS2017_ENTERPRISE="%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"
if exist %TOOLS_VS2017_ENTERPRISE% (
  echo Building with vs2017 Enterprise...
  call %TOOLS_VS2017_ENTERPRISE% %ARCH%
  goto tools_configured
)

:vs2017_professional
set TOOLS_VS2017_PRO="%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Professional\VC\Auxiliary\Build\vcvarsall.bat"
if exist %TOOLS_VS2017_PRO% (
  echo Building with vs2017 Professional...
  call %TOOLS_VS2017_PRO% %ARCH%
  goto tools_configured
)

:vs2017_community
set TOOLS_VS2017_COMMUNITY="%ProgramFiles(x86)%\Microsoft Visual Studio\2017\VC\Auxiliary\Build\vcvarsall.bat"
if exist %TOOLS_VS2017_COMMUNITY% (
  echo Building with vs2017 Community...
  call %TOOLS_VS2017_COMMUNITY% %ARCH%
  goto tools_configured
)

:vs2017_buildtools
set TOOLS_VS2017="%ProgramFiles(x86)%\Microsoft Visual Studio\2017\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
if exist %TOOLS_VS2017% (
  echo Building with vs2017 BuildTools...
  call %TOOLS_VS2017% %ARCH%
  goto tools_configured
)

:vs2015
set TOOLS_VS2015="%ProgramFiles(x86)%\Microsoft Visual Studio 14.0\VC\vcvarsall.bat"
if exist %TOOLS_VS2015% (
  echo Building with vs2015 BuildTools...
  call %TOOLS_VS2015% %ARCH% %WINSDK_VERSION%
  set "VCPKG_VISUAL_STUDIO_PATH=%ProgramFiles(x86)%\Microsoft Visual Studio 14.0"
  set VCPKG_PLATFORM_TOOLSET=v140
  goto tools_configured
)

echo WARNING: cannot auto-detect Visual Studio version !!!
REM Caller may decide what to do if Visual Studio environment
REM is not set up by checking TOOLS_VS_NOTFOUND
set TOOLS_VS_NOTFOUND=1
exit /b 0

REM +-------------------------------------------------------------------+
REM | There is no auto-detection of LLVM Clang version.                 |
REM | LLVM Clang of any version is installed in the same directory      |
REM | at %ProgramFiles%\LLVM\bin . Developers choose their own custom   |
REM | layout for installing multiple clang toolchains side-by-side.     |
REM |                                                                   |
REM | Example layout (merely a guideline, layout could differ):         |
REM |                                                                   |
REM | %ProgramFiles%\LLVM-9\bin                                         |
REM | %ProgramFiles%\LLVM-10\bin                                        |
REM | %ProgramFiles%\LLVM-11\bin                                        |
REM | %ProgramFiles%\LLVM-12\bin                                        |
REM +-------------------------------------------------------------------+
REM
REM ## Example 1: use clang-10 located in LLVM-10 directory:
REM set BUILDTOOLS_VERSION=clang-10
REM set "PATH=%ProgramFiles%\LLVM-10\bin;%PATH%"
REM tools\build.cmd
REM
REM ## Example 2: use whatever clang located in LLVM directory:
REM set BUILDTOOLS_VERSION=clang
REM set "PATH=%ProgramFiles%\LLVM\bin;%PATH%"
REM tools\build.cmd
REM
REM BUILDTOOLS_VERSION determines the output directory location.
REM Store build artifacts produced by different toolchains -
REM side-by-side, each in its own separate output directory.
REM
:clang
:clang-9
:clang-10
:clang-11
:clang-12

:tools_configured
