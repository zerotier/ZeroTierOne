Test Root Topology Script
======

This builds a test-root-topology from any number of running test-rootserver-# Docker containers. This can then be used with the (undocumented) -T (override root topology) option to run test networks under Docker.

Once you have a local Docker test network running you can use iptables rules to simulate a variety of network pathologies, or you can just use it to test any new changes to the protocol or node behavior at some limited scale.
