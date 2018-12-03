#!/usr/bin/env bash

CONTROLLERS=`kubectl get pods -o=name | grep controller | sed "s/^.\{4\}//"`

for c in ${CONTROLLERS[@]}
do
    kubectl delete pod ${c}
done
