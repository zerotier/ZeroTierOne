#!/bin/bash

# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

set -e
apt-get update
apt-get install --no-install-recommends --no-install-suggests -y \
                build-essential \
                ca-certificates \
                wget \
                git \
                valgrind \
                lcov \
                iwyu \
                pkg-config
