#!/bin/bash

# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

if [[ ! -e tools/format.sh ]]; then
  echo "This tool must be run from the topmost directory." >&2
  exit 1
fi

set -e

FIND="find . -name third_party -prune -o -name tools -prune -o -name .git -prune -o -name _deps -prune -o -name .build -prune -o -name out -prune -o -name .vs -prune -o -name opentelemetry_logo.png -prune -o -name TraceLoggingDynamic.h -prune -o"

# GNU syntax.
SED=(sed -i)
if [[ "$(uname)" = "Darwin" ]]; then
  SED=(sed -i "")
fi

# Correct common miscapitalizations.
"${SED[@]}" 's/Open[t]elemetry/OpenTelemetry/g' $($FIND -type f -print)
# No CRLF line endings, except Windows files.
"${SED[@]}" 's/\r$//' $($FIND -name '*.ps1' -prune -o \
  -name '*.cmd' -prune -o -type f -print)
# No trailing spaces, except in patch.
"${SED[@]}" 's/ \+$//' $($FIND -name "*.patch" -prune -o -type f -print)

# If not overridden, try to use clang-format-18 or clang-format.
if [[ -z "$CLANG_FORMAT" ]]; then
  CLANG_FORMAT=clang-format
  if which clang-format-18 >/dev/null; then
    CLANG_FORMAT=clang-format-18
  fi
fi

$CLANG_FORMAT -version
$CLANG_FORMAT -i -style=file \
  $($FIND -name '*.cc' -print -o -name '*.h' -print)

if which cmake-format >/dev/null; then
  echo "Running cmake-format $(cmake-format --version 2>&1)."
  cmake-format -i $($FIND -name 'CMakeLists.txt' -print -name '*.cmake' -print -name '*.cmake.in' -print)
else
  echo "Can't find cmake-format. It can be installed with:"
  echo "  pip install --user cmake_format"
  exit 1
fi

if [[ -z "$BUILDIFIER" ]]; then
  BUILDIFIER="$HOME/go/bin/buildifier"
  if ! which "$BUILDIFIER" >/dev/null; then
    BUILDIFIER=buildifier
  fi
fi
if which "$BUILDIFIER" >/dev/null; then
  echo "Running $BUILDIFIER"
  "$BUILDIFIER" $($FIND -name WORKSPACE -print -o -name BUILD -print -o \
    -name '*.BUILD' -o -name '*.bzl' -print)
else
  echo "Can't find buildifier. It can be installed with:"
  echo "  go get github.com/bazelbuild/buildtools/buildifier"
  exit 1
fi
