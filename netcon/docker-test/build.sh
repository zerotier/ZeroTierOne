#!/bin/bash

echo "*** Building Zerotier-One, libraries, and test/monitor images..."

./build_zt.sh
./build_tests.sh $1

echo "*** Done"
