ZeroTier Release Notes
======

*As of 1.2.0 this will serve as a detailed changelog, which we've needed for a long time.*

# 2017-03-13 -- Version 1.2.0

Version 1.2.0 is a major milestone release and introduces a large number of new capabilities to the ZeroTier core network hypervisor. It also includes some security tightening, major UI improvements for Windows and Macintosh platforms, and a number of bug fixes and platform issue workarounds.

## Features in 1.2.0

### The ZeroTier Rules Engine

The largest new feature in 1.2.0, and the product of many months of work, is our advanced network rules engine. With this release we achieve traffic control, security monitoring, and micro-segmentation capability on par with many enterprise SDN solutions designed for use in advanced data centers and corporate networks.

Rules allow you to filter packets on your network and vector traffic to security observers (e.g. a node running Snort). Security observation can be performed in-band using REDIRECT or out of band using TEE, and for tha latter it can be done for headers only, for select traffic, or probabilistically to reduce overhead on large distributed networks.

Tags and capabilites provide advanced methods for implementing fine grained permission structures and micro-segmentation schemes without bloating the size and complexity of your rules table.

See our manual for more information.

### Root Server Federation

It's now possible to create your own root servers and add them to the root server pool on your nodes. This is done by creating what's called a "moon," which is a signed enumeration of root servers and their stable points on the network. Refer to the manual for more details on how to do this and how it works.

Federated roots achieve a number of things:

 * You can deploy your own infrastructure to reduce dependency on ours.
 * You can deploy them *inside your LAN* to ensure that network connectivity inside your facility still works if the Internet goes down. This is the first step toward making ZeroTier viable as an in-house SDN solution.
 * Roots can be deployed inside national boundaries for countries with data residency laws or "great firewalls." (As of 1.2.0 there is still no way to force all traffic to use these roots, but that will be easy to do in a later version.)
 * Last but not least this makes ZeroTier somewhat less centralized by eliminating any hard dependency on ZeroTier, Inc.'s infrastructure.

Our roots will of course remain and continue to provide zero-configuration instant-on deployment, a secure global authority for identities, and free traffic relaying for those who can't establish peer to peer connections.

### Local Configuration

An element of our design philosophy is "features are bugs." This isn't an absolute dogma but more of a guiding principle. We try as hard as we can to avoid adding features, especially "knobs" that must be tweaked by a user.

As of 1.2.0 we've decided that certain knobs are unavoidable, and so there is now a `local.conf` file that can be used to configure them. See the ZeroTier One documentation for these. They include:

 * Blacklisting interfaces you want to make sure ZeroTier doesn't use for network traffic, such as VPNs, slow links, or backplanes designated for only certain kinds of traffic.
 * Turning uPnP/NAT-PMP on or off.
 * Configuring software updates on Windows and Mac platforms.
 * Defining trusted paths (the old trusted paths file is now deprecated)
 * Setting the ZeroTier main port so it doesn't have to be changed on the command line, which is very inconvenient in many cases.

### Improved In-Band Software Updates

A good software update system for Windows and Mac clients has been a missing feature in previous versions. It does exist but we've been shy about using it so far due to its fragility in some environments.

We've greatly improved this mechanism in 1.2.0. Not only does it now do a better job of actually invoking the update, but it also transfers updates in-band using the ZeroTier protocol. This means it can work in environments that do not allows http/https traffic or that force it through proxies. There's also now an update channel setting: `beta` or `release` (the default).

As before software updates are authenticated in two ways:

 1. ZeroTier's own signing key is used to sign all updates and this signature is checked prior to installation. Our signatures are done on an air-gapped machine.

 2. Updates for Mac and Windows are signed using Apple and Microsoft (DigiCert) keys and will not install unless these signatures are also valid.

Version 1.2.0's in-band mechanism effectively adds a third way: updates are fetched in-band from a designated ZeroTier node, thus authenticating the source using ZeroTier's built-in encryption and authentication mechanisms.

Updates are now configurable via `local.conf`. There are three options: `disable`, `download`, and `apply`. The third is the default for official builds on Windows and Mac, making updates happen silently and automatically as they do for popular browsers like Chrome and Firefox. For managed enterprise deployments IT people could ship a local.conf that disables updates and instead push updates via their management capabilities. Updates are disabled on Linux and other Unix-type platforms as these get updates through package repositories.

### Path Quality Monitoring (QoS and SD-WAN phase one)

Version 1.2.0 is now aware of the link quality of direct paths with other 1.2.0 nodes. This information isn't used yet but is visible through the JSON API. (Quality always shows as 100% with pre-1.2.0 nodes.)

Link quality monitoring is a precursor to intelligent multi-path and QoS support, which will in future versions bring us to feature parity with SD-WAN products like Cisco iWAN.

"Connect all the things!"

### Security Improvements

Version 1.2.0 adds anti-DOS (denial of service) rate limits and other hardening for improved resiliency against a number of denial of service attack scenarios.

It also adds a mechanism for instantaneous credential revocation. This can be used to revoke certificates of membership instantly to kick a node off a network (for private networks) and also to revoke capabilities and tags. The new controller sends revocations by default when a peer is de-authorized.

Revocations propagate using a "rumor mill" peer to peer algorithm. This means that a controller need only successfully send a revocation to at least one member of a network with connections to other active members. At this point the revocation will flood through the network peer to peer very quickly. This helps make revocations more robust in the face of poor connectivity with the controller or attempts to incapacitate the controller with denial of service attacks, as well as making revocations faster on huge networks.

### Windows and Macintosh UI Improvements (ZeroTier One)

The Mac has a whole new UI built natively in Objective-C. It provides a pulldown similar in appearance and operation to the Mac WiFi task bar menu. The Windows UI has also been improved and now provides a task bar icon that can be right-clicked to manage networks. Both now expose managed route and IP permissions, allowing nodes to easily opt in to full tunnel operation if you have a router configured on your network.

## Major Bug Fixes in 1.2.0

 * **The Windows HyperV 100% CPU bug**
   * This long-running problem turns out to have been an issue with Windows itself, but one we were triggering by placing invalid data into the Windows registry. Microsoft is aware of the issue but we've also fixed the triggering problem on our side. ZeroTier should now co-exist quite well with HyperV and should now be able to be bridged with a HyperV virtual switch.
 * **Segmenation Faults on musl-libc based Linux Systems**
   * Alpine Linux and some embedded Linux systems that use musl libc (a minimal libc) experienced segmentation faults. These were due to a smaller default stack size. A work-around that sets the stack size for new threads has been added.
 * **Windows Firewall Blocks Local JSON API**
   * On some Windows systems the firewall likes to block 127.0.0.1:9993 for mysterious reasons. This is now fixed in the installer via the addition of another firewall exemption rule.
 * **UI Crash on Embedded Windows Due to Missing Fonts**
   * The MSI installer now ships fonts and will install them if they are not present, so this should be fixed.
