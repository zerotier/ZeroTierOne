Network Configuration Master
======

This folder contains code implementing the node/NetworkConfigMaster.hpp interface to allow ZeroTier nodes to create and manage virtual networks.

The standard implementation uses SQLite3 with the attached schema. A separate service (not included here yet) is used to administrate that database and configure networks.

### Building

By default this code is not built or included in the client. To build on Linux, BSD, or Mac add ZT_ENABLE_NETCONF_MASTER=1 to the make command line. It could be built on Windows as well, but you're on your own there. You'd have to build SQLite3 first, or get a pre-built copy somewhere.

### Running

To enable netconf functionality, place a properly initialized SQLite3 database called **netconf.db** into the ZeroTier working directory of the node you wish to serve network configurations and restart it. If that file is present it will be opened and the network configuration master function will be enabled. You will see this in the log file.

To initialize a database run:

    sqlite3 -init netconf-schema.sql netconf.db

Then type '.quit' to exit the SQLite3 command shell.

### Reliability

Network configuration masters can go offline without affecting already-configured members of running networks. You just won't be able to add new members, de-authorize members, or otherwise change any network configuration while the master is offline.

High-availability can be implemented through fail-over. A simple method involves making a frequent backup of the SQLite database (use the SQLite command line client to do this safely) and the network configuration master's working directory. Then, if the master goes down, another instance of it can rapidly be provisioned elsewhere. Since ZeroTier addresses are mobile, the new instance will quickly take over for the old one and service requests.

### Limits

A single network configuration master can administrate up to 2^24 networks as per the ZeroTier protocol limit. The number of clients is theoretically unlimited, but in practice is limited by network bandwidth.

You should keep an eye on CPU utilization and stop adding networks/users to a network configuration master if it gets too high. The bottleneck here is not the SQLite database but the CPU overhead of signing certificates of membership. You'll hit limits there long before hitting any limit associated with SQLite.
