Network Configuration Master
======

This folder contains code implementing the node/NetworkConfigMaster.hpp interface to allow ZeroTier nodes to create and manage virtual networks.

The standard implementation uses SQLite3 with the attached schema. A separate service (not included here yet) is used to administrate that database and configure networks.

### Building

By default this code is not built or included in the client. To build on Linux, BSD, or Mac add ZT_ENABLE_NETCONF_MASTER=1 to the make command line. It could be built on Windows as well, but you're on your own there. You'd have to build SQLite3 first, or get a pre-built copy somewhere.

### Running

When you run a node with netconf support, a SQLite3 database will be created in the ZeroTier One working directory. On Linux this is /var/lib/zerotier-one by default unless you run the service with a command line to specify something else.

This database can be attached to and modified while the service is running as per SQLite3's rather awesome sharing capabilities. For now you're on your own in that department too, but in the future we might ship some code for this.

### Reliability

Network configuration masters can go offline without affecting already-configured members of running networks. You just won't be able to add new members, de-authorize members, or otherwise change any network configuration while the master is offline.

High-availability can be implemented through fail-over. A simple method involves making a frequent backup of the SQLite database (use the SQLite command line client to do this safely) and the network configuration master's working directory. Then, if the master goes down, another instance of it can rapidly be provisioned elsewhere. Since ZeroTier addresses are mobile, the new instance will quickly take over for the old one and service requests.

### Limits

A single network configuration master can administrate up to 2^24 networks as per the ZeroTier protocol limit. The number of clients is theoretically unlimited, but in practice is limited by network bandwidth.

You should keep an eye on CPU utilization and stop adding networks/users to a network configuration master if it gets too high. The bottleneck here is not the SQLite database but the CPU overhead of signing certificates of membership. You'll hit limits there long before hitting any limit associated with SQLite.
