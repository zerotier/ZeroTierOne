#!/bin/bash

if [ "$#" -ne "2" ]; then
	echo 'Usage: controller-api-test.sh <network ID to create> <local TCP port for HTTP API>'
	exit 1
fi

network_json=$(cat <<EOF
{
	name: "test network",
	private: true,
	v4AssignMode: "zt",
	v6AssignMode: "none",
	multicastLimit: 100,
	ipAssignmentPools: [
		{
			network: "10.1.2.0",
			netmaskBits: 24
		}
	],
	rules: [
		{
			ruleId: 100,
			etherType: 0x0800,
			action: "accept"
		},
		{
			ruleId: 200,
			etherType: 0x0806,
			action: "accept"
		},
		{
			ruleId: 300,
			etherType: 0x86dd,
			action: "accept"
		}
	]
}
EOF
)

echo "$network_json" | curl -d - -v "http://127.0.0.1:$2/controller/network/$1"
