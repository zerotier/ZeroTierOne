#!/bin/sh

# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0
set -e


# TODO: it's not ideal experience, but we use have to use brew-provided deps.
# Sometimes we might run into a situation where a different user takes over
# control of the brew dirs. That causes the brew update to fail.
# Temporarily allow the user to take over control of brew files.

echo ***
echo *** You may need to enter your admin password to update the brew files:
echo ***

which -s brew
if [[ $? != 0 ]] ; then
    # Install Homebrew
    ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
else
    # FIXME: do we always require the brew update??
    brew update
fi

sudo chown -R $(whoami) /usr/local/Cellar
sudo chown -R $(whoami) /usr/local/Homebrew
sudo chown -R $(whoami) /usr/local/var/homebrew
sudo chown -R $(whoami) /usr/local/etc/bash_completion.d /usr/local/include /usr/local/lib/pkgconfig /usr/local/share/aclocal /usr/local/share/locale /usr/local/share/zsh /usr/local/share/zsh/site-functions /usr/local/var/homebrew/locks

brew install cmake
brew install coreutils
brew install wget
brew install clang-format
brew install google-benchmark
brew tap nlohmann/json
brew install nlohmann-json
brew install abseil
