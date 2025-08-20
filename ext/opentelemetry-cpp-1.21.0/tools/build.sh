#!/bin/bash

# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0
set -e

export PATH=/usr/local/bin:$PATH

##
## Install all build tools and dependencies on Mac
##
function install_mac_tools {
  if [ ! -f $BUILDTOOLS_FILE ] ; then
    $WORKSPACE_ROOT/tools/setup-buildtools-mac.sh
    echo > $BUILDTOOLS_FILE
  else
    echo "Build tools already installed. Skipping build tools installation."
  fi
}

##
## Install all build tools and dependencies on Linux
##
function install_linux_tools {
  if [ ! -f $BUILDTOOLS_FILE ] ; then
    sudo $WORKSPACE_ROOT/tools/setup-buildtools.sh
    echo > $BUILDTOOLS_FILE
  else
    echo "Build tools already installed. Skipping build tools installation."
  fi
}

##
## Build dependencies
##
function build_dependencies {
  # Build Google Benchmark
  $WORKSPACE_ROOT/tools/build-benchmark.sh
  # Build Google Test
  $WORKSPACE_ROOT/tools/build-gtest.sh
}

##
## Build specific configuration for a given platform
##
function build {
  echo "Build configuration: $BUILD_CONFIG"
  cd $WORKSPACE_ROOT
  
  export BUILD_ROOT=`pwd`/out/$PLATFORM_NAME/$BUILD_CONFIG
  mkdir -p $BUILD_ROOT
  if [ ! -w $BUILD_ROOT ] ; then
    echo "Unable to create output directory: $BUILD_ROOT"
    exit 1
  fi

  if [ -z ${USE_VCPKG} ] ; then
    # TODO: consider that dependencies may also be coming from OS or brew
    build_dependencies
  else
    echo VCPKG_ROOT=${VCPKG_ROOT}
    # Prefer ninja from VCPKG if available
    NINJA=$WORKSPACE_ROOT/`find tools/vcpkg -name ninja -type f -print -quit`
    if [ -z ${NINJA} ] ; then
      NINJA=`which ninja`
    fi
  fi

  # Build OpenTelemetry SDK
  pushd $BUILD_ROOT
  if [ -z ${NINJA} ] ; then 
    cmake $BUILD_OPTIONS $WORKSPACE_ROOT
    make
  else
    cmake -G "Ninja" $BUILD_OPTIONS $WORKSPACE_ROOT
    echo Building with NINJA=$NINJA
    $NINJA
  fi
  popd
}

function runtests {
  pushd $BUILD_ROOT
  ctest
  popd
}

##
## Clean
##
function clean {
  rm -f CMakeCache.txt *.cmake
  rm -rf out
  rm -rf .buildtools
# make clean
}

##
## Detect compiler
##
function detect_compiler {
  if [ -z "${CC}" ] ; then
    # Compiler autodetection
    if [ -z "${APPLE}" ] ; then
      # Prefer gcc for non-Apple
      if [ -f /usr/bin/gcc ] ; then
        echo "gcc   version: `gcc --version`"
        PLATFORM_NAME=`gcc -dumpmachine`-gcc-`gcc -dumpversion`
      fi
    else
      # Prefer clang on Appple platforms
      if [ -f /usr/bin/clang ] ; then
        echo "clang version: `clang --version`"
        PLATFORM_NAME=`clang -dumpmachine`-clang-`clang -dumpversion`
      fi
    fi
  else
    # Use compiler specified by ${CC} environment variable
    IFS=- read $PLATFORM_NAME $COMPILER_VERSION <<< "${CC}"
    echo "CC version: `${CC} --version`"
    PLATFORM_NAME=$PLATFORM_NAME-`${CC} -dumpversion`
  fi

  if [ -z "${PLATFORM_NAME}" ] ; then
    # Default configuration name for unknown compiler
    # could be overridden by setting env var explicitly
    PLATFORM_NAME=unknown-0
  fi
}

##
## Detect Host OS, install tools and detect compiler
##
function install_tools {

  # Marker file to signal that the tools have been already installed (save build time for incremental builds)
  BUILDTOOLS_FILE=`pwd`/.buildtools

  # Host OS detection
  OS_NAME=`uname -a`
  case "$OS_NAME" in
    *Darwin*)
      export APPLE=1
      # Set target MacOS minver
      export MACOSX_DEPLOYMENT_TARGET=10.10
      install_mac_tools ;;

    *Linux*)
      export LINUX=1
      [[ -z "$NOROOT" ]] && install_linux_tools || echo "No root. Skipping build tools installation." ;;

    *)
      echo "WARNING: unsupported OS $OS_NAME. Skipping build tools installation." ;;
  esac

  detect_compiler
}

##
## Parse arguments
##
function parse_args {
  # Build debug build by default
  if [ "$1" == "release" ] ; then
    BUILD_TYPE="release"
  else
    BUILD_TYPE="debug"
  fi
}

################################################################################################################

## Switch to workspace root directory first
DIR="$( cd "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
WORKSPACE_ROOT=$DIR/..
cd $WORKSPACE_ROOT
echo "Current directory is `pwd`"

# Parse command line arguments
parse_args

# Install the necessary build tools if needed
[[ -z "$NOROOT" ]] && install_tools || echo "No root: skipping build tools installation."

# Build given configuration. Default configuration is ABI-stable 'nostd::' classes.
# Please refer to CMakeLists.txt for the list of supported build configurations.
BUILD_CONFIG=${1-nostd}

shift
BUILD_OPTIONS="$@"
build
runtests
