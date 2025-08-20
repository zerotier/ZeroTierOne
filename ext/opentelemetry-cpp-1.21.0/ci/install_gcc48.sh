#!/bin/bash

# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

set -e
apt-get update
apt-get install --no-install-recommends --no-install-suggests -y \
  g++-4.8
