#!/bin/bash
set -e
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
pushd $DIR/../../ > /dev/null
basename -s .git `git config --get remote.origin.url`
popd > /dev/null
