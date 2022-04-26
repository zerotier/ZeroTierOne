#!/bin/bash

ZTO_VER=$(git describe --abbrev=0 --tags)
ZTO_COMMIT=$(git rev-parse HEAD)
ZTO_DESC=$(jq -r '.desc' ../config.json)

# Clean up any pre-existing packages
find pkg/asustor -type f -name "*.apk" -exec rm -rvf {} \;
# Copy current license
cp ../../LICENSE.txt zerotier/control/license.txt
# Configure package data
tmp="config-tmp.json"
jq --arg a "$ZTO_VER" '.general.version = $a' pkg/asustor/zerotier/CONTROL/config.json > $tmp && mv $tmp pkg/asustor/zerotier/CONTROL/config.json
echo $ZTO_DESC > pkg/asustor/zerotier/CONTROL/description.txt

# Copy binaries into pkg directory
cp -vf output/static/zerotier-one.${ZTO_VER}.alpine-aarch64 pkg/asustor/zerotier/bin/zerotier-one.aarch64
cp -vf output/static/zerotier-one.${ZTO_VER}.alpine-i386 pkg/asustor/zerotier/bin/zerotier-one.i386
cp -vf output/static/zerotier-one.${ZTO_VER}.alpine-x86_64 pkg/asustor/zerotier/bin/zerotier-one.x86-64

# Package
python pkg/asustor/apkg-tools.py create pkg/asustor/zerotier
rm -rf output/asustor
mkdir -p output/asustor
mv ./*.apk output/asustor

# Show output product
cat pkg/asustor/zerotier/CONTROL/config.json
tree output/asustor
