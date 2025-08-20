#!/bin/sh

# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0
set -e

# Try to autodetect the tools dir
if [ "$BASH_SOURCE" != "" ]; then
TOOLS_PATH=`dirname ${BASH_SOURCE[0]}`
else
TOOLS_PATH=`pwd`/tools
fi
echo "Tools directory: $TOOLS_PATH"

# Install build tools if not installed yet
FILE=.buildtools
OS_NAME=`uname -a`
if [ ! -f $FILE ]; then
case "$OS_NAME" in
 *Darwin*) tools/setup-buildtools-mac.sh ;;
 *Linux*)  [[ -z "$NOROOT" ]] && sudo $TOOLS_PATH/setup-buildtools.sh || echo "No root: skipping build tools installation." ;;
 *)        echo "WARNING: unsupported OS $OS_NAME , skipping build tools installation.."
esac
# Assume that the build tools have been successfully installed
echo > $FILE
else
echo Build tools have been already installed, skipping installation.
fi

# Configure git aliases for current session
export PATH=$TOOLS_PATH:$PATH
git config alias.cl '!git-cl.sh'
