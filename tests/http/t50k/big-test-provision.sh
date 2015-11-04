#!/bin/bash

export PATH=/home/api/bin:/bin:/usr/bin:/sbin:/usr/sbin:/usr/local/bin

# Provision the Big Test(tm) on Digital Ocean using tugboat CLI

REGIONS="nyc3 ams3 ams2 sfo1 sgp1 lon1 fra1 tor1"
PER_REGION=25
DROPLET_IMAGE=docker
DROPLET_SIZE=16gb
SSH_KEY=1543438

for r in $REGIONS; do
	for((n=0;n<$PER_REGION;n++)); do
		tugboat create TEST-${r}-${n}.zerotier.net --size=$DROPLET_SIZE --image=$DROPLET_IMAGE --region=$r --keys=$SSH_KEY
	done
done
