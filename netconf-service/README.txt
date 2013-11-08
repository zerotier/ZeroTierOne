This is the netconf service, which can be built and run by placing it in the
services.d subfolder of the ZeroTier One home directory.

Users probably won't be interested in this. It's for running a "netconf
master," which handles certificate issuing, static IP assignment, and other
things for a network. The ZeroTier address of the netconf master forms the
first 40 bits of a hexadecimal network ID, permitting the master to be
located and queried.

Masters currently don't support multi-homing, but they can easily be made
fault tolerant via fail-over. If the master node goes down, it can be
started elsewhere with the same ZT1 identity. (The underlying database would
also have to be fault tolerant.)

For this to work it requires a MySQL backend with a properly structured
database.
