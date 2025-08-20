#!/bin/bash

# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0
set -e

usage() { echo "Usage: $0 -t <tag>" 1>&2; exit 1; }

while getopts ":t:" o; do
    case "${o}" in
        t)
            tag=${OPTARG}
            ;;
        *)
            usage
            ;;
    esac
done
if [ ! -z "${t}" ]; then
    usage
fi

#validate tag
semver_regex="^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)(\\-[0-9A-Za-z-]+(\\.[0-9A-Za-z-]+)*)?(\\+[0-9A-Za-z-]+(\\.[0-9A-Za-z-]+)*)?$"
if [[ ${tag} =~ ${semver_regex} ]]; then
    echo "${tag} is valid semver tag"
else
    echo "Error: ${tag} is not a valid semver tag. Exiting"
    exit 1
fi

#ensure tag doesn't exits
if [[ $(git tag --list ${tag}) ]]; then
   echo "Error: Tag ${tag} already exists. Exiting"
   exit 1
fi

if ! git diff --quiet; then \
	echo "Error: Working tree is not clean, can't proceed with the release process\n"
	git status
	git diff
	exit 1
fi

# check changelog exists
changelog_file="./CHANGELOG.md"
if [ ! -f $changelog_file ]; then
    echo "Error: $changelog_file doesn't exist. Ensure that you are running this script from repo root directory "
    exit 1
fi

if ! grep -q "^\#\# \[Unreleased\]$" "$changelog_file" ; then
    echo "Error: $changelog_file doesn't contain Unreleased information. Please update the file with changes and run this script again."
    exit 1
fi

git checkout -b pre_release_${tag} main
if [ $? -ne 0 ]; then
    echo "Error: Cannot create release branch. Ensure you have sufficient permissions to repo and try again."
    exit 1
fi

# update CHANGELOG.md
date=$(date '+%Y-%m-%d')
sed  -i "/\#\# \[Unreleased\]/a\\ \n\#\# \[${tag}\] ${date}"  $changelog_file
if [ $? -ne 0 ]; then
    echo "Error: Cannot update CHANGELOG.md file. Update it manually, create the ${tag} and push changes to upstream"
    exit 1
fi

git add CHANGELOG.md
git commit -m "Prepare for releasing ${tag}"

echo "Now validate the changes using git diff main, create the ${tag} and push changes to upstream"
echo
