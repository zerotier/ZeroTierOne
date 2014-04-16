ZeroTier One - Ethernet Virtualization
======

ZeroTier One is an ethernet virtualization engine. It creates virtual LANs of almost unlimited size that span physical network boundaries.

The underlying protocol is peer to peer with managed anchor points ("supernodes") for instant-on communication, assisted NAT traversal, and relaying for users who cannot make P2P connections. All unicast packets are encrypted end-to-end using private keys that only you control. Multicast and broadcast work as they would on a normal LAN, except that traffic is throttled and intelligently prioritized on large networks to stay within bandwidth limits while preserving essential functionality (multicast triage algorithm). Its behavior is not unlike some enterprise-grade intelligent ethernet switches designed for huge wired networks such as university LANs.

[Visit ZeroTier Networks on the web](https://www.zerotier.com/) for more information. Follow the [ZeroTier blog](https://www.zerotier.com/blog/) and the [GitHub project](https://github.com/zerotier/ZeroTierOne) to stay up to date. See the GitHub-hosted wiki (sidebar) for technical info and help for various platforms.

Auto-updating binary packages that install easily [can be found here.](https://www.zerotier.com/download.html) Packages for popular Linux distributions that neatly wrap the Linux installer/uninstaller are coming soon. If you want to build from source, clone this repository and see BUILDING.txt and RUNNING.txt. If you build manually you'll have to update manually.

Once you are up and running, you can [create an account on the network control panel](https://www.zerotier.com/networks.html) if you want to create a private network or you can join [Earth](https://www.zerotier.com/earth.html) by running (on Linux and Mac) `sudo ./zerotier-cli join 8056c2e21c000001`. (Earth is a public virtual network, meaning that it has no access control and allows anyone to join. Make sure your system is up to date and you have no unprotected network services listening.)

Note: If UDP traffic over port 9993 (at a minimum) is not permitted by your local or network firewall(s), ZeroTier One will fall back to TCP tunneling to supernodes over port 443 and will not be able to make direct NAT-t connections to other peers. This fallback mode slows things down considerably. If you're getting very poor performance check your firewall settings. You can see if fallback mode is active by checking for outbound TCP connections from the zerotier-one process to port 443 on five or six hosts in the zerotier.com domain. (See Defaults.cpp for a list of these hosts.)

Most users will use IPv4 and/or IPv6 over ZeroTier One, but since it virtualizes at the ethernet (layer 2) level it can technically host almost any protocol. Users have made classic multiplayer games work over IPX, for example. Services like DHCP and IPv6 NDP for link-local addresses can work automatically.

The service is free for public networks and for managed private networks up to ten users. The code is open source and is licensed under the GNU GPL v3 (not LGPL). If you'd like to embed it in a closed-source commercial product, please e-mail [contact@zerotier.com](mailto:contact@zerotier.com) to discuss licensing.
