#!/bin/bash

# Builds Zerotier-One and libraries required for Network Containers, then
# copies the binaries into the test directory.

cd ../../

make clean
make
cd netcon
make -f make-intercept.mk lib
rm *.o
rm liblwip.so
make -f make-liblwip.mk

cd docker-test

cp ../../zerotier-one zerotier-one
cp ../../zerotier-cli zerotier-cli

cp ../liblwip.so liblwip.so
cp ../libzerotierintercept.so.1.0 libzerotierintercept.so.1.0
cp ../zerotier-intercept zerotier-intercept

