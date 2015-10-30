#!/bin/bash

# Edit as needed -- note that >1000 per host is likely problematic due to Linux kernel limits
NUM_CONTAINERS=100
CONTAINER_IMAGE=zerotier/http-test

#
# This script is designed to be run on Docker hosts to run NUM_CONTAINERS
#
# It can then be run on each Docker host via pssh or similar to run very
# large scale tests.
#

export PATH=/bin:/usr/bin:/usr/local/bin:/usr/sbin:/sbin

# Kill and clean up old test containers if any -- note that this kills all containers on the system!
docker ps -q | xargs -n 1 docker kill
docker ps -aq | xargs -n 1 docker rm

# Pull latest if needed -- change this to your image name and/or where to pull it from
docker pull $CONTAINER_IMAGE

# Run NUM_CONTAINERS
for ((n=0;n<$NUM_CONTAINERS;n++)); do
	docker run --device=/dev/net/tun --privileged -d $CONTAINER_IMAGE
done

exit 0
