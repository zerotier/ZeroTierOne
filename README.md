ZeroTier One
======

ZeroTier One creates flat virtual Ethernet networks of almost unlimited size. [Visit ZeroTier on the web](https://www.zerotier.com/) for more information.

This code is presently in **ALPHA** testing. That means that the protocol spec may change in incompatible ways, and it certainly has bugs. Testers should "git pull," rebuild, and restart fairly often. If things mysteriously stop working, do that.

See BUILDING.txt and RUNNING.txt for instructions. It currently builds on Mac and Linux. A Windows port is coming soon. Nice packages/installers and auto-update are also coming when alpha transitions to beta.

Note that this won't work if your firewall does not allow outbound UDP. It must allow UDP conversations on port 8993 at a minimum.

At present there is only one virtual LAN and you are dumped there by default. It's called Earth, and is exactly what it sounds like. The ability to create and join additional networks is coming soon. Once you're on, visit [earth.zerotier.net](http://earth.zerotier.net/) to see your Earth LAN IP address and other information.

**Security warning:** You read that right. ZeroTier One places your computer on an absolutely open global Ethernet party line. Please ensure that you are up to date on your OS patches and we recommend turning off unnecessary services. Also be sure that anything else you are sharing is password protected provided you don't want to share it: printers, iPhoto and iTunes shares, etc.

ZeroTier One is licensed under the GNU General Public License version 3. You are free to use, modify, or redistribute it under the terms of that license. If you would like to embed ZeroTier One in a closed source product or create a closed source derivative product, contact ZeroTier Networks LLC.

Check out the [blog](http://blog.zerotier.com/) for announcements, in-depth articles, and related thoughts. There is also a [Google group](https://groups.google.com/forum/#!forum/zerotier-one-users) for questions and discussion.

----
**FAQ**

**Q:** What can I do with this?  
**A:** For starters, try opening iTunes if you have it installed. If others are also online and sharing their collections, you might see them. If you have any games that run over a LAN (except those that require IPX), try those. What else can you think of to do on a completely flat, open network? Games? Collaborative software development? Remote debugging? Transferring files using simple drive shares? Sharing your desktop printer to someone on another continent? Use your imagination.

**Q:** Why do I get an IP address in the 27.0.0.0 or 28.0.0.0 range? And why does a lookup claim these addresses belong to the U.S. Department of Defense?  
**A:** Short answer: because IPv4 needs to die. Long answer: the Earth network assigns IPv4 IPs from these ranges. They do in fact belong to the DOD, but they are *not* routed to the open Internet. The DOD owns them but uses them internally for private networks. As a result, there is nothing *technically* wrong with "bogarting" these for our own private network. It's considered bad practice, but if you want a private address space in IPv4 that is unlikely to overlap other private address spaces (like 10/8 and 192.168/16), it's the only way. [Cellular carriers](http://www.androidcentral.com/sprint-internet-dept-defense-and-you)  and [cable companies](http://www.dslreports.com/forum/r25679029-Why-is-my-first-hop-to-a-DoD-assigned-IP-address-) frequently do the same thing.

**Q:** Is IPv6 supported?  
**A:** Yes. IPv6 link-local addresses (those in the fe80::/10 block) are auto-assigned and should work fine. No other IPv6 addresses are assigned *yet*, but there are plans to do interesting things in this area in the future.

**Q:** I don't want a giant Ethernet party line. Can I leave it and create private LANs instead?  
**A:** Yes, soon. A GUI to configure such things is in development. But for now there's only Earth.

**Q:** Are you going to charge for this?  
**A:** Public virtual LANs will remain free. We intend to charge for private networks in some way, but the exact model is TBD. Other cloud-supported paid features are also TBD.

**Q:** What's a supernode?  
**A:** Supernodes are nodes run by ZeroTier Networks that orindary users use to find one another and communicate until/unless they can perform NAT traversal and connect directly. They run the exact same software as everyone else. The only thing that really makes a supernode special is that it's designated as such.

**Q:** Can I run a supernode?  
**A:** No, not at the moment, and there would be no benefit to doing so.

**Q:** Will my local firewall rules apply to ZeroTier One traffic?  
**A:** ZeroTier creates a virtual Ethernet tap device (zt# on Mac and Linux) that emulates a wired Ethernet port. If your firewall applies to *all* network ports, it will filter traffic through this port as well. If it applies only to the primary interface, it may not. See your OS's firewall documentation, as different OSes and flavors thereof have slightly different configurations in this regard.

**Q:** Can you see my traffic? What about other users? Can you sniff the LAN?  
**A:** No. All unicast (direct computer to computer) traffic is encrypted end-to-end (even if it's being relayed), and the ZeroTier virtual LAN behaves like a LAN with a secure enterprise-grade switch that does not allow unicast conversations to be sniffed. Multicast and broadcast traffic will of course be seen by many recipients, but that's the idea.

**Q:** You say "almost unlimited size." Isn't multicast and broadcast traffic eventually going to be too much? What happens then?  
**A:** ZeroTier One uses an algorithm called *implicit social switching*. The overall maximum number of recipients for a multicast is limited, so if there are too many listeners to a given multicast address then obviously not everyone will receive every message. So who does? Social switching causes multicasts to propagate in the direction of peers to whom you have recently communicated. As a result, multicasts tend to propagate along lines of association. The people most likely to get your service announcements are those with whom you frequently connect.

**Q:** Is this designed to replace IP, BGP, IPv6, routers, etc.?  
**A:** No. Its purpose is to act as a collaboration tool, a VPN alternative, a network mobility tool, a testbed for the development of software that takes advantage of fully open networking, a virtual LAN party for gamers, and so on, but it's not intended (or able) to replace the core of the Internet.

**Q:** Can I bridge this to a physical port and plug in an Xbox, PlayStation, etc.?  
**A:** Not currently, as foreign Ethernet frames are not forwarded. This may be possible in a future version.

----
**Status**

*What works:*
 * Network auto-configuration via JSON API with identity based authentication and encryption
 * End-to-end encryption with automatic key exchange
 * Ethernet multicast with implicit social propagation (though the algorithm will certainly be getting tweaked as we go)
   * IPv4 ARP works (see MulticastGroup.hpp for details)
   * IPv6 neighbor discovery protocol works, thus IPv6 link-local addressing on the ZeroTier device also works
   * mDNS (though if the LAN gets big you will not see everyone, just people nearby on your implicit social graph)
   * IP-level multicast ought to "just work," though it may take up to a minute or so for a new join to propagate
 * NAT traversal works pretty well, at least behind relatively sane unrestrictive firewalls/routers. You can see this by pinging another node and watching, after a few seconds, the latency drop dramatically since you're no longer relaying. If it doesn't your firewall might block such things. Additional NAT traversal strategies are coming.
 * Changes in your local network configuration are generally detected and will cause peers to be re-acquired.

*Known immediate issues:*
 * Multiple network support is in but there is no interface to configure it, hence it is useless. But in alpha it'll be nice to shove everyone onto "Earth" in order to stress test that little "almost unlimited size" boast.
 * There is no multiple-launch protection yet and multiple instances on the same system do not work well due to route conflicts. Take care that zerotier-one is not running before launching it again. The command "sudo killall zerotier-one" is helpful.
 * Sometimes ZeroTier One doesn't like to terminate when asked nicely. This is related to issues with the tap device closing down properly. If it hangs around after a TERM signal, send it a KILL (9) signal.
 * The locally bound UDP port isn't configurable yet. It's 8993 by default.
 * Known security issues:
   * There is no rate limit for multicast. Please be nice during alpha testing and don't flood.
   * There is no filtering. Whether or not to try to do port-level filtering is not yet decided, as technically the local OS firewall and OS service configuration should do this and ZeroTier is not about re-inventing wheels. But certain things *will*need to be filtered by ZT1 itself. These include certain ICMP and ICMPv6 messages: router advertisement, router solicitation, source redirect, etc. DHCP and BOOTP should probably also be filtered at a minimum.
   * ARP and IPv6 NDP want extra security measures to prevent poisoning and spoofing. The best mechanism is being researched.
 * If a supernode goes down, this takes quite a while to be detected and things will go dead. Fast recovery from relay failure is in the works.
 * If by a one in 2^40 chance two people generate the same ZeroTier address, this is currently not handled. It's unlikely and also hard (as in cryptographically hard) to do intentionally. See comments in Identity.cpp for details.

*What doesn't work and might not work for a while, if ever:*
 * Only ARP, IPv4, and IPv6 frames are allowed. The ability to permit other ethertypes on user-created networks may be added later to allow things like old games that use IPX or private networks to remote administer weird hardware.
 * Bridging of the ZeroTier device to other networks will not work, and may never. This is TBD.
 * VLAN tagged frames will not work and probably never will, as higher level network provisioning handles that concept.

----

(c)2012-2013 [ZeroTier Networks LLC](https://www.zerotier.com/)
