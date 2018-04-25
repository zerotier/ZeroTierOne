ZeroTier - A Planetary Ethernet Switch
======

ZeroTier is a smart programmable Ethernet switch for planet Earth.

It replaces the physical LAN/WAN boundary with a virtual one, allowing devices of any type at any location to be managed as if they all reside in the same cloud region or data center. All traffic is encrypted end-to-end and takes the most direct path available for minimum latency and maximum performance. The goals and design of ZeroTier are inspired by among other things the original [Google BeyondCorp](https://static.googleusercontent.com/media/research.google.com/en//pubs/archive/43231.pdf) paper and the [Jericho Forum](https://en.wikipedia.org/wiki/Jericho_Forum).

Visit [ZeroTier's site](https://www.zerotier.com/?pk_campaign=github_ZeroTierOne) for more information and [pre-built binary packages](https://www.zerotier.com/download.shtml?pk_campaign=github_ZeroTierOne). Apps for Android and iOS are available for free in the Google Play and Apple app stores.

### Getting Started

Everything in the ZeroTier world is controlled by two types of identifier: 40-bit/10-digit *ZeroTier addresses* and 64-bit/16-digit *network IDs*. A ZeroTier address identifies a node or "device" (laptop, phone, server, VM, app, etc.) while a network ID identifies a virtual Ethernet network that can be joined by devices.

Another way of thinking about it is that ZeroTier addresses are port numbers on a giant planetary-sized smart switch while network IDs are VLANs to which these ports can be assigned. For more details read about VL1 and VL2 in [the ZeroTier manual](https://www.zerotier.com/manual.shtml).

*Network controllers* are ZeroTier nodes that act as access control certificate authorities and configuration managers for virtual networks. The first 40 bits (or 10 digits) of a network ID is the ZeroTier address of its controller. You can create networks with our [hosted controllers](https://my.zerotier.com/) and web UI/API or [host your own](controller/) if you don't mind posting some JSON configuration info or writing a script to do so.

### Project Layout

 - `artwork/`: icons, logos, etc.
 - `attic/`: old stuff and experimental code that we want to keep around for reference.
 - `controller/`: the reference network controller implementation, which is built and included by default on desktop and server build targets.
 - `debian/`: files for building Debian packages on Linux.
 - `doc/`: manual pages and other documentation.
 - `ext/`: third party libraries, binaries that we ship for convenience on some platforms (Mac and Windows), and installation support files.
 - `include/`: include files for the ZeroTier core.
 - `java/`: a JNI wrapper used with our Android mobile app. (The whole Android app is not open source but may be made so in the future.)
 - `macui/`: a Macintosh menu-bar app for controlling ZeroTier One, written in Objective C.
 - `node/`: the ZeroTier virtual Ethernet switch core, which is designed to be entirely separate from the rest of the code and able to be built as a stand-alone OS-independent library. Note to developers: do not use C++11 features in here, since we want this to build on old embedded platforms that lack C++11 support. C++11 can be used elsewhere.
 - `osdep/`: code to support and integrate with OSes, including platform-specific stuff only built for certain targets.
 - `service/`: the ZeroTier One service, which wraps the ZeroTier core and provides VPN-like connectivity to virtual networks for desktops, laptops, servers, VMs, and containers.
 - `tcp-proxy/`: TCP proxy code run by ZeroTier, Inc. to provide TCP fallback (this will die soon!).
 - `windows/`: Visual Studio solution files, Windows service code for ZeroTier One, and the Windows task bar app UI.

The base path contains the ZeroTier One service main entry point (`one.cpp`), self test code, makefiles, etc.

### Build and Platform Notes

To build on Mac and Linux just type `make`. On FreeBSD and OpenBSD `gmake` (GNU make) is required and can be installed from packages or ports. For Windows there is a Visual Studio solution in `windows/'.

 - **Mac**
   - Xcode command line tools for OSX 10.7 or newer are required.
   - Tap device driver kext source is in `ext/tap-mac` and a signed pre-built binary can be found in `ext/bin/tap-mac`. You should not need to build it yourself. It's a fork of [tuntaposx](http://tuntaposx.sourceforge.net) with device names changed to `zt#`, support for a larger MTU, and tun functionality removed.
 - **Linux**
   - The minimum compiler versions required are GCC/G++ 4.9.3 or CLANG/CLANG++ 3.4.2.
   - Linux makefiles automatically detect and prefer clang/clang++ if present as it produces smaller and slightly faster binaries in most cases. You can override by supplying CC and CXX variables on the make command line.
   - CentOS 7 ships with a version of GCC/G++ that is too old, but a new enough version of CLANG can be found in the *epel* repositories. Type `yum install epel-release` and then `yum install clang` to build there.
 - **Windows**
   - Windows 7 or newer is supported. This *may* work on Vista but isn't officially supported there. It will not work on Windows XP.
   - We build with Visual Studio 2015. Older versions may not work. Clang or MinGW will also probably work but may require some makefile hacking.
   - Pre-built signed Windows drivers are included in `ext/bin/tap-windows-ndis6`. The MSI files found there will install them on 32-bit and 64-bit systems. We don't recommend trying to build Windows drivers from scratch unless you know what you're doing. One does not simply "build" a Windows driver.
 - **FreeBSD**
   - Tested most recently on FreeBSD-11. Older versions may work but we're not sure.
   - GCC/G++ 4.9 and gmake are required. These can be installed from packages or ports. Type `gmake` to build.
 - **OpenBSD**
   - There is a limit of four network memberships on OpenBSD as there are only four tap devices (`/dev/tap0` through `/dev/tap3`). We're not sure if this can be increased.
   - OpenBSD lacks `getifmaddrs` (or any equivalent method) to get interface multicast memberships. As a result multicast will only work on OpenBSD for ARP and NDP (IP/MAC lookup) and not for other purposes.
   - Only tested on OpenBSD 6.0. Older versions may not work.
   - GCC/G++ 4.9 and gmake are required and can be installed using `pkg_add` or from ports. They get installed in `/usr/local/bin` as `egcc` and `eg++` and our makefile is pre-configured to use them on OpenBSD.

Typing `make selftest` will build a *zerotier-selftest* binary which unit tests various internals and reports on a few aspects of the build environment. It's a good idea to try this on novel platforms or architectures.

### Running

Running *zerotier-one* with -h will show help.

On Linux and BSD you can start the service with:

    sudo ./zerotier-one -d

A home folder for your system will automatically be created.

The service is controlled via the JSON API, which by default is available at 127.0.0.1 port 9993. We include a *zerotier-cli* command line utility to make API calls for standard things like joining and leaving networks. The *authtoken.secret* file in the home folder contains the secret token for accessing this API. See README.md in [service/](service/) for API documentation.

Here's where home folders live (by default) on each OS:

 * **Linux**: `/var/lib/zerotier-one`
 * **FreeBSD** / **OpenBSD**: `/var/db/zerotier-one`
 * **Mac**: `/Library/Application Support/ZeroTier/One`
 * **Windows**: `\ProgramData\ZeroTier\One` (That's for Windows 7. The base 'shared app data' folder might be different on different Windows versions.)

Running ZeroTier One on a Mac is the same, but OSX requires a kernel extension. We ship a signed binary build of the ZeroTier tap device driver, which can be installed on Mac with:

    sudo make install-mac-tap

This will create the home folder for Mac, place *tap.kext* there, and set its modes correctly to enable ZeroTier One to manage it with *kextload* and *kextunload*.

### Troubleshooting

For most users, it just works.

If you are running a local system firewall, we recommend adding a rule permitting UDP port 9993 inbound and outbound. If you installed binaries for Windows this should be done automatically. Other platforms might require manual editing of local firewall rules depending on your configuration.

The Mac firewall can be found under "Security" in System Preferences. Linux has a variety of firewall configuration systems and tools. If you're using Ubuntu's *ufw*, you can do this:

    sudo ufw allow 9993/udp

On CentOS check `/etc/sysconfig/iptables` for IPTables rules. For other distributions consult your distribution's documentation. You'll also have to check the UIs or documentation for commercial third party firewall applications like Little Snitch (Mac), McAfee Firewall Enterprise (Windows), etc. if you are running any of those. Some corporate environments might have centrally managed firewall software, so you might also have to contact IT.

ZeroTier One peers will automatically locate each other and communicate directly over a local wired LAN *if UDP port 9993 inbound is open*. If that port is filtered, they won't be able to see each others' LAN announcement packets. If you're experiencing poor performance between devices on the same physical network, check their firewall settings. Without LAN auto-location peers must attempt "loopback" NAT traversal, which sometimes fails and in any case requires that every packet traverse your external router twice.

Users behind certain types of firewalls and "symmetric" NAT devices may not able able to connect to external peers directly at all. ZeroTier has limited support for port prediction and will *attempt* to traverse symmetric NATs, but this doesn't always work. If P2P connectivity fails you'll be bouncing UDP packets off our relay servers resulting in slower performance. Some NAT router(s) have a configurable NAT mode, and setting this to "full cone" will eliminate this problem. If you do this you may also see a magical improvement for things like VoIP phones, Skype, BitTorrent, WebRTC, certain games, etc., since all of these use NAT traversal techniques similar to ours.

If you're interested, there's a [technical deep dive about NAT traversal on our blog](https://www.zerotier.com/blog/?p=226?pk_campaign=github_ZeroTierOne). A troubleshooting tool to help you diagnose NAT issues is planned for the future as are uPnP/IGD/NAT-PMP and IPv6 transport.

If a firewall between you and the Internet blocks ZeroTier's UDP traffic, you will fall back to last-resort TCP tunneling to rootservers over port 443 (https impersonation). This will work almost anywhere but is *very slow* compared to UDP or direct peer to peer connectivity.

### Contributing

Please make pull requests against the `dev` branch. The `master` branch is release, and `edge` is for unstable and work in progress changes and is not likely to work.

### License

The ZeroTier source code is open source and is licensed under the GNU GPL v3 (not LGPL). If you'd like to embed it in a closed-source commercial product or appliance, please e-mail [contact@zerotier.com](mailto:contact@zerotier.com) to discuss commercial licensing. Otherwise it can be used for free.
