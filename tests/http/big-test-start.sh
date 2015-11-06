#!/bin/bash

# More than 500 container seems to result in a lot of sporadic failures, probably due to Linux kernel scaling issues with virtual network ports
# 250 with a 16GB RAM VM like Amazon m4.xlarge seems good
NUM_CONTAINERS=250
CONTAINER_IMAGE=zerotier/http-test

export PATH=/bin:/usr/bin:/usr/local/bin:/usr/sbin:/sbin

pssh -h big-test-hosts -x '-t -t' -i -OUserKnownHostsFile=/dev/null -OStrictHostKeyChecking=no -t 600 -p 256 "for ((n=0;n<$NUM_CONTAINERS;n++)); do sudo docker run --device=/dev/net/tun --privileged -d $CONTAINER_IMAGE; sleep 0.1; done"

exit 0
