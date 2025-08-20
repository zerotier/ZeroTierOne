#!/bin/bash

# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

set -e
export DEBIAN_FRONTEND=noninteractive
old_grpc_version='v1.33.2'
new_grpc_version='v1.49.2'
modern_grpc_version='v1.55.0'
gcc_version_for_new_grpc='5.1'
std_version='14'
if [ ! -z "${CXX_STANDARD}" ]; then
    std_version="${CXX_STANDARD}"
fi
install_grpc_version=${new_grpc_version}
install_dir='/usr/local/'
build_shared_libs=''
build_internal_abseil_cpp=1
GRPC_BUILD_OPTIONS=()
usage() {
    echo "Usage: $0 [options...]" 1>&2;
    echo "Available options:" 1>&2;
    echo "    -v <gcc-version>              Set GCC version" 1>&2;
    echo "    -h                            Show help message and exit" 1>&2;
    echo "    -i <install_dir>              Set installation prefix" 1>&2;
    echo "    -m                            Use the modern gRPC version" 1>&2;
    echo "    -p <protobuf|abseil-cpp>      Let gRPC find protobuf or abseil-cpp by CONFIG package" 1>&2;
    echo "    -r <gRPC version>             Specify the version of gRPC" 1>&2;
    echo "    -s <STD version>              Specify std version(CMAKE_CXX_STANDARD)" 1>&2;
    echo "    -T                            Build static libraries" 1>&2;
    echo "    -H                            Build shared libraries" 1>&2;
}

while getopts ":v:hi:mp:r:s:TH" o; do
    case "${o}" in
        v)
            gcc_version=${OPTARG}
            ;;
        h)
            usage
            exit 0;
            ;;
        i)
            install_dir=${OPTARG}
            ;;
        p)
            if [ "${OPTARG}" == "protobuf" ]; then
                GRPC_BUILD_OPTIONS=(${GRPC_BUILD_OPTIONS[@]} "-DgRPC_PROTOBUF_PROVIDER=package")
            elif [ "${OPTARG}" == "abseil-cpp" ]; then
                GRPC_BUILD_OPTIONS=(${GRPC_BUILD_OPTIONS[@]} "-DgRPC_ABSL_PROVIDER=package")
                build_internal_abseil_cpp=0
            else
                usage
                exit 1;
            fi
            ;;
        r)
            install_grpc_version=${OPTARG}
            ;;
        m)
            install_grpc_version=${modern_grpc_version}
            ;;
        s)
            std_version=${OPTARG}
            ;;
        T)
            build_shared_libs="OFF"
            ;;
        H)
            build_shared_libs="ON"
            ;;
        *)
            usage
            exit 1;
            ;;
    esac
done

if [ -z "${gcc_version}" ]; then
    gcc_version=`gcc --version | awk '/gcc/  {print $NF}'`
fi
if [[ "${gcc_version}" < "${gcc_version_for_new_grpc}" ]] && [[ "${gcc_version:1:1}" == "." ]]; then
    echo "${gcc_version} less than ${gcc_version_for_new_grpc}"
    std_version='11'
    install_grpc_version=${old_grpc_version}
fi
if ! type cmake > /dev/null; then
    #cmake not installed, exiting
    exit 1
fi
export BUILD_DIR=/tmp/
export INSTALL_DIR=${install_dir}
pushd $BUILD_DIR
echo "installing grpc version: ${install_grpc_version}"
git clone --depth=1 -b ${install_grpc_version} https://github.com/grpc/grpc
pushd grpc
git submodule init
git submodule update --depth 1

if [[ $build_internal_abseil_cpp -ne 0 ]]; then
    mkdir -p "third_party/abseil-cpp/build" && pushd "third_party/abseil-cpp/build"
    set -x

    ABSEIL_CPP_BUILD_OPTIONS=(
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_CXX_STANDARD=${std_version}
        -DCMAKE_CXX_STANDARD_REQUIRED=ON
        -DCMAKE_CXX_EXTENSIONS=OFF
        -DCMAKE_POSITION_INDEPENDENT_CODE=TRUE
        -DCMAKE_INSTALL_PREFIX=$INSTALL_DIR
    )
    if [ ! -z "$build_shared_libs" ]; then
        ABSEIL_CPP_BUILD_OPTIONS=(${ABSEIL_CPP_BUILD_OPTIONS[@]} "-DBUILD_SHARED_LIBS=$build_shared_libs")
    fi
    cmake  "${ABSEIL_CPP_BUILD_OPTIONS[@]}" ..
    make -j $(nproc)
    make install
    popd
fi
mkdir -p build && pushd build

GRPC_BUILD_OPTIONS=(
    ${GRPC_BUILD_OPTIONS[@]}
    -DgRPC_INSTALL=ON
    -DCMAKE_CXX_STANDARD=${std_version}
    -DCMAKE_CXX_STANDARD_REQUIRED=ON
    -DCMAKE_CXX_EXTENSIONS=OFF
    -DgRPC_BUILD_TESTS=OFF
    -DgRPC_BUILD_GRPC_CPP_PLUGIN=ON
    -DgRPC_BUILD_GRPC_CSHARP_PLUGIN=OFF
    -DgRPC_BUILD_GRPC_OBJECTIVE_C_PLUGIN=OFF
    -DgRPC_BUILD_GRPC_PHP_PLUGIN=OFF
    -DgRPC_BUILD_GRPC_NODE_PLUGIN=OFF
    -DgRPC_BUILD_GRPC_PYTHON_PLUGIN=OFF
    -DgRPC_BUILD_GRPC_RUBY_PLUGIN=OFF
    -DgRPC_BUILD_GRPCPP_OTEL_PLUGIN=OFF
    -DCMAKE_INSTALL_PREFIX=$INSTALL_DIR
    -DCMAKE_PREFIX_PATH=$INSTALL_DIR
)
if [ ! -z "$build_shared_libs" ]; then
    GRPC_BUILD_OPTIONS=(${GRPC_BUILD_OPTIONS[@]} "-DBUILD_SHARED_LIBS=$build_shared_libs")
fi

echo "Building gRPC ${install_grpc_version}"
echo "CMake build options:" "${GRPC_BUILD_OPTIONS[@]}"

cmake "${GRPC_BUILD_OPTIONS[@]}" ..
make -j $(nproc)
make install
popd
popd

export PATH=${INSTALL_DIR}/bin:$PATH  # ensure to use the installed grpc
