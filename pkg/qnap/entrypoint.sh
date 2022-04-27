#!/bin/bash

ZTO_VER=$(git describe --abbrev=0 --tags)
ZTO_COMMIT=$(git rev-parse HEAD)
ZTO_DESC=$(jq -r '.desc' ../config.json)

mkdir -p pkg/qnap/zerotier/arm_64
mkdir -p pkg/qnap/zerotier/arm_x09
mkdir -p pkg/qnap/zerotier/arm_x10
mkdir -p pkg/qnap/zerotier/arm_x12
mkdir -p pkg/qnap/zerotier/arm_x19
mkdir -p pkg/qnap/zerotier/arm_x31
mkdir -p pkg/qnap/zerotier/arm_x41
mkdir -p pkg/qnap/zerotier/x86_64
mkdir -p pkg/qnap/zerotier/x86
mkdir -p pkg/qnap/zerotier/x86_ce53xx

cat > pkg/qnap/zerotier/qpkg.cfg <<- EOM
# Update package config
# Name of the packaged application.
QPKG_NAME="zerotier"
# Name of the display application.
QPKG_DISPLAY_NAME="ZeroTier"
# Version of the packaged application.
QPKG_VER="$ZTO_VER"
# Author or maintainer of the package
QPKG_AUTHOR="ZeroTier, Inc."
# License for the packaged application
QPKG_LICENSE="BUSL-1.1"
# One-line description of the packaged application
QPKG_SUMMARY="$ZTO_DESC"

# Preferred number in start/stop sequence.
QPKG_RC_NUM="101"
# Init-script used to control the start and stop of the installed application.
QPKG_SERVICE_PROGRAM="zerotier.sh"

# Specifies any packages required for the current package to operate.
QPKG_REQUIRE="QVPN Service"
# Specifies what packages cannot be installed if the current package
# is to operate properly.
#QPKG_CONFLICT="Python, OPT/sed"
# Name of configuration file (multiple definitions are allowed).
#QPKG_CONFIG="myApp.conf"
#QPKG_CONFIG="/etc/config/myApp.conf"
# Port number used by service program.
QPKG_SERVICE_PORT="9993"

# Minimum QTS version requirement
QTS_MINI_VERSION="4.1.0"
# Maximum QTS version requirement
QTS_MAX_VERSION="5.0.0"

# Location of icons for the packaged application.
QDK_DATA_DIR_ICONS="icons"
EOM

# Copy binaries into pkg directory
# See: https://github.com/qnap-dev/QDK
cp -vf output/static/zerotier-one.${ZTO_VER}.alpine-aarch64 pkg/qnap/zerotier/arm_64/zerotier-one
#cp -vf output/static/zerotier-one.${ZTO_VER}.alpine-armv5tejl pkg/qnap/zerotier/arm-x09/zerotier-one
#cp -vf output/static/zerotier-one.${ZTO_VER}.alpine-armv5tel pkg/qnap/zerotier/arm-x19/zerotier-one
cp -vf output/static/zerotier-one.${ZTO_VER}.alpine-armhf pkg/qnap/zerotier/arm-x31/zerotier-one
cp -vf output/static/zerotier-one.${ZTO_VER}.alpine-armhf pkg/qnap/zerotier/arm-x41/zerotier-one
cp -vf output/static/zerotier-one.${ZTO_VER}.alpine-i386 pkg/qnap/zerotier/x86/zerotier-one
cp -vf output/static/zerotier-one.${ZTO_VER}.alpine-x86_64 pkg/qnap/zerotier/x86_64/zerotier-one
cp -vf output/static/zerotier-one.${ZTO_VER}.alpine-i386 pkg/qnap/zerotier/x86_ce53xx/zerotier-one

rm -rf output/qnap/*

pushd pkg/qnap/zerotier
./qbuild #--build-arch arm-x31
cp -f build/* ../../../output/qnap

# Show output product
cd popd
