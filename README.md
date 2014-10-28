ZeroTier One - Planetary Scale Software Defined Networking
======

ZeroTier One is an ethernet virtualization engine. It creates virtual switched LANs of almost unlimited size that span physical network boundaries. To the operating system these behave just like ordinary Ethernet ports. Everything just works, even as your computer moves around or your physical Internet link changes.

It's intended to replace conventional hub-and-spoke VPNs, provide provider-neutral secure private backplane networks to multi-data-center and hybrid cloud deployments, allow remote access to embedded devices, and much more.

ZeroTier uses a peer to peer network with managed anchor points ("supernodes"). These are operated by ZeroTier Networks, and are free to use. They provides instant "zero configuration" startup, NAT traversal assistance, relaying in cases where NAT traversal doesn't work, and a trusted root authority for looking up peer public keys (identities). The supernodes run the same open-source software as regular nodes, are geographically distributed across three continents, and have remained stable for over a year. [This blog post](http://adamierymenko.com/decentralization-i-want-to-believe/) discusses the design rationale in some detail.

ZeroTier One encrypts all traffic end-to-end with keys that only you control. Even when traffic is traversing our network we cannot decrypt it. It's not, however, a "strong privacy" tool since it would still be possible for us (or your ISP or anyone else in between) to observe "meta-data" about your traffic. If you need *that* level of anonymity, we recommend [Tor](https://www.torproject.org) or something similar. Onion routing is the only technology we're aware of that is capable of delivering that level of end-to-end privacy protection.

At the moment the ZeroTier wire protocol is only documented in the source (start with the comments in Packet.hpp), but more formal documentation is planned for the near future. Encapsulation overhead is comparable to IPSec or OpenVPN.

[Visit ZeroTier Networks on the web](https://www.zerotier.com/) for more information. Follow the [ZeroTier blog](https://www.zerotier.com/blog/), [Twitter feed](https://twitter.com/ZeroTier) and the main [GitHub project](https://github.com/zerotier/ZeroTierOne) to stay up to date. See the GitHub-hosted wiki (sidebar) for more technical info and help for various platforms. There's also a [support portal](https://www.zerotier.com/support/) for more general information.

### Getting Started

Auto-updating binary packages that install easily [can be found here](https://www.zerotier.com/download.html) for officially supported platforms. Packages for popular Linux distributions that neatly wrap the Linux installer/uninstaller are coming soon.

If you want to build from source, clone this repository and see BUILDING.txt and RUNNING.txt. If you build manually you'll have to update manually, and we recommend doing so often as development is progressing very rapidly. (Protocol is pretty stable but you might miss out on new features or experience performance degradation if you're too far behind.)

Once you are up and running, you have several options.

ZeroTier provides ZeroTier One for free, but to help finance development and operations we have a [user-friendly freemium control panel on the web](https://www.zerotier.com/admin.html). This control panel lets you quickly and easily create your own networks, and the operation of their controller node(s) is left to us. Public networks created on our site are free, and private networks are free for up to ten devices. After that there is a monthly charge.

You *only* need an account on our site if you want to use the control panel found there. Joining networks requires no account. To get authorized on a private network, just send your device's 10-digit ID to the network's administrator and they can add you.

Public networks, as the name implies, can be joined without getting authorization from anyone. All you need is their 16-digit network ID. A public network called [Earth](https://www.zerotier.com/earth.html) (8056c2e21c000001) exists for everyone, but be sure your device is adequately secured and up to date before joining.

Alternatively, you can run your own network configuration controller. This lets you run any network for free. To do this, start with the netconf-service/ subfolder of this project. You'll need to do a bit of system administration work and manually populate a Redis database, but it's not terribly hard if you're into that kind of thing.

More products and services will be forthcoming.

### Basic Troubleshooting

For about 95% of users, it just works.

If you are running a local system firewall, we recommend adding a rule permitting UDP port 9993 inbound and outbound. The binary Windows installer takes care of this for Windows' built-in firewall, but other platforms and third party firewall software may need this for optimal performance.

ZeroTier One peers will automatically locate each other and communicate directly over a local wired LAN *if UDP port 9993 inbound is open*. If that port is filtered, they won't be able to see each others' LAN announcement packets. If you're experiencing poor performance between devices on the same physical network, check their firewall settings. Without LAN auto-location peers must attempt "loopback" NAT traversal, which sometimes fails and in any case requires that every packet traverse your external router twice.

Users behind certain types of firewalls and "symmetric" NAT devices may not able able to connect to external peers directly at all. ZeroTier has limited support for port prediction and will *attempt* to traverse symmetric NATs, but this doesn't always work. If P2P connectivity fails you'll be bouncing UDP packets off our relay servers resulting in slower performance. Some NAT router(s) have a configurable NAT mode, and setting this to "full cone" will eliminate this problem. If you do this you may also see a magical improvement for things like VoIP phones, Skype, BitTorrent, WebRTC, certain games, etc., since all of these use NAT traversal techniques similar to ours.

If you're interested, there's a [technical deep dive about NAT traversal on our blog](https://www.zerotier.com/blog/?p=226). A troubleshooting tool to help you diagnose NAT issues is planned for the future as are uPnP/IGD/NAT-PMP and IPv6 transport.

If a firewall between you and the Internet blocks ZeroTier's UDP traffic, you will fall back to last-resort TCP tunneling to supernodes over port 443 (https impersonation). This will work almost anywhere but is *slow*. If performance seems just terrible, this is the first thing to check.

Some Windows users have reported a [problem with the TAP device driver](https://github.com/zerotier/ZeroTierOne/issues/107). From user reports this seems to occur on Windows systems that have not been updated through Windows Update. While we have a fix planned, we **really** recommend keeping your system up to date for security reasons. Frankly we're a bit torn on this one. If this is truly the cause, perhaps ZeroTier failing to work on unpatched Windows systems is a feature not a bug.

More help can be found in the [GitHub wiki](https://github.com/zerotier/ZeroTierOne/wiki) and the [customer support portal / knowledge base](https://www.zerotier.com/support/).

The ZeroTier source code is open source and is licensed under the GNU GPL v3 (not LGPL). If you'd like to embed it in a closed-source commercial product or appliance, please e-mail [contact@zerotier.com](mailto:contact@zerotier.com) to discuss licensing.
