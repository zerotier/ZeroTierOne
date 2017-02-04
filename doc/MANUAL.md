ZeroTier Planetary Switch Users Guide
======

This manual describes the design and operation of ZeroTier and its associated services, apps, and libraries. Its intended audience includes IT professionals, network administrators, information security experts, and developers.

[ZeroTier Central](https://my.zerotier.com/), our enterprise web UI, has its own help and documentation that can be accessed through its interface.

## Table of Contents

1. [Introduction](#1)
2. [How it Works](#2)
   1. [VL1: The ZeroTier Peer to Peer Network](#2_1)
      1. [Network Topology and Peer Discovery](#2_1_1)
      2. [Addressing](#2_1_2)
      3. [Encryption and Authentication](#2_1_3)
      4. [Trusted Paths for Fast Local SDN](#2_1_4)
      5. [Troubleshooting Connectivity Problems](#2_1_5)
   2. [VL2: The Ethernet Virtualization Layer](#2_2)
      1. [Networks Identifiers and Controllers](#2_2_1)
      2. [Multicast](#2_2_2)
      3. [Ethernet Bridging](#2_2_3)
      4. [Certificates of Membership](#2_2_4)
      5. [Revocations and Fast Propagation](#2_2_5)
      6. [Rules Engine](#2_2_6)
      7. [Capabilities](#2_2_7)
      8. [Tags](#2_2_8)
5. [ZeroTier One: The Network Virtualization Service](#5)
   1. [Installation and Operation](#5_1)
      1. [Joining and Leaving Networks](#5_1_1)
      2. [Listing VL1 Peers for Network Troubleshooting](#5_1_2)
      3. [Joining Federated Roots ("Orbiting Moons")](#5_1_3)
   2. [Communicating With the Service](#5_2)
   3. [Operating a Network Controller](#5_3)
      1. [Controller Configuration](#5_3_1)
      2. [High Availability](#5_3_2)
      3. [Security Considerations](#5_3_3)
   4. [Mobile Apps](#5_4)
   5. [Advanced Topics](#5_5)
      1. [Static Paths and Interface Exclusion](#5_5_1)
      2. [Defining Trusted Paths](#5_5_2)
      3. [Allowing Remote Administrative Requests](#5_5_3)
      4. [Creating Federated Roots ("Moons")](#5_5_4)
      5. [Clustering and Geo-Optimized Routing](#5_5_5)
6. [Common Use Cases](#6)
   1. [SDN and General Network Virtualization](#6_1)
   2. [Replacing Conventional VPNs for Remote Access](#6_2)
      1. [Layer 2 Bridge Deployment Strategy](#6_2_1)
      2. [Layer 3 Router Deployment Strategy](#6_2_2)
      3. [Full Tunnel / Default Gateway Override](#6_2_3)
   3. [Multi-Site and Hybrid Cloud Backplane](#6_3)
      1. [Configuring for Specific Cloud Providers](#6_3_1)
         1. [Amazon](#6_3_1_1)
         2. [Microsoft Azure](#6_3_1_2)
         3. [Google Cloud](#6_3_1_3)
         4. [Digital Ocean, Vultr, Linode, OVH, etc.](#6_3_1_4)
   4. [SD-WAN for Site-to-Site Connectivity](#6_4)
      1. [Layer 2 Bridge Deployment Strategy](#6_4_1)
      2. [Layer 3 Router Deployment Strategy](#6_4_2)
   5. [Deivce (IoT) and Application Peer-to-Peer Networking](#6_5)
      1. [Running ZeroTier One in Containers and Virtual Appliances](#6_5_1)
      2. [ZeroTier One on Linux or BSD Powered IoT Devices](#6_5_2)
7. [For Developers: Connecting IoT Devices and Apps](#7)
   1. [ZeroTier SDK for Apps](#7_1)
   2. [ZeroTier Network Hypervisor Core](#7_2)
      1. [Code Layout and Design](#7_2_1)
      2. [Building and Using](#7_2_2)
7. [Licensing](#8)

------

## **1.** Introduction <a name="1"></a>

ZeroTier is a smart Ethernet switch for planet Earth.

When the world is a single data center VPN, SDN, SD-WAN, and application peer to peer networking converge. The vast byzantine complexity of managing all these systems independently largely disappears. We've re-thought networking from first principles to deliver the flat end-to-end simplicity of the original pre-NAT pre-mobility Internet but in a way that meets the security, privacy, and mobility requirements of the 21st century.

This guide is written for users with at least an intermediate understanding of topics like Ethernet and TCP/IP networking. It explains ZeroTier's design and use in considerable detail. Most users with sufficient IT expertise to configure a router or firewall will *not* need this guide to deploy ZeroTier for simple use cases. Indeed we've built a substantial user base prior to its publication.

So before reading all this you might want to just try installing ZeroTier on a few things and creating a network. Come back when you want to understand what's happening or when you need to make use of more advanced features like rules, capabilities, federation, or clustering.

## **2.** How it Works <a name="2"></a>

This section explains how ZeroTier's network hypervisor works. It's not required reading to operate ZeroTier for all but the most advanced deployments, but understanding how things work is always helpful if you ever need to troubleshoot anything.

ZeroTier is comprised of two closely coupled but conceptually distinct layers [in the OSI model](https://en.wikipedia.org/wiki/OSI_model) sense: a virtual "wire" layer called VL1 that moves data around and a virtual switched Ethernet layer called VL2 to provide devices and apps with a familiar interface. Since almost any protocol can be carried over Ethernet, emulating standard Ethernet behavior maximizes versatility.

### **2.1.** VL1: The ZeroTier Peer to Peer Network

To build a planetary data center we first had to begin with the wiring. Tunneling into the Earth's core and putting a giant wire closet down there wasn't an option, so we decided to use software to build virtual wires over the existing Internet instead.

In conventional networks L1 (OSI layer 1) refers to the actual CAT5/CAT6 cables or wireless radio channels over which data is carried and the physical transciever chips that modulate and demodulate it. VL1 is a peer to peer network that does the same thing by using encryption, authentication, and a lot of networking tricks to create virtual wires as needed.

### **2.1.1.** Network Topology and Peer Discovery

VL1's persistent structure is a hierarchical tree similar to DNS, but its leaves make direct ephemeral connections to one another on demand. At the base of the tree resides a pool of equal and fully redundant *roots* whose function is closely analogous to that of [DNS root name servers](https://en.wikipedia.org/wiki/Root_name_server).

Roots run the same software as regular endpoints but reside at fast stable locations on the network and are designated as such by a *world definition*. There are two kinds of world definitions: a *planet* and a *moon*. The ZeroTier protocol contains a secure mechanism allowing world definitions to be updated in band.

There is only one planet. Earth's root servers are operated by ZeroTier, Inc. as a free service. Their presence defines and unifies the global data center where we all reside.

Users can create "moons." These nominate additional roots for redundancy or performance. The most common reasons for doing this are to eliminate hard dependency on ZeroTier's third party infrastructure or to designate local roots inside your building or cloud so ZeroTier can work without a connection to the Internet. Moons are by no means required and most of our users get by just fine without them. 

When peers start out they have no direct links to one another, only upstream to roots. Every peer on VL1 possesses a globally unique address, but unlike IP addresses these are opaque cryptographic identifiers that encode no routing information. To communicate peers first send packets "up" the tree, and as these packets traverse the network they trigger the opportunistic creation of direct links along the way. The tree is constantly trying to "collapse itself" to optimize itself to the pattern of traffic it is carrying.

In the simplest case using only global roots, an initial connection setup between peers A and B goes like this:

1. A wants to send a packet to B, but since it has no direct path it sends it upstream to R (a root).
2. R *does* have a direct link to B so it forwards the packet there.
3. R also sends a message called *RENDEZVOUS* to A containing hints about how it might reach B, and to B informing it how it might reach A. We call this "transport triggered link provisioning."
4. A and B get *RENDEZVOUS* and attempt to send test messages to each other, possibly accomplishing [hole punching](https://en.wikipedia.org/wiki/UDP_hole_punching) of any NATs or stateful firewalls that happen to be in the way. If this works a direct link is established and packets no longer need to take the scenic route.

If R is a federated root the same process occurs, but possibly with additional steps.

VL1 provides instant always-on virtual L1 connectivity between all devices in the world. Indirect paths are automatically and transparently upgraded to direct paths whenever possible, and if a direct path is lost ZeroTier falls back to indirect communication and the process begins again.

If a direct path can never be established, indirect communication can continue forever with direct connection attempts also continuing indefinitely on a periodic basis. The protocol also contains other facilities for direct connectivity establishment such as LAN peer discovery, port prediction to traverse IPv4 symmetric NATs, and explicit endpoint advertisement that can be coupled with port mapping using uPnP or NAT-PMP if available. VL1 is persistent and determined when it comes to finding the most efficient way to move data.

This is not a wholly unique design. It shares features in common with STUN/ICE, SIP, and other protocols. The most novel aspect may be the simplification achieved through lazy transport triggered link provisioning. This trades the [complicated state machines of STUN/ICE](https://www.pkcsecurity.com/untangling-webrtc-flow.html) for a stateless algorithm with implicit empirical parameters. It also eliminates asymmetry. As mentioned above, roots run the same code as regular nodes.

*[A blog post from 2014 by ZeroTier's original author explains some of the reasoning process that led to this design.](http://adamierymenko.com/decentralization-i-want-to-believe/)*

### **2.1.2.** Addressing

Every device (a "device" can be anything from a laptop to an app) is identified on VL1 by a 40-bit (10 hex digit) unique *ZeroTier address*. These are the addresses used to address packets in the process described in 2.1.1 above.

These addresses are computed from the public portion of a public/private key pair. An address along with its public key is called an *identity*. If you look at the home directory of a running ZeroTier instance you will see `identity.public` and `identity.secret`.

When ZeroTier starts for the first time it generates a new key pair and a new identity. It then attempts to advertise it upstream to the network. In the very unlikely event that the identity's 40-bit unique address is taken, it discards it and generates another.

Identities are claimed on a first come first serve basis and currently expire from global roots after 60 days of inactivity. If a long-dormant device returns it may re-claim its identity unless its address has been taken in the meantime (again, highly unlikely). 

The address derivation algorithm used to compute addresses from public keys imposes a computational cost barrier against the intentional generation of a collision. Currently it would take approximately 10,000 CPU-years to do so (assuming e.g. a 3ghz Intel core). This is expensive but not impossible, but it's only the first line of defense. After generating a collision an attacker would then have to compromise all upstream nodes and replace the address's cached identity, not to mention also doing the same for peers that have seen the target identity recently.

In addition to assisting with communication, upstream nodes also act as identity caches. If the identity corresponding to an address is not known a peer may request it by sending a message called *WHOIS* upstream.

### **2.1.3.** Encryption and Authentication

If you don't know much about cryptography you can safely skip this section. **TL;DR: packets are end-to-end encrypted and can't be read by roots or anyone else, and we use modern 256-bit crypto in ways recommended by the professional cryptographers that created it.**

Asymmetric public key encryption is [Curve25519/Ed25519](https://en.wikipedia.org/wiki/Curve25519), a 256-bit elliptic curve variant.

Every VL1 packet is encrypted end to end using (as of the current version) 256-bit [Salsa20](https://ianix.com/pub/salsa20-deployment.html) and authenticated using the [Poly1305](https://en.wikipedia.org/wiki/Poly1305) message authentication (MAC) algorithm.

MAC is computed after encryption [(encrypt-then-MAC)](https://tonyarcieri.com/all-the-crypto-code-youve-ever-written-is-probably-broken) and the cipher/MAC composition used is identical to the [NaCl reference implementation](https://nacl.cr.yp.to).

As of today we do not implement [forward secrecy](https://en.wikipedia.org/wiki/Forward_secrecy) or other stateful cryptographic features in VL1. We don't do this for the sake of simplicity, reliability, and code footprint, and because frequently changing state makes features like clustering and fail-over much harder to implement. See [our discussion on GitHub](https://github.com/zerotier/ZeroTierOne/issues/204).

For those who have very high security needs and want forward secrecy, we currently recommend the use of encrypted protocols such as SSH and SSL over ZeroTier. Not only do these provide forward secrecy, the use of multiple layers of encryption in this way provides excellent [defense in depth](https://en.wikipedia.org/wiki/Defense_in_depth_%28computing%29). The computational cost of this additional crypto is typically small, and the benefit can potentially be large. All software can contain bugs, but multiple layers of protection means that discovery of a catastrophic bug in any one layer does not result in compromise of your entire system. We recommend the same for authentication. While ZeroTier VL2 provides certificate-based network boundary enforcement, we do not recommend that users rely solely on this for access control to critical systems. It is always good to use more than one security measure whenever practical.

### **2.1.4.** Trusted Paths for Fast Local SDN

To support the use of ZeroTier as a high performance SDN/NFV protocol over physically secure networks the protocol supports a feature called *trusted paths*. It is possible to configure all ZeroTier devices on a given network to skip encryption and authentication for traffic over a designated physical path. This can cut CPU use noticably in high traffic scenarios but at the expense of effectively all transport security over the configured trusted backplane.

Trusted paths do not prevent communication with devices elsewhere, since traffic over other paths will be encrypted and authenticated normally.

We don't recommend the use of this feature unless you really need the performance and you know what you're doing. Extra security is never a bad thing. We also recommend thinking carefully before disabling transport security on a cloud private network. Larger cloud providers such as Amazon and Azure tend to provide good network segregation but many less costly providers offer private networks that are "party lines." For these the encryption and authentication provided by ZeroTier is very desirable. In fact, we have a few users using ZeroTier exactly for this reason.

### **2.1.5.** Troubleshooting Connectivity Problems

