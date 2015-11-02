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

pssh -h big-test-hosts -i -t 128 -p 256 "docker ps -q | xargs -r docker kill && docker ps -aq | xargs -r docker rm"

exit 0
