#!/bin/bash

ZTO_VER=$(git describe --tags $(git rev-list --tags --max-count=1))
ZTO_COMMIT=$(git rev-parse HEAD)

build() {
  sudo docker login --username=${DOCKERHUB_USERNAME}

  sudo docker buildx build \
    --push \
    --platform linux/arm/v7,linux/arm64/v8,linux/amd64 \
    --tag zerotier/zerotier-synology:${ZTO_VER} \
    --tag zerotier/zerotier-synology:latest \
    --build-arg ZTO_COMMIT=${ZTO_COMMIT} \
    --build-arg ZTO_VER=${ZTO_VER} \
    .
}

"$@"
