ZeroTier One
======

ZeroTier is a software defined networking layer for Earth.

It can be used for on-premise network virtualization, as a peer to peer VPN for mobile teams, for hybrid or multi-data-center cloud deployments, or just about anywhere else secure software defined virtual networking is useful.

ZeroTier One is our OS-level client service. It allows Mac, Linux, Windows, FreeBSD, and soon other types of clients to join ZeroTier virtual networks like conventional VPNs or VLANs. It can run on native systems, VMs, or containers (Docker, OpenVZ, etc.).

Visit [ZeroTier's site](https://www.zerotier.com/) for more information. You can also download professionally packaged binary installers/packages for a variety of supported OSes there if you don't want to build ZeroTier One from source.

### Building from Source

For Mac, Linux, and BSD, just type "make" (or "gmake" on BSD). You won't need much installed; here are the requirements for various platforms:

 * Mac: Xcode command line tools, and [Packages](http://s.sudre.free.fr/Software/Packages/about.html) if you want to build an OSX .pkg installer ("make mac-dist-pkg"). It should build on OSX 10.7 or newer.
 * Linux: gcc/g++ or clang/clang++ (Makefile will use clang by default if available.)
 * FreeBSD (and other BSD): C++ compiler (G++ usually) and GNU make (gmake).

Each supported platform has its own *make-XXX.mk* file that contains the actual make rules for the platform. The right .mk file is included by the main Makefile based on the GNU make *OSTYPE* variable. Take a look at the .mk file for your platform for other targets, debug build rules, etc.

Windows, of course, is special. We build for Windows with Microsoft Visual Studio 2012 on Windows 7. A solution file is located in the *windows* subfolder. Newer versions of Visual Studio (and Windows) may work but haven't been tested. Older versions almost certainly will not, since they lack things like *stdint.h* and certain STL features. MinGW or other ports of gcc/clang to Windows should also work but haven't been tested. Build steps for Windows are a bit more complicated. For the moment you are on your own there.

Mobile versions are in progress. They don't work yet, and in any case only the glue code will be included in this repository. The full mobile apps are in private repositories on our own git server.

### Supported Platforms

CPU architecture shouldn't matter unless it's smaller than 32-bit or something really bizarre like a "middle-endian" processor. We have reports of ZeroTier One running on arm32, arm64, and MIPS. It builds and runs out of the box on Raspberry Pi, BeagleBone, BananaPi, and other ARM-based developer/hobbyist boards.

ZeroTier is written in C and C++ (C++03 / ISO/IEC 14882:2003) and uses data structures and algorithms from the C++03 STL. We do not use any C++11 features (yet), since we want to support a few old and embedded platforms that don't have C++11 compilers. You *will* require a compiler and headers new enough to support 64-bit integers (long long) and the *stdint.h* header. The latter could also be faked by adding defines for things like *uint32\_t*, *int64\_t*, etc.

Typing "make selftest" will build a *zerotier-selftest* binary which unit tests various internals and reports on a few aspects of the build environment. It's a good idea to try this on novel platforms or architectures.

### Running

Running *zerotier-one* with -h will show help.

On Linux and BSD you can start the service with:

    sudo ./zerotier-one -d

A home folder for your system will automatically be created.

The service is controlled via the JSON API, which by default is available at 127.0.0.1 port 9993. We include a *zerotier-cli* command line utility to make API calls for standard things like joining and leaving networks. The *authtoken.secret* file in the home folder contains the secret token for accessing this API. See README.md in [service/](service/) for API documentation.

Here's where home folders live (by default) on each OS:

 * Linux: /var/lib/zerotier-one
 * BSD: /var/db/zerotier-one
 * Mac: /Library/Application Support/ZeroTier/One
 * Windows: \\ProgramData\\ZeroTier\\One (That's for Windows 7. The base 'shared app data' folder might be different on different Windows versions.)

Running ZeroTier One on a Mac is the same, but OSX requires a kernel extension. We ship a signed binary build of the ZeroTier tap device driver, which can be installed on Mac with:

    sudo make install-mac-tap

This will create the home folder for Mac, place *tap.kext* there, and set its modes correctly to enable ZeroTier One to manage it with *kextload* and *kextunload*.

We recommend using our binary packages on Windows, since there are several prerequisites such as a tap driver that must be installed on the system *and* in the home folder.

### Joining A Network

ZeroTier virtual networks are identified by 16-digit hexadecimal network IDs, while devices are identified by 10-digit addresses. To get your address run:

    sudo zerotier-cli status

(Use ./zerotier-cli if you're running it right from your build folder.)

You should see something like:

    200 info ########## ONLINE #.#.#

That 10-digit hex code is you. It's derived via a one-way proof of work function from your cryptographic public key. Your public key can be found in *identity.public* in ZeroTier's home folder, while *identity.secret* contains your full identity including the secret portion of the key pair.

(The identity files define your device's *identity*. Moving them to another system will move that identity. Be careful when cloning virtual machines that have identities stored on them. If two devices have the same identity, they'll "fight" over it and you won't know which device will receive network packets.)

If you want to do a quick test, you can join [Earth](https://www.zerotier.com/earth.html). It's a global public network that anyone can join. Type:

    sudo zerotier-cli join 8056c2e21c000001

Then:

    sudo zerotier-cli listnetworks

At first it'll be in *REQUESTING\_CONFIGURATION* state. In a few seconds to a minute you should see something like:

    200 listnetworks 8056c2e21c000001 earth.zerotier.net ##:##:##:##:##:## OK PUBLIC zt0 ##.##.##.##/##

Earth will assign you an IP address in the "unofficially available" globally unrouted 28.0.0.0/7 IP block so as to avoid conflicts with local networks. (Your networks can use any IP scheme, or can even leave IP addresses unmanaged.) Once you get an IP, you should be able to ping something:

    ping earth.zerotier.net

Go to [http://earth.zerotier.net/](http://earth.zerotier.net/) to see a short little welcome page that will tell you your IP and Ethernet MAC address.

Earth is a public place. If you don't want to stick around run:

    sudo zerotier-cli leave 8056c2e21c000001

The network (and associated interface) should be gone.

Networks are created and administrated by network controllers. Most users will want to use our hosted controllers. Visit [our web site](https://www.zerotier.com/) for more information. Later in this README there are brief instructions about building ZeroTier One with network controller support for those who want to try running their own.

Macintosh and Windows installers also install a GUI application.

### Installing

We don't have a "make install" rule quite yet. On Linux you can type:

    make installer

This will build a binary that, when run, will install ZeroTier One on most current Linux distributions. We also have RPM and DEB build files in *ext/installfiles/linux* that wrap this installer in packages for RedHat/CentOS and Debian/Ubuntu derived distributions. If *rpmbuild* is present on the system, the RPM will be built. If *dpkg-deb* is present, the DEB package will be built.

On Mac the best way is to install [Packages](http://s.sudre.free.fr/Software/Packages/about.html) and use:

    make mac-dist-pkg

This builds a .pkg file that can be installed.

In FreeBSD there is now an official .pkg in the FreeBSD repository. Type "pkg install zerotier". It can also be built and installed from source.

Linux/BSD and Mac installations have an *uninstall.sh* file in their ZeroTier home folder that cleanly removes ZeroTier One from the system. Run this with:

    sudo /path/to/ZeroTier/home/folder/uninstall.sh

Windows installers are insane. We build our .MSI installers with [Advanced Installer Enterprise](http://www.advancedinstaller.com). The Advanced Installer project file is in *ext/installfiles/windows*. To avoid lasting psychological trauma we recommend leaving Windows installers alone and using the pre-built Windows binaries on our web site.

### Using ZeroTier One in Docker Containers

To run the ZeroTier One service in a Docker container, run it with "--device=/dev/net/tun --cap-add=NET_ADMIN". This will allow ZeroTier One to open a "tap" virtual network port inside the container.

Alternately, you can use Ethernet bridging to bridge the *docker0* device on your system to a ZeroTier virtual network. This allows you to run ZeroTier One on the host and bridge the entire Docker network backplane to a virtual network or other hosts.

We're working on better "official" Docker support. In the meantime there is a [user-contributed project here](https://github.com/davide/docker-zerotier).

### Building with Network Controller Support

**Warning: as of beta version 1.0.3 the new network controller is not heavily tested. We recommend waiting for 1.0.4 to deploy this in production.**

Network controllers are nodes responsible for issuing configurations and certificates to members of ZeroTier virtual networks. Most users won't need to run their own, so this code is by default not included in the ZeroTier One binary.

You can build a network controller on Linux or Mac with:

    make ZT_ENABLE_NETWORK_CONTROLLER=1

This will build a version that contains the Sqlite-backed network controller and associated extensions to the JSON local service control API. You will need the development headers for sqlite3 installed. On Mac these ship as part of Xcode, while on Linux they'll be found in packages for the various distributions.

See the JSON API documentation in [service/](service/) for more information about how to control controllers.

### Troubleshooting

For most users, it just works.

If you are running a local system firewall, we recommend adding a rule permitting UDP port 9993 inbound and outbound. If you installed binaries for Windows this should be done automatically. Other platforms might require manual editing of local firewall rules depending on your configuration.

The Mac firewall can be found under "Security" in System Preferences. Linux has a variety of firewall configuration systems and tools. If you're using Ubuntu's *ufw*, you can do this:

    sudo ufw allow 9993/udp

On CentOS check */etc/sysconfig/iptables* for IPTables rules. For other distributions consult your distribution's documentation. You'll also have to check the UIs or documentation for commercial third party firewall applications like Little Snitch (Mac), McAfee Firewall Enterprise (Windows), etc. if you are running any of those. Some corporate environments might have centrally managed firewall software, so you might also have to contact IT.

ZeroTier One peers will automatically locate each other and communicate directly over a local wired LAN *if UDP port 9993 inbound is open*. If that port is filtered, they won't be able to see each others' LAN announcement packets. If you're experiencing poor performance between devices on the same physical network, check their firewall settings. Without LAN auto-location peers must attempt "loopback" NAT traversal, which sometimes fails and in any case requires that every packet traverse your external router twice.

Users behind certain types of firewalls and "symmetric" NAT devices may not able able to connect to external peers directly at all. ZeroTier has limited support for port prediction and will *attempt* to traverse symmetric NATs, but this doesn't always work. If P2P connectivity fails you'll be bouncing UDP packets off our relay servers resulting in slower performance. Some NAT router(s) have a configurable NAT mode, and setting this to "full cone" will eliminate this problem. If you do this you may also see a magical improvement for things like VoIP phones, Skype, BitTorrent, WebRTC, certain games, etc., since all of these use NAT traversal techniques similar to ours.

If you're interested, there's a [technical deep dive about NAT traversal on our blog](https://www.zerotier.com/blog/?p=226). A troubleshooting tool to help you diagnose NAT issues is planned for the future as are uPnP/IGD/NAT-PMP and IPv6 transport.

If a firewall between you and the Internet blocks ZeroTier's UDP traffic, you will fall back to last-resort TCP tunneling to rootservers over port 443 (https impersonation). This will work almost anywhere but is *very slow* compared to UDP or direct peer to peer connectivity.

### Contributing

There are three main branches: **edge**, **test**, and **master**. Other branches may be for specific features, tests, or use cases. In general **edge** is "bleeding" and may or may not work, while **test** should be relatively stable and **master** is the latest tagged release. Pull requests should generally be done against **test** or **edge**, since pull requests against **master** may be working against a branch that is somewhat out of date.

### License

The ZeroTier source code is open source and is licensed under the GNU GPL v3 (not LGPL). If you'd like to embed it in a closed-source commercial product or appliance, please e-mail [contact@zerotier.com](mailto:contact@zerotier.com) to discuss commercial licensing. Otherwise it can be used for free.
