#!/bin/sh

ZTO_VER=$(git describe --abbrev=0 --tags)
ZTO_COMMIT=$(git rev-parse HEAD)
ZTO_DESC=$(jq -r '.desc' ../config.json)

generate_new_pkg_spec()
{
  cat > zerotier/apkg.rc <<- EOM
Package: zerotier
Section: Apps
Version: $ZTO_VER
Packager: ZeroTier, Inc.
Email: contact@zerotier.com
Homepage: http://www.zerotier.com
Description: $ZTO_DESC
AddonShowName: ZeroTier
Icon: zerotier.png
AddonIndexPage: index.html
AddonUsedPort: 9993
InstDepend:
InstConflict:
StartDepend:
StartConflict:
CenterType:1
UserControl:0
MinFWVer:
MaxFWVer:
IndividualFlag:

EOM
}

pkg_x64()
{
  MKSAPKG=../mksapkg-OS5
  chmod a+x $MKSAPKG
  BIN_SRC_DIR=..

  pushd zerotier

  cp -f $BIN_SRC_DIR/zerotier-one bin/zerotier-one
  $MKSAPKG -E -s -m WDMyCloudDL2100
  $MKSAPKG -E -s -m WDMyCloudDL4100
  $MKSAPKG -E -s -m MyCloudPR4100
  $MKSAPKG -E -s -m MyCloudPR2100

  popd
}

pkg_armhf()
{
  MKSAPKG=../mksapkg-OS5
  chmod a+x $MKSAPKG
  BIN_SRC_DIR=..

  pushd zerotier
  cp -f $BIN_SRC_DIR/zerotier-one bin/zerotier-one

  # MyCloudEX2Ultra (Armada A385)
  $MKSAPKG -E -s -m MyCloudEX2Ultra
  # WDMyCloudEX4100 (Armada A385)
  $MKSAPKG -E -s -m WDMyCloudEX4100
  # WDMyCloudEX2100 (Armada A385)
  $MKSAPKG -E -s -m WDMyCloudEX2100
  # WDMyCloudEX4 (Armada A300)
  $MKSAPKG -E -s -m WDMyCloudEX4
  # WDMyCloudEX2 (Armada A370)
  $MKSAPKG -E -s -m WDMyCloudEX2
  # WDMyCloudMirrorGen2 (Armada A385)
  $MKSAPKG -E -s -m WDMyCloudMirrorGen2
  # WDMyCloudMirror (Armada A370)
  $MKSAPKG -E -s -m WDMyCloudMirror
  # WDCloud (Armada A375 (2 cores, 1GHz each, armhf, ARMv7l, Cortex A9, vfp, neon))
  $MKSAPKG -E -s -m WDCloud

  popd
}

normalize-filenames()
{
  for f in *'('*')'*
  do
     mv -i "$f" "${f/(*)/}"
  done
}

clean()
{
  rm -rf *.bin zerotier/apkg.sign
}

generate_new_pkg_spec
pkg_x64
pkg_armhf
normalize-filenames

"$@"
