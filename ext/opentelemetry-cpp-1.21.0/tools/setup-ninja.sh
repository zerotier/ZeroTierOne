#!/bin/bash

# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0
set -e

# TODO: add support for Ninja on Mac OS X
wget -O /tmp/ninja.zip https://github.com/ninja-build/ninja/releases/download/v1.10.1/ninja-linux.zip
sudo unzip /tmp/ninja.zip -d /usr/local/bin/
