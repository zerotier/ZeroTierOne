ZeroTier - A Planetary Ethernet Switch
======

ZeroTier is a software-based managed Ethernet switch for planet Earth.

It erases the LAN/WAN distinction and makes VPNs, tunnels, proxies, and other kludges arising from the inflexible nature of physical networks obsolete. Everything is encrypted end-to-end and traffic takes the most direct (peer to peer) path available.

This repository contains ZeroTier One, a service that provides ZeroTier network connectivity to devices running Windows, Mac, Linux, iOS, Android, and FreeBSD and makes joining virtual networks as easy as joining IRC or Slack channels. It also contains the OS-independent core ZeroTier protocol implementation in [node/](node/).

Visit [ZeroTier's site](https://www.zerotier.com/) for more information and [pre-built binary packages](https://www.zerotier.com/download.shtml). Apps for Android and iOS are available for free in the Google Play and Apple app stores.

### Getting Started

ZeroTier's basic operation is easy to understand. Devices have 10-digit *ZeroTier addresses* like `89e92ceee5` and networks have 16-digit network IDs like `8056c2e21c000001`. All it takes for a device to join a network is its 16-digit ID, and all it takes for a network to authorize a device is its 10-digit address. Everything else is automatic.

A "device" can be anything really: desktops, laptops, phones, servers, VMs/VPSes, containers, and even (soon) apps.

For testing we provide a public virtual network called *Earth* with network ID `8056c2e21c000001`. On Linux and Mac you can do this with:

    sudo zerotier-cli join 8056c2e21c000001

Now wait about 30 seconds and check your system with `ip addr list` or `ifconfig`. You'll see a new interface whose name starts with *zt* and it should quickly get an IPv4 and an IPv6 address. Once you see it get an IP, try pinging `earth.zerotier.net` at `29.209.112.93`. If you've joined Earth from more than one system, try pinging your other machine.

*(IPv4 addresses for Earth are assigned from the block 28.0.0.0/7, which is not a part of the public Internet but is non-standard for private networks. It's used to avoid IP conflicts during testing. Your networks can run any IP addressing scheme you want.)*

If you don't want to belong to a giant Ethernet party line anymore, just type:

    sudo zerotier-cli leave 8056c2e21c000001

The *zt* interface will disappear. You're no longer on the network.

To create networks of your own you'll need a network controller. You can use [our hosted controller at my.zerotier.com](https://my.zerotier.com) which is free for up to 100 devices on an unlimited number of networks, or you can build your own controller and run it through its local JSON API. See [README.md in controller/](controller/) for more information.

### Building from Source

For Mac, Linux, and BSD, just type "make" (or "gmake" on BSD). You won't need much installed; here are the requirements for various platforms:

 * **Mac**: Xcode command line tools. It should build on OSX 10.7 or newer.
 * **Linux**: gcc/g++ (4.9 or newer recommended) or clang/clang++ (3.4 or newer recommended) Makefile will use clang by default if available. The Linux build will auto-detect the presence of development headers for *json-parser*, *http-parser*, *li8bnatpmp*, and *libminiupnpc* and will link against the system libraries for these if they are present and recent enough. Otherwise the bundled versions in [ext/](ext/) will be used. Type `make install` to install the binaries and other files on the system, though this will not create init.d or systemd links.
 * **FreeBSD**: C++ compiler (G++ usually) and GNU make (gmake).

Each supported platform has its own *make-XXX.mk* file that contains the actual make rules for the platform. The right .mk file is included by the main Makefile based on the GNU make *OSTYPE* variable. Take a look at the .mk file for your platform for other targets, debug build rules, etc.

Typing `make selftest` will build a *zerotier-selftest* binary which unit tests various internals and reports on a few aspects of the build environment. It's a good idea to try this on novel platforms or architectures.

Windows, of course, is special. We build for Windows with Microsoft Visual Studio 2012 on Windows 7. A solution file is located in the *windows/* subfolder. Newer versions of Visual Studio (and Windows) may work but haven't been tested. Older versions almost certainly will not, since they lack things like *stdint.h* and certain STL features. MinGW or other ports of gcc/clang to Windows should also work but haven't been tested.

32 and 64 bit X86 and ARM (e.g. Raspberry Pi, Android) are officially supported. Community members have built for MIPS and Sparc without issues.

### Running

Running *zerotier-one* with -h will show help.

On Linux and BSD you can start the service with:

    sudo ./zerotier-one -d

A home folder for your system will automatically be created.

The service is controlled via the JSON API, which by default is available at 127.0.0.1 port 9993. We include a *zerotier-cli* command line utility to make API calls for standard things like joining and leaving networks. The *authtoken.secret* file in the home folder contains the secret token for accessing this API. See README.md in [service/](service/) for API documentation.

Here's where home folders live (by default) on each OS:

 * **Linux**: `/var/lib/zerotier-one`
 * **FreeBSD**: `/var/db/zerotier-one`
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

If you're interested, there's a [technical deep dive about NAT traversal on our blog](https://www.zerotier.com/blog/?p=226). A troubleshooting tool to help you diagnose NAT issues is planned for the future as are uPnP/IGD/NAT-PMP and IPv6 transport.

If a firewall between you and the Internet blocks ZeroTier's UDP traffic, you will fall back to last-resort TCP tunneling to rootservers over port 443 (https impersonation). This will work almost anywhere but is *very slow* compared to UDP or direct peer to peer connectivity.

### Contributing

Please make pull requests against the `dev` branch. The `master` branch is release, and `edge` is for unstable and work in progress changes and is not likely to work.

### License

The ZeroTier source code is open source and is licensed under the GNU GPL v3 (not LGPL). If you'd like to embed it in a closed-source commercial product or appliance, please e-mail [contact@zerotier.com](mailto:contact@zerotier.com) to discuss commercial licensing. Otherwise it can be used for free.
