#!/bin/bash

# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

set -e

brew install bazelisk
sudo ln -s /usr/local/bin/bazelisk /usr/local/bin/bazel
