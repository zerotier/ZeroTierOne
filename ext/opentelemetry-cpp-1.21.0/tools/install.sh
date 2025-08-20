#!/bin/bash

# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0
set -e

DIR="$( cd "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
WORKSPACE_ROOT=$DIR/..
pushd $WORKSPACE_ROOT
OPENTELEMETRY_INSTALL_DIR=${1-/usr/local}
echo "Install SDK to $OPENTELEMETRY_INSTALL_DIR"
mkdir -p $OPENTELEMETRY_INSTALL_DIR/lib
# TODO: install libraries?
cp -R api/include $OPENTELEMETRY_INSTALL_DIR
cp -R sdk/include/opentelemetry/sdk $OPENTELEMETRY_INSTALL_DIR/include/opentelemetry
mkdir -p $OPENTELEMETRY_INSTALL_DIR/lib
echo dummy > $OPENTELEMETRY_INSTALL_DIR/lib/libopentelemetry.stub
popd
