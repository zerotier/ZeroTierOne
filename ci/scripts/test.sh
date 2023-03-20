#!/bin/bash
set -euo pipefail
IFS=$'\n\t'

ZT_NAME="$1" ; shift
DISTRO="$1" ; shift
ZT_ISA="$1" ; shift
VERSION="$1" ; shift
BUILD_EVENT="$1" ; shift

source "$(dirname $0)/lib.sh"

if [ -f "ci/Dockerfile-test.${ZT_NAME}" ]; then
    DOCKERFILE="ci/Dockerfile-test.${ZT_NAME}"
else
    DOCKERFILE="ci/Dockerfile-test.${PKGFMT}"
fi

if [ ${BUILD_EVENT} == "tag" ]; then
    BASEURL="zerotier-releases.ci.lab"
else
    BASEURL="zerotier-builds.ci.lab"
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

docker pull -q --platform="linux/${DOCKER_ARCH}" 084037375216.dkr.ecr.us-east-2.amazonaws.com/${ZT_NAME}-tester

docker buildx build \
       --build-arg BASEURL="${BASEURL}" \
       --build-arg ZT_NAME="${ZT_NAME}" \
       --build-arg DISTRO="${DISTRO}" \
       --build-arg DEB_ARCH="${DEB_ARCH}" \
       --build-arg DNF_ARCH="${DNF_ARCH}" \
       --build-arg VERSION="${VERSION}" \
       --platform "linux/${DOCKER_ARCH}" \
       --no-cache \
       -f ${DOCKERFILE} \
       -t test \
       . \
