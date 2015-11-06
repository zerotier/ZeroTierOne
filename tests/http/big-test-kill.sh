#!/bin/bash

# Kills all running Docker containers on all big-test-hosts

export PATH=/bin:/usr/bin:/usr/local/bin:/usr/sbin:/sbin

pssh -h big-test-hosts -x '-t -t' -i -OUserKnownHostsFile=/dev/null -OStrictHostKeyChecking=no -t 0 -p 256 "sudo docker ps -aq | xargs -r sudo docker rm -f"

exit 0
