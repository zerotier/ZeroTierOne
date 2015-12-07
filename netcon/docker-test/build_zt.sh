#!/bin/bash

# Builds Zerotier-One and libraries required for Network Containers, then
# copies the binaries into the test directory.

cd ../../

make clean
make netcon
make one

cd netcon/docker-test

cp ../../zerotier-one zerotier-one
cp ../../zerotier-cli zerotier-cli
cp ../../zerotier-cli zerotier-netcon-service

cp ../liblwip.so liblwip.so
cp ../libzerotierintercept.so.1.0 libzerotierintercept.so.1.0
cp ../zerotier-intercept zerotier-intercept

