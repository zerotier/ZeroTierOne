#!/bin/sh

# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0
set -e

if [ "format" = "$1" ]; then
  if [ -z "$2" ]; then
    echo Please specify file name.
    exit
  fi

  if [ -f "$2" ]; then
    FILEPATH=$2
  else
    FILEPATH=${GIT_PREFIX}$2
  fi
  echo Formatting $FILEPATH
  clang-format -style=file -i $FILEPATH
fi
