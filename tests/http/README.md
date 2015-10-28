HTTP one-to-all test
======

This code can be deployed across a large number of VMs or containers to test and benchmark HTTP traffic within a virtual network at scale. The agent acts as a server and can query other agents, while the server collects agent data and tells agents about each other. It's designed to use RFC4193-based ZeroTier IPv6 addresses within the cluster, which allows the easy provisioning of a large cluster without IP conflicts.

