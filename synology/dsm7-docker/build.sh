#!/bin/bash

ZTO_VER=$(jq -r '.version' ../config.json)

build()
{
  sudo docker build --load --rm -t zerotier-synology .
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
