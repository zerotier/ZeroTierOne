Network Controller Implementation
======

This folder contains code implementing the node/NetworkController.hpp interface to allow ZeroTier nodes to create and manage virtual networks.

The standard implementation uses SQLite3 with the attached schema. A separate service (not included here yet) is used to administrate that database and configure networks.

### Building

By default this code is not built or included in the client. To build on Linux, BSD, or Mac add ZT_ENABLE_NETCONF_MASTER=1 to the make command line. It could be built on Windows as well, but you're on your own there. You'd have to build SQLite3 first, or get a pre-built copy somewhere.

### Running

To enable netconf functionality, place a properly initialized SQLite3 database called **netconf.db** into the ZeroTier working directory of the node you wish to serve network configurations and restart it. If that file is present it will be opened and the network configuration master function will be enabled. You will see this in the log file.

To initialize a database run:

    sqlite3 -init netconf-schema.sql netconf.db

Then type '.quit' to exit the SQLite3 command shell.

Since SQLite3 supports multiple concurrent processes attached to the same database, it's easy to have another process administrate network details while the ZeroTier One service serves them. The schema is simple. Folks with some sysadmin expertise should be able to figure out how to populate a database and get something running. We'll probably publish some code for this at some point in the future, but for now it's all tied up with our zerotier.com web backend.

One important detail you'll need to know:

Whenever a network (including associated tables) is changed in any way, its revision number must be incremented. For private networks this is part of the certificate. Certificates are permitted to differ by up to 16 revisions. Therefore, to explicitly and rapidly de-authorize someone you should do a *two-step increment*. This is done with a time delay. First de-authorize the user and increment the revision by one. Then wait 30-60 seconds and increment it by 15. This gives all running clients a chance to get updated certificates before the now-excluded node falls off the revision number horizon. All other changes need only increment once, since a few nodes briefly having a slightly out of date config won't cause any harm.

### Reliability

Network configuration masters can go offline without affecting already-configured members of running networks. You just won't be able to add new members, de-authorize members, or otherwise change any network configuration while the master is offline.

High-availability can be implemented through fail-over. A simple method involves making a frequent backup of the SQLite database (use the SQLite command line client to do this safely) and the network configuration master's working directory. Then, if the master goes down, another instance of it can rapidly be provisioned elsewhere. Since ZeroTier addresses are mobile, the new instance will quickly take over for the old one and service requests.

### Limits

A single network configuration master can administrate up to 2^24 networks as per the ZeroTier protocol limit. The number of clients is theoretically unlimited, but in practice is limited by network bandwidth.

You should keep an eye on CPU utilization and stop adding networks/users to a network configuration master if it gets too high. The bottleneck here is not the SQLite database but the CPU overhead of signing certificates of membership. You'll hit limits there long before hitting any limit associated with SQLite.
