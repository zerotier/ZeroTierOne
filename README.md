ZeroTier One
======

ZeroTier One creates flat virtual Ethernet networks of almost unlimited size. [Visit ZeroTier on the web](https://www.zerotier.com/) for more information.

This code is presently in **ALPHA** testing. That means that the protocol spec may change in incompatible ways, and it certainly has bugs. Testers should "git pull," rebuild, and restart fairly often. If things mysteriously stop working, do that.

See BUILDING.txt and RUNNING.txt for instructions. It currently builds on Mac and Linux. A Windows port is coming soon. Nice packages/installers and auto-update is also coming when alpha transitions to beta.

Note that this won't work if your firewall does not allow outbound UDP. It must allow UDP conversations on port 8993 at a minimum.

At present there is only one virtual LAN and you are dumped there by default. It's called Earth, and is exactly what it sounds like. The ability to create and join additional networks is coming soon. Once you're on, visit [earth.zerotier.net](http://earth.zerotier.net/) to see your Earth LAN IP address and other information.

**Security warning:** You read that right. ZeroTier One places your computer on an absolutely open global Ethernet party line. Please ensure that you are up to date on your OS patches and we recommend turning off unnecessary services. Also be sure that anything else you are sharing is password protected provided you don't want to share it: printers, iPhoto and iTunes shares, etc.

ZeroTier One is licensed under the GNU General Public License version 3. Anyone wishing to embed this in a commercial product or create a derivative product should contact [ZeroTier Networks LLC](https://www.zerotier.com/) to obtain a commercial license.

Check out the [blog](https://www.zerotier.com/blog.html) for announcements, in-depth articles, and related thoughts.

----
**FAQ**

**Q:** What can I do with this?  
**A:** For starters, try opening iTunes if you have it installed. If others are also online and sharing their collections, you might see them. If you have any games that run over a LAN (except those that require IPX), try those. What else can you think of to do on a completely flat, open network? Collaborative software development? Remote debugging? Transferring files using simple drive shares? Sharing your desktop printer to someone on another continent? Use your imagination.

**Q:** Why do I get an IP address in the 27.0.0.0 or 28.0.0.0 range? And why does a lookup claim these addresses belong to the U.S. Department of Defense?  
**A:** Short answer: because IPv4 needs to die. Long answer: the Earth network assigns IPv4 IPs from these ranges. They do in fact belong to the DOD, but they are *not* routed to the open Internet. The DOD owns them but uses them internally for private networks. As a result, there is nothing *technically* wrong with "bogarting" these for our own private network. It's considered bad practice, but if you want a private address space in IPv4 that is unlikely to overlap other private address spaces (like 10/8 and 192.168/16), it's the only way. [Cellular carriers](http://www.androidcentral.com/sprint-internet-dept-defense-and-you)  and [cable companies](http://www.dslreports/forum/r25679029-Why-is-my-first-hop-to-a-DoD-assigned-IP-address-) frequently do the same thing.

**Q:** Is IPv6 supported?  
**A:** Yes. IPv6 link-local addresses (those in the fe80::/10 block) are auto-assigned and should work fine. No other IPv6 addresses are assigned *yet*, but there are plans to do interesting things in this area in the future.

**Q:** Are you going to charge for this?  
**A:** Public networks will remain free, but we intend to charge for private virtual LANs. ZeroTier has other ideas too, but they're top secret for the moment.

**Q:** What's a supernode?  
**A:** Supernodes are nodes run by ZeroTier Networks that orindary users use to find one another and communicate until/unless they can perform NAT traversal and connect directly. They run the exact same software as everyone else. The only thing that really makes a supernode special is that it's designated as such.

**Q:** Can I run a supernode?  
**A:** No, not at the moment, and there would be no benefit to doing so.

**Q:** Can you see my traffic? What about other users? Can you sniff the LAN?  
**A:** No. All unicast (direct computer to computer) traffic is encrypted end-to-end (even if it's being relayed), and the ZeroTier virtual LAN behaves like a LAN with a secure enterprise-grade switch that does not allow unicast conversations to be sniffed. Multicast and broadcast traffic will of course be seen by many recipients, but that's the idea.

**Q:** You say "almost unlimited size." Isn't multicast and broadcast traffic eventually going to be too much? What happens then?  
**A:** ZeroTier One uses an algorithm called *implicit social switching*. The overall maximum number of recipients for a multicast is limited, so if there are too many listeners to a given multicast address then obviously not everyone will receive every message. So who does? Social switching causes multicasts to propagate in the direction of peers to whom you have recently communicated. As a result, multicasts tend to propagate along lines of association. The people most likely to get your service announcements are those with whom you frequently connect.

**Q:** I don't see broadcasts.  
**A:** At the moment only Ethernet multicast is propagated, not broadcast (ff:ff:ff:ff:ff:ff). This may change in the future. IPv4 ARP uses broadcast but is handled by special code that subscribes to a multicast group using the broadcast address combined with the IPv4 address to make it address-specific. See comments in MulticastGroup.hpp for deeper technical details.

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
 * Multiple network support is in but there is no interface to configure it, hence it is useless. But in alpha it'll be nice to shove everyone onto "Earth" in order to stress test that little "almost unlimited size" boast. LAN party!
   * Multiple networks would currently all have the same MAC, which some OSes may not like. There is a possible strategy for dealing with this, but it needs to be explored.
 * There is no multiple-launch protection yet. If you launch more than one instance on the same working directory everything breaks. Take care that zerotier-one is not running before launching it again. The command "sudo killall zerotier-one" is helpful.
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
