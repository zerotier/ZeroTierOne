#!/bin/bash

# Builds Zerotier-One and libraries required for Network Containers, then
# copies the binaries into the test directory.

cd ../../
make clean
make one
make netcon
cd netcon/docker-test

cp ../../zerotier-cli zerotier-cli
cp ../../zerotier-netcon-service zerotier-netcon-service
cp ../../libzerotierintercept.so libzerotierintercept.so

cp ../liblwip.so liblwip.so
cp ../zerotier-intercept zerotier-intercept

cp ../../zerotier-one zerotier-one

