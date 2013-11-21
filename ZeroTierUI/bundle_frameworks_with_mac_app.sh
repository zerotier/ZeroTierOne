#!/bin/bash

qt_libs=/Applications/Qt5.1.1/5.1.1/clang_64/lib

if [ ! -d "ZeroTier One.app" ]; then
	echo "Build ZeroTier One.app first."
	exit 1
fi
if [ ! -d "$qt_libs" ]; then
	echo "Edit bundle_frameworks_with_mac_app.sh and set qt_libs correctly first."
	exit 1
fi

cd "ZeroTier One.app/Contents"

rm -rf Frameworks
mkdir Frameworks
cd Frameworks
mkdir QtGui.framework
cp -v $qt_libs/QtGui.framework/QtGui QtGui.framework
mkdir QtWidgets.framework
cp -v $qt_libs/QtWidgets.framework/QtWidgets QtWidgets.framework
mkdir QtCore.framework
cp -v $qt_libs/QtCore.framework/QtCore QtCore.framework
