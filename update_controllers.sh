#!/usr/bin/env bash

if [ -z "$1" ]
then
    echo "Must supply a docker tag"
    exit 1
fi

TAG=$1

CONTROLLERS=( 12ac4a1e71 159924d630 17d709436c 1c33c1ced0 1d71939404 1d71939404 565799d8f6 6ab565387a 8056c2e21c 8850338390 8bd5124fd6 93afae5963 9bee8941b5 9f77fc393e a09acf0233 a84ac5c10a abfd31bd47 af78bf9436 c7c8172af1 d5e5fb6537 e4da7455b2 e5cd7a9e1c ea9349aa9c )

for c in ${CONTROLLERS[@]}
do
    kubectl set image deployment controller-${c} ztcentral-controller=gcr.io/zerotier-central/ztcentral-controller:${TAG}
done
