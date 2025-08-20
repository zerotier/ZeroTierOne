#!/bin/bash

# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

set -ex

PKG_CONFIG_PATH="/usr/local/lib64/pkgconfig:${PKG_CONFIG_PATH:-}"

for library in api common logs metrics resources trace version; do
  pkg-config --validate opentelemetry_${library} --print-errors
done
