#!/usr/bin/env bash

if [ -z "$1" ]
then
    echo "Usage: $0 <docker_tag> <k8s_namespace>"
    echo "    k8s_namesapce is set to default if not specified"
    exit 1
fi

if [ -z "$2" ]
then
    NAMESPACE=default
else
    NAMESPACE=$2
fi

TAG=$1

CONTROLLERS=`kubectl get pods -o=name --namespace=${NAMESPACE} | grep controller | sed "s/^.\{4\}//" | cut -d '-' -f 2`

for c in ${CONTROLLERS[@]}
do
    kubectl set image deployment --namespace=${NAMESPACE} controller-${c} ztcentral-controller=docker.zerotier.com/zerotier-central/ztcentral-controller:${TAG}
    echo "Sleeping for 1 minute"
    sleep 60
done
