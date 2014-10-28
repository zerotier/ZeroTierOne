Headless Test Network
======

The code in testnet.cpp (in base) and here in testnet/ is for running headless ZeroTier One test networks.

To build, type (from base) "make testnet". This will build the *zerotier-testnet* binary. Then run it with a directory to use for temporary node home directory storage as a parameter, e.g. "./zerotier-testnet /tmp/zttestnet".

Type **help** for help.

Right now the testnet simulates a perfect IP network and allows you to perform unicast and multicast tests. This is useful for verifying the basic correctness of everything under ideal conditions, and for smoke testing. In the future support for NAT emulation, packet loss, and other test features will be added to make this a more full-blown test suite.

When you start the testnet for the first time, no nodes will exist. You have to create some. First, create supernodes with **mksn**. Create as many as you want. Once you've created supernodes (you can only do this once per testnet) you can create regular nodes with **mkn**.

Once everything is created use **list** to check the status.

Each node will create a couple threads, so if your OS imposes a limit this might cause some of your virtual nodes to stick in *INITIALIZING* status as shown in the **list** command. If this happens you might want to blow away the contents of your temp directory and try again with fewer nodes.

Each node will get a home at the test path you specified, so quitting with **quit** and re-entering will reload the same test network.

Next you'll need to join your nodes to a virtual ZeroTier network. ZeroTier supports a built-in "fake" public network with the ID **ffffffffffffffff**. This network is for testing and is convenient to use here. It's also possible to set up the netconf-master within one of your test nodes, but doing so is beyond the scope of this doc (for now, but if your clever you can probably figure it out). Verify by doing **listnetworks**.

Now you can send some packets. Try:

    unicast * * ffffffffffffffff 24 60

That will do a unicast all-to-all test and report results. At first latencies might seem high, especially for a headless fake IP network. If you try it again you'll see them drop to zero or nearly so, since everyone will have executed a peer to peer connection.

    multicast <some node's 10-digit ZT address> * ffffffffffffffff 24 60

This will send a multicast packet to ff:ff:ff:ff:ff:ff (broadcast) and report back who receives it. You should see multicast propagation limited to 64 nodes, since this is the setting for multicast limit on the fake test network (and the default if not overridden in netconf). Multicast will show the same "warm up" behavior as unicast.

Typing just "." will execute the same testnet command again.

The first 10-digit field of each response is the ZeroTier node doing the sending or receiving. A prefix of "----------" is used for general responses to make everything line up neatly on the screen. We recommend using a wide terminal emulator.

Enjoy!
