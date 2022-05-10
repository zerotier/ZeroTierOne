#!/bin/bash
set -euo pipefail
IFS=$'\n\t'

export GOOS=$1
export GOARCH=$2
export VERSION=$3
export DOCKER_BUILDKIT=1

echo "nproc: $(nproc)"

case $GOARCH in
    armv5)
        export ARCH=arm/v5
        ;;
    armv7)
        export ARCH=arm/v7
        ;;
    arm64)
        export ARCH=arm64/v8
        ;;
    *)
        export ARCH=$GOARCH
        ;;
esac

docker run --privileged --rm tonistiigi/binfmt --install all
docker pull registry.sean.farm/sid-builder
docker buildx build --platform ${GOOS}/${ARCH} -f ci/Dockerfile.sid --target export -t test . --output out/${GOOS}/${GOARCH}
