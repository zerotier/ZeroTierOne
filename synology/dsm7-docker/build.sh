#!/bin/bash

ZTO_VER=$(git describe --abbrev=0 --tags)
ZTO_COMMIT=$(git rev-parse HEAD)

build()
{
  sudo docker build --load --rm -t zerotier-synology . --build-arg ZTO_COMMIT=${ZTO_COMMIT} --build-arg ZTO_VER=${ZTO_VER}
  LATEST_DOCKER_IMAGE_HASH=$(sudo docker images -q zerotier-synology)
  sudo docker tag ${LATEST_DOCKER_IMAGE_HASH} zerotier/zerotier-synology:${ZTO_VER}
  sudo docker tag ${LATEST_DOCKER_IMAGE_HASH} zerotier/zerotier-synology:latest
}

push()
{
  sudo docker login --username=${DOCKERHUB_USERNAME}
  sudo docker push zerotier/zerotier-synology:${ZTO_VER}
  sudo docker push zerotier/zerotier-synology:latest
}

"$@"
