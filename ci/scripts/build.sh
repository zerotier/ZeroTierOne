#!/bin/bash
set -euo pipefail
IFS=$'\n\t'

export PLATFORM=$1
export ZT_ISA=$2
export VERSION=$3
export EVENT=$4

case $PLATFORM in
    sid)
        export PKGFMT=none
        ;;
    el*|fc*|amzn*)
        export PKGFMT=rpm
        ;;
    *)
        export PKGFMT=deb
esac

#
# Allow user to drop in custom Dockerfile for PLATFORM
#

if [ -f "ci/Dockerfile.${PLATFORM}" ]; then
    export DOCKERFILE="ci/Dockerfile.${PLATFORM}"
else
    export DOCKERFILE="ci/Dockerfile.${PKGFMT}"
fi

#
# Rust sometimes gets confused about where it's running.
# Normally, the build images will have Rust pre-baked.
# Pass RUST_TRIPLET for convenience when using a custom Dockerfile
#

case $ZT_ISA in
    386)
        export DOCKER_ARCH=386
        export RUST_TRIPLET=i686-unknown-linux-gnu
        ;;
    amd64)
        export DOCKER_ARCH=amd64
        export RUST_TRIPLET=x86_64-unknown-linux-gnu
        ;;
    armv7)
        export DOCKER_ARCH=arm/v7
        export RUST_TRIPLET=armv7-unknown-linux-gnueabihf
        ;;
    arm64)
        export DOCKER_ARCH=arm64/v8
        export RUST_TRIPLET=aarch64-unknown-linux-gnu
        ;;
    riscv64)
        export DOCKER_ARCH=riscv64
        export RUST_TRIPLET=riscv64gc-unknown-linux-gnu
        ;;
    ppc64le)
        export DOCKER_ARCH=ppc64le
        export RUST_TRIPLET=powerpc64le-unknown-linux-gnu
        ;;
    mips64le)
        export DOCKER_ARCH=mips64le
        export RUST_TRIPLET=mips64el-unknown-linux-gnuabi64
        ;;
    s390x)
        export DOCKER_ARCH=s390x
        export RUST_TRIPLET=s390x-unknown-linux-gnu
        ;;
    *)
        echo "ERROR: could not determine architecture settings. PLEASE FIX ME"
        exit 1
        ;;
esac

#
# Print debug info
#

echo "#~~~~~~~~~~~~~~~~~~~~"
echo "$0 variables:"
echo "nproc: $(nproc)"
echo "ZT_ISA: ${ZT_ISA}"
echo "DOCKER_ARCH: ${DOCKER_ARCH}"
echo "RUST_TRIPLET: ${RUST_TRIPLET}"
echo "VERSION: ${VERSION}"
echo "EVENT: ${EVENT}"
echo "PKGFMT: ${PKGFMT}"
echo "PWD: ${PWD}"
echo "DOCKERFILE: ${DOCKERFILE}"
echo "#~~~~~~~~~~~~~~~~~~~~"

#
# Munge RPM and Deb
#

if [ ${PKGFMT} != "none" ] && [ ${EVENT} != "tag" ]; then
    make munge_rpm zerotier-one.spec VERSION=${VERSION}
    make munge_deb debian/changelog VERSION=${VERSION}
fi

#
# Assemble buildx arguments
#

build_args=(
    --no-cache
    --build-arg PLATFORM=${PLATFORM}
    --build-arg RUST_TRIPLET=${RUST_TRIPLET}
    --build-arg DOCKER_ARCH=${DOCKER_ARCH}
    --platform linux/${DOCKER_ARCH}
    -f ${DOCKERFILE}
    -t build
    .
)

if [ ${PKGFMT} != "none" ]; then
    build_args+=("--output type=local,dest=.")
    build_args+=("--target export")
fi

#
# Do build
#

docker buildx build ${build_args[@]}
