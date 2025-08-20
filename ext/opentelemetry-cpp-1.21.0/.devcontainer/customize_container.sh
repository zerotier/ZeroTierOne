#!/bin/bash

# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

set -eu

if [[ $IS_CONTAINER_BUILD != "true" ]]; then
     echo "This script should only run inside a Docker container."
     exit 1
fi

if [[ -n "$INSTALL_PACKAGES" ]]; then
    packages=($INSTALL_PACKAGES)
    for package in "${packages[@]}"; do
        apt install -y "$package"
    done
fi

if [[ $(id "$USER_NAME" 2>/dev/null) ]]; then
    echo "User '$USER_NAME' already exists. Removing it."
    userdel -rf "$USER_NAME"
elif [[ $(id -u "$USER_UID" 2>/dev/null) ]]; then
    OTHER_USER=$(getent passwd "$USER_UID" | cut -d: -f1)
    echo "User '$OTHER_USER' exists with UID $USER_UID. Removing it."
    userdel -rf "$OTHER_USER"
fi

if [[ ! $(getent group "$USER_GID" 2>/dev/null) ]]; then
    echo "Group '$USER_GID' does not exist. Adding it."
    groupadd -g "$USER_GID" "$USER_NAME"
fi

useradd -m -u "$USER_UID" -g "$USER_GID" -s /bin/bash "$USER_NAME"
echo "Created user '$USER_NAME' (UID: $USER_UID, GID: $USER_GID)."

echo "$USER_NAME ALL=(ALL) NOPASSWD:ALL" | tee /etc/sudoers.d/"$USER_NAME"

echo "User and group setup complete."
