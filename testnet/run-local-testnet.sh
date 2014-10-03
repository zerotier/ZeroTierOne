#!/bin/bash

export PATH=/bin:/usr/bin:/usr/local/bin

#if [ ! -x ../zerotier-one ]; then
#	echo "$0: ../zerotier-one not found; build first"
#	exit 1
#fi

if [ $# -lt 1 ]; then
	echo "Usage: $0 <number of regular nodes to create>"
	exit 1
fi

if [ ! -d ./local-testnet ]; then
	echo "$0: cannot find local-testnet/ base for nodes"
	exit 1
fi

cd local-testnet
supernodes=`echo sn????`
cd ..

create_nodes=$1
node_num=0

echo "Starting supernodes: $supernodes"
echo

for sn in $supernodes; do
	node_path=local-testnet/$sn
	node_port=2`echo $sn | cut -d n -f 2`

	echo zerotier-one -T../root-topology/local-testnet/root-topology -p$node_port -u $node_path
	../zerotier-one -T../root-topology/local-testnet/root-topology -p$node_port -u $node_path
done

echo
echo "Starting $create_nodes regular nodes..."
echo

while [ $node_num -lt $create_nodes ]; do
	node_path=local-testnet/`printf n%.4d $node_num`
	node_port=`printf 3%.4d $node_num`

	mkdir -p $node_path

	echo zerotier-one -T../root-topology/local-testnet/root-topology -p$node_port -u $node_path
	../zerotier-one -T../root-topology/local-testnet/root-topology -p$node_port -u $node_path

	node_num=`expr $node_num + 1`
done

echo
echo "Nodes are now running. Waiting for all nodes to exit."
echo

wait
