#!/bin/bash

# Runs test image and monitor image as daemons
test_name=${PWD##*/}
echo 'Starting containers for: ' "$test_name"
touch "$test_name".name
test_container=$(docker run -d -it -v $PWD/../../_results:/opt/results --device=/dev/net/tun "$test_name":latest)
monitor_container=$(docker run -d -it -v $PWD/../../_results:/opt/results --device=/dev/net/tun "$test_name"_monitor:latest)

echo "waiting $netcon_test_wait_time for test to complete."
sleep $netcon_test_wait_time
docker stop $(docker ps -a -q)
docker rm $test_container
docker rm $monitor_container

rm -f *.name