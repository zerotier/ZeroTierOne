# ZeroTier One Network Configuration Service

## What is it?

It's the thing that controls virtual networks. It's a completely separate subsystem from supernodes; see the [Technical FAQ](https://github.com/zerotier/ZeroTierOne/wiki/Technical-FAQ) for more information on that.

ZeroTier's 16-digit / 64-bit network IDs are actually two numbers packed together into one. The most significant 40 bits / first 10 digits of a network ID are the ZeroTier address of the *network configuration master* responsible for issuing network configurations and certificates to members of the network. The least significant 24 bits / last 6 digits are an arbitrary 24-bit number used to identify this network on its given master.

When a ZeroTier node joins a network or updates its network configuration, it queries the network configuration master and receives a response containing either an error or a dictionary with that node's membership information and (if it's a private network) membership certificate.

Network configuration masters can go offline without affecting communication on the network, since they're only needed when it's necessary to issue new configurations. (Certificates are a more involved topic that's beyond the scope of this document, but suffice to say that the same applies there.) They can also be made fault tolerant by mirroring their identities and databases to a backup server that can take over if the main server dies.

Networks managed through the *zerotier.com* site are managed by network configuration masters run by ZeroTier networks, but if you're willing to do a bit of manual system administration you can set up and run your own.

## Installation

The first step is to choose a node to act as a netconf master. The netconf master subprocess is only supported on Unix-like platforms and has only been tested so far on Linux, but will probably work on Mac as well. But Windows builds don't support service subprocesses so the master cannot be a Windows node.

At the moment, netconf masters *cannot join their own networks*. We recommend using a node that you don't intend to make a member of the networks it administrates. Or, as an alternative, you can run ZeroTier One on a different port and with a different home folder. This is what we do, and on our masters we run it separately on startup with a script that executes "sudo /var/lib/zerotier-one/zerotier-one -p9994 /var/lib/zerotier-one-netconf-master &". This runs the service on UDP port 9994 and uses "/var/lib/zerotier-one-netconf-master" as its home folder. *Two or more instances of ZeroTier One can co-exist on the same machine as long as only one of them tries to actually join networks.*

Before setting up, three prerequisites must be installed: [Node](http://nodejs.org), npm (node's package manager), and Redis (a NoSQL database). On most Linux distributions there are packages for all these, so installing them should be easy. On CentOS/RedHat based distributions they are in the [EPEL](https://fedoraproject.org/wiki/EPEL) repository, so after installing or enabling EPEL you can just type "sudo yum install nodejs npm redis". Then start the Redis service with "sudo service redis start" and enable it on boot with "sudo chkconfig redis on". (Commands may differ on other Linux distributions.) Verify that it's running by typing "redis-cli" and testing whether it connects.

Once prerequisites are installed, follow these instructions:

1) Go to the ZeroTier home (for the node you plan to designate as master) and create a subfolder called "services.d".

2) Copy or symlink "netconf-master" from this repository into "services.d".

3) In the "services.d" folder, create a symlink called "netconf.service" that points to "netconf-master/netconf.service". Also check to make sure that "netconf-master/netconf.service" is executable. This is what the ZeroTier One service will execute to launch the subprocess.

4) In "services.d/netconf-master" type "npm install" to install NodeJS package dependencies.

5) Edit the initdb.js and change the network ID and other settings of the network you want to create. Remember how network IDs are made; the *first 10 digits* of your network's ID *must be the ZeroTier address of your netconf master node*. If it doesn't have one yet, try starting the ZeroTier service briefly and then killing it and it will generate *identity.public* and *identity.secret* automatically. The address is the first hex field in either of these files.

6) Run "node initdb.js" to initialize your Redis database. (If you want to use a Redis database other than 0, such as to avoid polluting another Redis database on the same machine, edit "config.js" and change the Redis database index there before initializing the database or running the service.)

7) Start or restart the ZeroTier One service. Check its "node.log" and your system process list to ensure that it successfully started the netconf server slave process.

## Using Your Own Networks

Once the netconf master service is running, you can test it by simply joining the network ID of the network you created from any ZeroTier One node anywhere in the world. If everything is set up correctly, it should just work. Your netconf service is controlling this network.

## Private Networks and Certificates

If you set the "private" flag to "1" and designated your network as private, members must be authorized. Each time a member attempts to join a network, a member record is created in Redis. These will have Redis names like "zt1:network:################:member:##########:~". Open this hash in any Redis database editor (or use hgetall and hset in redis-cli) and set "authorized" to "1" for whichever members you wish to authorize.

## What About the Web UI?

At the moment, the web-based administration interface used on zerotier.com is the only part of the system that is not open source. Right now it's all tied in with our payment processor, and our model right now is to charge for the convenience of using it.

We *might* open source it in the future, but in the meantime anyone with a bit of developer expertise should be able to write a few scripts to list networks, change network parameters, authorize members on private networks, etc. The Redis database schema is documented in *redis-schema.md*, and only the stuff under "zt1:network:..." is actually used by the netconf master service. The stuff under "zt1:user:..." is used by our web UI and doesn't need to be present for the netconf service to operate.
