#!/bin/bash
set -euo pipefail
IFS=$'\n\t'

ZT_NAME="$1" ; shift
DISTRO="$1" ; shift
ZT_ISA="$1" ; shift
VERSION="$1" ; shift
BUILD_EVENT="$1" ; shift

source "$(dirname $0)/lib.sh"

if [ -f "ci/Dockerfile-build.${ZT_NAME}" ]; then
    DOCKERFILE="ci/Dockerfile-build.${ZT_NAME}"
    echo "USING DOCKERFILE ${DOCKERFILE}"
else
    DOCKERFILE="ci/Dockerfile-build.${PKGFMT}"
    echo "USING DOCKERFILE ${DOCKERFILE}"    
fi

echo "#~~~~~~~~~~~~~~~~~~~~"
echo "$0 variables:"
echo "nproc: $(nproc)"
echo "ZT_NAME: ${ZT_NAME}"
echo "DISTRO: ${DISTRO}"
echo "ZT_ISA: ${ZT_ISA}"
echo "VERSION: ${VERSION}"
echo "BUILD_EVENT: ${BUILD_EVENT}"
echo "DOCKER_ARCH: ${DOCKER_ARCH}"
echo "DNF_ARCH: ${DNF_ARCH}"
echo "RUST_TRIPLET: ${RUST_TRIPLET}"
echo "PKGFMT: ${PKGFMT}"
echo "PWD: ${PWD}"
echo "DOCKERFILE: ${DOCKERFILE}"
echo "#~~~~~~~~~~~~~~~~~~~~"

# if [ ${BUILD_EVENT} == "push" ]; then
    make munge_rpm zerotier-one.spec VERSION=${VERSION}
    make munge_deb debian/changelog VERSION=${VERSION}
# fi

docker buildx build \
       --build-arg ZT_NAME="${ZT_NAME}" \
       --build-arg RUST_TRIPLET="${RUST_TRIPLET}" \
       --build-arg DOCKER_ARCH="${DOCKER_ARCH}" \
       --build-arg DNF_ARCH="${DNF_ARCH}" \
       --platform linux/${DOCKER_ARCH} \
       -f ${DOCKERFILE} \
       -t build \
       . \
       --output type=local,dest=. \
       --target export
