#!/usr/bin/env bash

if [ -z "$1" ]
then
    echo "Must supply a docker tag"
    exit 1
fi

TAG=$1

CONTROLLERS=`kubectl get pods -o=name | grep controller | sed "s/^.\{4\}//" | cut -d '-' -f 2`

for c in ${CONTROLLERS[@]}
do
    kubectl set image deployment controller-${c} ztcentral-controller=gcr.io/zerotier-central/ztcentral-controller:${TAG}
done
