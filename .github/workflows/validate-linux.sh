#!/bin/bash

# This test script joins Earth and pokes some stuff

TEST_NETWORK=8056c2e21c000001
RUN_LENGTH=30
TEST_FINISHED=false
ZTO_VER=$(git describe --tags $(git rev-list --tags --max-count=1))
ZTO_COMMIT=$(git rev-parse HEAD)
ZTO_COMMIT_SHORT=$(git rev-parse --short HEAD)
TEST_DIR_PREFIX="$ZTO_VER-$ZTO_COMMIT_SHORT-test-results"

TEST_OK=0
TEST_FAIL=1

echo "Performing test on: $ZTO_VER-$ZTO_COMMIT_SHORT"
TEST_FILEPATH_PREFIX="$TEST_DIR_PREFIX/$ZTO_COMMIT_SHORT"
mkdir $TEST_DIR_PREFIX

# How long we will wait for ZT to come online before considering it a failure
MAX_WAIT_SECS=30

################################################################################
# Multi-node connectivity and performance test                                 #
################################################################################

test() {

	echo -e "\nPerforming pre-flight checks"

	check_exit_on_invalid_identity

	echo -e "\nRunning test for $RUN_LENGTH seconds"

	NS1="ip netns exec ns1"
	NS2="ip netns exec ns2"

	ZT1="$NS1 ./zerotier-cli -p9996 -D$(pwd)/node1"
	# Specify custom port on one node to ensure that feature works
	ZT2="$NS2 ./zerotier-cli -p9997 -D$(pwd)/node2"

	echo -e "\nSetting up network namespaces..."
	echo "Setting up ns1"

	ip netns add ns1
	$NS1 ip link set dev lo up
	ip link add veth0 type veth peer name veth1
	ip link set veth1 netns ns1
	ip addr add 192.168.0.1/24 dev veth0
	ip link set dev veth0 up

	$NS1 ip addr add 192.168.0.2/24 dev veth1
	$NS1 ip link set dev veth1 up

	# Add default route
	$NS1 ip route add default via 192.168.0.1

	iptables -t nat -A POSTROUTING -s 192.168.0.0/255.255.255.0 \
		-o eth0 -j MASQUERADE
	iptables -A FORWARD -i eth0 -o veth0 -j ACCEPT
	iptables -A FORWARD -o eth0 -i veth0 -j ACCEPT

	echo "Setting up ns2"
	ip netns add ns2
	$NS2 ip link set dev lo up
	ip link add veth2 type veth peer name veth3
	ip link set veth3 netns ns2
	ip addr add 192.168.1.1/24 dev veth2
	ip link set dev veth2 up

	$NS2 ip addr add 192.168.1.2/24 dev veth3
	$NS2 ip link set dev veth3 up
	$NS2 ip route add default via 192.168.1.1

	iptables -t nat -A POSTROUTING -s 192.168.1.0/255.255.255.0 \
		-o eth0 -j MASQUERADE
	iptables -A FORWARD -i eth0 -o veth2 -j ACCEPT
	iptables -A FORWARD -o eth0 -i veth2 -j ACCEPT

	# Allow forwarding
	sysctl -w net.ipv4.ip_forward=1

	################################################################################
	# Memory Leak Check                                                            #
	################################################################################

	export FILENAME_MEMORY_LOG="$TEST_FILEPATH_PREFIX-memory.log"

	echo -e "\nStarting a ZeroTier instance in each namespace..."

	export time_test_start=$(date +%s)

	# Spam the CLI as ZeroTier is starting
	spam_cli 100

	echo "Starting memory leak check"
	$NS1 sudo valgrind --demangle=yes --exit-on-first-error=yes \
		--error-exitcode=1 \
		--xml=yes \
		--xml-file=$FILENAME_MEMORY_LOG \
		--leak-check=full \
		./zerotier-one node1 -p9996 -U >>node_1.log 2>&1 &

	# Second instance, not run in memory profiler
	# Don't set up internet access until _after_ zerotier is running
	# This has been a source of stuckness in the past.
	$NS2 ip addr del 192.168.1.2/24 dev veth3
	$NS2 sudo ./zerotier-one node2 -U -p9997 >>node_2.log 2>&1 &
	sleep 1;
	$NS2 ip addr add 192.168.1.2/24 dev veth3
	$NS2 ip route add default via 192.168.1.1

	echo -e "\nPing from host to namespaces"

	ping -c 3 192.168.0.1
	ping -c 3 192.168.1.1

	echo -e "\nPing from namespace to host"

	$NS1 ping -c 3 192.168.0.1
	$NS1 ping -c 3 192.168.0.1
	$NS2 ping -c 3 192.168.0.2
	$NS2 ping -c 3 192.168.0.2

	echo -e "\nPing from ns1 to ns2"

	$NS1 ping -c 3 192.168.0.1

	echo -e "\nPing from ns2 to ns1"

	$NS2 ping -c 3 192.168.0.1

	################################################################################
	# Online Check                                                                 #
	################################################################################

	echo "Waiting for ZeroTier to come online before attempting test..."
	node1_online=false
	node2_online=false
	both_instances_online=false
	time_zt_node1_start=$(date +%s)
	time_zt_node2_start=$(date +%s)

	for ((s = 0; s <= $MAX_WAIT_SECS; s++)); do
		node1_online="$($ZT1 -j info | jq '.online' 2>/dev/null)"
		node2_online="$($ZT2 -j info | jq '.online' 2>/dev/null)"
		echo "Checking for online status: try #$s, node1:$node1_online, node2:$node2_online"
		if [[ "$node2_online" == "true" && "$node1_online" == "true" ]]; then
			export both_instances_online=true
			export time_to_both_nodes_online=$(date +%s)
			break
		fi
		sleep 1
	done

	echo -e "\n\nContents of ZeroTier home paths:"

	ls -lga node1
	tree node1
	ls -lga node2
	tree node2

	echo -e "\n\nRunning ZeroTier processes:"
	echo -e "\nNode 1:\n"
	$NS1 ps aux | grep zerotier-one
	echo -e "\nNode 2:\n"
	$NS2 ps aux | grep zerotier-one

	echo -e "\n\nStatus of each instance:"

	echo -e "\n\nNode 1:\n"
	$ZT1 status
	echo -e "\n\nNode 2:\n"
	$ZT2 status

	if [[ "$both_instances_online" != "true" ]]; then
		exit_test_and_generate_report $TEST_FAIL "one or more nodes failed to come online"
	fi

	echo -e "\nJoining networks"

	$ZT1 join $TEST_NETWORK
	$ZT2 join $TEST_NETWORK

	sleep 10

	node1_ip4=$($ZT1 get $TEST_NETWORK ip4)
	node2_ip4=$($ZT2 get $TEST_NETWORK ip4)

	echo "node1_ip4=$node1_ip4"
	echo "node2_ip4=$node2_ip4"

	echo -e "\nPinging each node"

	PING12_FILENAME="$TEST_FILEPATH_PREFIX-ping-1-to-2.txt"
	PING21_FILENAME="$TEST_FILEPATH_PREFIX-ping-2-to-1.txt"

	$NS1 ping -c 16 $node2_ip4 >$PING12_FILENAME
	$NS2 ping -c 16 $node1_ip4 >$PING21_FILENAME

	ping_loss_percent_1_to_2=$(cat $PING12_FILENAME |
		grep "packet loss" | awk '{print $6}' | sed 's/%//')
	ping_loss_percent_2_to_1=$(cat $PING21_FILENAME |
		grep "packet loss" | awk '{print $6}' | sed 's/%//')

	# Normalize loss value
	export ping_loss_percent_1_to_2=$(echo "scale=2; $ping_loss_percent_1_to_2/100.0" | bc)
	export ping_loss_percent_2_to_1=$(echo "scale=2; $ping_loss_percent_2_to_1/100.0" | bc)

	################################################################################
	# CLI Check                                                                    #
	################################################################################

	echo "Testing basic CLI functionality..."

	spam_cli 10

	$ZT1 join $TEST_NETWORK

	$ZT1 -h
	$ZT1 -v
	$ZT1 status
	$ZT1 info
	$ZT1 listnetworks
	$ZT1 peers
	$ZT1 listpeers

	$ZT1 -j status
	$ZT1 -j info
	$ZT1 -j listnetworks
	$ZT1 -j peers
	$ZT1 -j listpeers

	$ZT1 dump

	$ZT1 get $TEST_NETWORK allowDNS
	$ZT1 get $TEST_NETWORK allowDefault
	$ZT1 get $TEST_NETWORK allowGlobal
	$ZT1 get $TEST_NETWORK allowManaged
	$ZT1 get $TEST_NETWORK bridge
	$ZT1 get $TEST_NETWORK broadcastEnabled
	$ZT1 get $TEST_NETWORK dhcp
	$ZT1 get $TEST_NETWORK id
	$ZT1 get $TEST_NETWORK mac
	$ZT1 get $TEST_NETWORK mtu
	$ZT1 get $TEST_NETWORK name
	$ZT1 get $TEST_NETWORK netconfRevision
	$ZT1 get $TEST_NETWORK nwid
	$ZT1 get $TEST_NETWORK portDeviceName
	$ZT1 get $TEST_NETWORK portError
	$ZT1 get $TEST_NETWORK status
	$ZT1 get $TEST_NETWORK type

	# Test an invalid command
	$ZT1 get $TEST_NETWORK derpderp

	# TODO: Validate JSON

	# Performance Test

	export FILENAME_PERF_JSON="$TEST_FILEPATH_PREFIX-iperf.json"

	echo -e "\nBeginning performance test:"

	echo -e "\nStarting server:"

	echo "$NS1 iperf3 -s &"
	sleep 1

	echo -e "\nStarting client:"
	sleep 1

	echo "$NS2 iperf3 --json -c $node1_ip4 > $FILENAME_PERF_JSON"

	cat $FILENAME_PERF_JSON

	# Let ZeroTier idle long enough for various timers

	echo -e "\nIdling ZeroTier for $RUN_LENGTH seconds..."
	sleep $RUN_LENGTH

	echo -e "\nLeaving networks"

	$ZT1 leave $TEST_NETWORK
	$ZT2 leave $TEST_NETWORK

	sleep 5

	exit_test_and_generate_report $TEST_OK "completed test"
}

################################################################################
# Generate report                                                              #
################################################################################

exit_test_and_generate_report() {

	echo -e "\nStopping memory check..."
	sudo pkill -15 -f valgrind
	sleep 10

	time_test_end=$(date +%s)

	echo "Exiting test with reason: $2 ($1)"

	# Collect ZeroTier dump files

	echo -e "\nCollecting ZeroTier dump files"

	node1_id=$($ZT1 -j status | jq -r .address)
	node2_id=$($ZT2 -j status | jq -r .address)

	$ZT1 dump
	mv zerotier_dump.txt "$TEST_FILEPATH_PREFIX-node-dump-$node1_id.txt"

	$ZT2 dump
	mv zerotier_dump.txt "$TEST_FILEPATH_PREFIX-node-dump-$node2_id.txt"

	# Copy ZeroTier stdout/stderr logs

	cp node_1.log "$TEST_FILEPATH_PREFIX-node-log-$node1_id.txt"
	cp node_2.log "$TEST_FILEPATH_PREFIX-node-log-$node2_id.txt"

	# Generate report

	cat $FILENAME_MEMORY_LOG

	DEFINITELY_LOST=$(xmlstarlet sel -t -v '/valgrindoutput/error/xwhat' \
		$FILENAME_MEMORY_LOG | grep "definitely" | awk '{print $1;}')
	POSSIBLY_LOST=$(xmlstarlet sel -t -v '/valgrindoutput/error/xwhat' \
		$FILENAME_MEMORY_LOG | grep "possibly" | awk '{print $1;}')

	# Generate coverage report artifact and summary

	FILENAME_COVERAGE_JSON="$TEST_FILEPATH_PREFIX-coverage.json"
	FILENAME_COVERAGE_HTML="$TEST_FILEPATH_PREFIX-coverage.html"

	echo -e "\nGenerating coverage test report..."

	gcovr -r . --exclude ext --json-summary $FILENAME_COVERAGE_JSON \
		--html >$FILENAME_COVERAGE_HTML

	cat $FILENAME_COVERAGE_JSON

	COVERAGE_LINE_COVERED=$(cat $FILENAME_COVERAGE_JSON | jq .line_covered)
	COVERAGE_LINE_TOTAL=$(cat $FILENAME_COVERAGE_JSON | jq .line_total)
	COVERAGE_LINE_PERCENT=$(cat $FILENAME_COVERAGE_JSON | jq .line_percent)

	COVERAGE_LINE_COVERED="${COVERAGE_LINE_COVERED:-0}"
	COVERAGE_LINE_TOTAL="${COVERAGE_LINE_TOTAL:-0}"
	COVERAGE_LINE_PERCENT="${COVERAGE_LINE_PERCENT:-0}"

	# Default values

	DEFINITELY_LOST="${DEFINITELY_LOST:-0}"
	POSSIBLY_LOST="${POSSIBLY_LOST:-0}"
	ping_loss_percent_1_to_2="${ping_loss_percent_1_to_2:-100.0}"
	ping_loss_percent_2_to_1="${ping_loss_percent_2_to_1:-100.0}"
	time_to_both_nodes_online="${time_to_both_nodes_online:--1}"

	# Summarize and emit json for trend reporting

	FILENAME_SUMMARY="$TEST_FILEPATH_PREFIX-summary.json"

	time_length_test=$((time_test_end - time_test_start))
	if [[ $time_to_both_nodes_online != -1 ]];
	then
		time_to_both_nodes_online=$((time_to_both_nodes_online - time_test_start))
	fi
	#time_length_zt_join=$((time_zt_join_end-time_zt_join_start))
	#time_length_zt_leave=$((time_zt_leave_end-time_zt_leave_start))
	#time_length_zt_can_still_ping=$((time_zt_can_still_ping-time_zt_leave_start))

	summary=$(
		cat <<EOF
{
  "version":"$ZTO_VER",
  "commit":"$ZTO_COMMIT",
  "arch_m":"$(uname -m)",
  "arch_a":"$(uname -a)",
  "binary_size":"$(stat -c %s zerotier-one)",
  "time_length_test":$time_length_test,
  "time_to_both_nodes_online":$time_to_both_nodes_online,
  "num_possible_bytes_lost": $POSSIBLY_LOST,
  "num_definite_bytes_lost": $DEFINITELY_LOST,
  "num_bad_formattings": $POSSIBLY_LOST,
  "coverage_lines_covered": $COVERAGE_LINE_COVERED,
  "coverage_lines_total": $COVERAGE_LINE_TOTAL,
  "coverage_lines_percent": $COVERAGE_LINE_PERCENT,
  "ping_loss_percent_1_to_2": $ping_loss_percent_1_to_2,
  "ping_loss_percent_2_to_1": $ping_loss_percent_2_to_1,
  "test_exit_code": $1,
  "test_exit_reason":"$2"
}
EOF
	)

	echo $summary >$FILENAME_SUMMARY
	cat $FILENAME_SUMMARY

	exit 0
}

################################################################################
# CLI Check                                                                    #
################################################################################

spam_cli() {
	echo "Spamming CLI..."
	# Rapidly spam the CLI with joins/leaves

	MAX_TRIES="${1:-10}"

	for ((s = 0; s <= MAX_TRIES; s++)); do
		$ZT1 status
		$ZT2 status
		sleep 0.1
	done

	SPAM_TRIES=128

	for ((s = 0; s <= SPAM_TRIES; s++)); do
		$ZT1 join $TEST_NETWORK
	done

	for ((s = 0; s <= SPAM_TRIES; s++)); do
		$ZT1 leave $TEST_NETWORK
	done

	for ((s = 0; s <= SPAM_TRIES; s++)); do
		$ZT1 leave $TEST_NETWORK
		$ZT1 join $TEST_NETWORK
	done
}

################################################################################
# Check for proper exit on load of invalid identity                            #
################################################################################

check_exit_on_invalid_identity() {
	echo "Checking ZeroTier exits on invalid identity..."
	mkdir -p $(pwd)/exit_test
	ZT1="sudo ./zerotier-one -p9999 $(pwd)/exit_test"
	echo "asdfasdfasdfasdf" > $(pwd)/exit_test/identity.secret
	echo "asdfasdfasdfasdf" > $(pwd)/exit_test/authtoken.secret

	echo "Launch ZeroTier with an invalid identity"
	$ZT1 &
	my_pid=$!

	echo "Waiting 5 seconds"
	sleep 5

	# check if process is running
	kill -0 $my_pid
	if [ $? -eq 0 ]; then
		exit_test_and_generate_report $TEST_FAIL "Exit test FAILED: Process still running after being fed an invalid identity"
	fi
}

test "$@"
