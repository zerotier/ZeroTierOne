#!/bin/bash
set -euo pipefail
IFS=$'\n\t'

ZT_NAME="$1" ; shift
DISTRO="$1" ; shift
ZT_ISA="$1" ; shift
VERSION="$1" ; shift
BUILD_EVENT="$1" ; shift

source "$(dirname $0)/lib.sh"

if [ ${BUILD_EVENT} == "tag" ]; then
    CHANNEL="bytey-releases"
else
    CHANNEL="bytey-builds"
fi

function publish_rpm {
    mkdir -p /${CHANNEL}/${DISTRO}
    cp -a ${ZT_NAME} /${CHANNEL}/${DISTRO}
}

function publish_deb {
    mkdir -p /${CHANNEL}/${DISTRO}/pool/dists/${ZT_NAME}/main
    cp -a ${ZT_NAME}/* /${CHANNEL}/${DISTRO}/pool/dists/${ZT_NAME}/main
}

case ${PKGFMT} in
    "rpm")
        publish_rpm
        ;;
    "deb")
        publish_deb
esac
            
