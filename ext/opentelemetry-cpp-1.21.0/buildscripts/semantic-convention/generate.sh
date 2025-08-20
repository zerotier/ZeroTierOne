#!/bin/bash

# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

#
# Adapted from:
# opentelemetry-java/buildscripts/semantic-convention/generate.sh
# for opentelemetry-cpp
#
set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
ROOT_DIR="${SCRIPT_DIR}/../../"

# freeze the spec & generator tools versions to make the generation reproducible

# repository: https://github.com/open-telemetry/semantic-conventions
SEMCONV_VERSION=1.34.0

# repository: https://github.com/open-telemetry/weaver
WEAVER_VERSION=0.15.0

SEMCONV_VERSION_TAG=v$SEMCONV_VERSION
WEAVER_VERSION_TAG=v$WEAVER_VERSION
SCHEMA_URL="https://opentelemetry.io/schemas/${SEMCONV_VERSION}"
INCUBATING_DIR=incubating

cd ${SCRIPT_DIR}

rm -rf semantic-conventions || true
mkdir semantic-conventions
cd semantic-conventions

git init
git remote add origin https://github.com/open-telemetry/semantic-conventions.git
git fetch origin "$SEMCONV_VERSION_TAG"
git reset --hard FETCH_HEAD
cd ${SCRIPT_DIR}

# SELINUX
# https://docs.docker.com/storage/bind-mounts/#configure-the-selinux-label

USE_MOUNT_OPTION=""

if [ -x "$(command -v getenforce)" ]; then
  SELINUXSTATUS=$(getenforce);
  if [ "${SELINUXSTATUS}" == "Enforcing" ]; then
    echo "Detected SELINUX"
    USE_MOUNT_OPTION=":z"
  fi;
fi

# DOCKER
# ======
#
# docker is a root container
#
# MY_UID=$(id -u)
# MY_GID=$(id -g)
# docker --user ${MY_UID}:${MY_GID}
#
# PODMAN
# ======
#
# podman is a rootless container
# docker is an alias to podman
#
# docker --user 0:0

MY_UID=$(id -u)
MY_GID=$(id -g)

if [ -x "$(command -v docker)" ]; then
  PODMANSTATUS=$(docker -v | grep -c podman);
  if [ "${PODMANSTATUS}" -ge "1" ]; then
    echo "Detected PODMAN"
    # podman is a rootless container.
    # Execute the docker image as root,
    # to avoid creating files as weaver:weaver in the container,
    # so files end up created as the local user:group outside the container.
    MY_UID="0"
    MY_GID="0"
    # Possible alternate solution: --userns=keep-id
  fi;
fi

generate() {
  TARGET=$1
  OUTPUT=$2
  FILTER=$3
  docker run --rm --user ${MY_UID}:${MY_GID} \
    -v ${SCRIPT_DIR}/semantic-conventions/model:/source${USE_MOUNT_OPTION} \
    -v ${SCRIPT_DIR}/templates:/templates${USE_MOUNT_OPTION} \
    -v ${ROOT_DIR}/tmpgen/:/output${USE_MOUNT_OPTION} \
    otel/weaver:$WEAVER_VERSION_TAG \
    registry \
    generate \
    --registry=/source \
    --templates=/templates \
    ${TARGET} \
    /output/${TARGET} \
    --param output=${OUTPUT} \
    --param filter=${FILTER} \
    --param schema_url=${SCHEMA_URL}
}

# stable attributes and metrics
mkdir -p ${ROOT_DIR}/tmpgen
generate "./" "./" "stable"

mkdir -p ${ROOT_DIR}/tmpgen/${INCUBATING_DIR}
generate "./" "./${INCUBATING_DIR}/" "any"

cp -r ${ROOT_DIR}/tmpgen/*.h \
      ${ROOT_DIR}/api/include/opentelemetry/semconv/

cp -r ${ROOT_DIR}/tmpgen/${INCUBATING_DIR}/*.h \
      ${ROOT_DIR}/api/include/opentelemetry/semconv/incubating

