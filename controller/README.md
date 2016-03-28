Network Controller Implementation
======

This folder contains code implementing the node/NetworkController.hpp interface to allow ZeroTier nodes to create and manage virtual networks.

### Building

By default this code is not built or included in the client. To build on Linux, BSD, or Mac add `ZT_ENABLE_NETWORK_CONTROLLER=1` to the make command line. You'll need the development headers for Sqlite3 installed. They ship as part of OSX and Xcode. On Linux or BSD you'll probably need to install a package.

### Running

When started, a controller-enabled build of ZeroTier One will automatically create and initialize a *controller.db* in its home folder. This is where all the controller's data and persistent state lives.

Since Sqlite3 supports multiple processes attached to the same database, it is safe to back up a running database with the command line *sqlite3* utility:

    sqlite3 /path/to/controller.db .dump

In production ZeroTier runs this frequently and keeps many timestamped copies going back about a week. These are also backed up (encrypted) to Amazon S3 along with the rest of our data.

### Administrating

See service/README.md for documentation on the JSON API presented by this network controller implementation. Also see *nodejs-zt1-client* for a NodeJS JavaScript interface.

### Reliability

Network controllers can go offline without affecting already-configured members of running networks. You just won't be able to change anything and new members will not be able to join.

High-availability can be implemented through fail-over. A simple method involves making a frequent backup of the SQLite database (use the SQLite command line client to do this safely) and the network configuration master's working directory. Then, if the master goes down, another instance of it can rapidly be provisioned elsewhere. Since ZeroTier addresses are mobile, the new instance will quickly (usually no more than 30s) take over for the old one and service requests.

### Limits

A single network configuration master can administrate up to 2^24 (~16m) networks as per the ZeroTier protocol limit. There is no hard limit on the number of clients, though millions or more would impose significant CPU demands on a server. Optimizations could be implemented such as memoization/caching to reduce this.
