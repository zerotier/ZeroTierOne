ZeroTier Security
======

## Summary


## Using ZeroTier Securely

### Overall Recommendations

*TL;DR: same as anything else: defense in depth defense in depth defense in depth.*

We encourage our users to treat private ZeroTier networks as being rougly equivalent in security to WPA2-enterprise securied WiFi or on-premise wired Ethernet. (Public networks on the other hand are open by design.) That means they're networks with perimeters, but like all networks the compromise of any participating device or network controller allows an attacker to breach this perimeter.

**Never trust the network.** Many modern security professionals discourage reliance on network perimeters as major components in any security strategy, and we strongly agree regardless of whether your network is physical or virtual.

As part of a defense in depth approach **we specifically encourage the use of other secure protocols and authentication systems over ZeroTier networks**. While the use of secure encrypted protocols like SSH and SSL over ZeroTier adds a bit more overhead, it greatly reduces the chance of total compromise.

Imagine that the per-day probability of a major "0-day" security flaw in ZeroTier and OpenSSH are both roughly 0.001 or one per thousand days. Using both at the same time gives you a cumulative 0-day risk of roughly 0.000001 or one per one million days.

Those are made-up numbers. In reality these probabilities can't be known ahead of time. History shows that a 0-day could be found in anything tomorrow, next week, or never. But layers of security give you an overall posture that is the product -- more than the sum -- of its parts. That's how defense in depth works.

### ZeroTier Specifics

#### Protect Your Identity

Each ZeroTier device has an identity. The secret portion of this identity is stored in a file called "identity.secret." *Protect this file.* If it's stolen your device's identity (as represented by its 10-digit ZeroTier address) can easily be stolen or impersonated and your traffic can be decrypted or man-in-the-middle'd.

#### Protect Your Controller

The second major component of ZeroTier network security is the network controller. It's responsible for issuing certificates and configuration information to all network members. That makes it a certificate authority. Compromise of the controller allows an attacker to join or disrupt any network the controller controls. It does *not*, however, allow an attacker to decrypt peer to peer unicast traffic.

If you are using our controller-as-a-service at [my.zerotier.com](https://my.zerotier.com), you are delegating this responsibility to us.

## Security Priorities

These are our security "must-haves." If the system fails in any of these objectives it is broken.

* ZeroTier must be secure against remote vulnerabilities. This includes things like unauthorized remote control, remote penetration of the device using ZeroTier as a vector, or remote injection of malware.

* The content (but not meta-data) of communication must be secure against eavesdropping on the wire by any known means. (We can't warrant against secret vulnerabilities against ciphers, etc., or anything else we don't know about.)

* Communication must be secure against man-in-the-middle attacks and remote device impersonation.

## Security Non-Priorities

There are a few aspects of security we knowingly do not address, since doing so would be beyond scope or would conflict too greatly with other priorities.

* ZeroTier makes no effort to conceal communication meta-data such as source and destination addresses and the amount of information transferred between peers. To do this more or less requires onion routing or other "heavy" approaches to anonymity, and this is beyond scope.

* ZeroTier does not implement complex certificate chains, X.509, or other feature-rich (some would say feature-laden) cryptographic stuff. We only implement the crypto we need to get the job done.

* We don't take extraordinary measures to preserve security under conditions in which an endpoint device has been penetrated by other means (e.g. "rooted" by third party malware) or physicall compromised. If someone steals your keys they've stolen your keys, and if they've "pwned" your device they can easily eavesdrop on everything directly.

## Insecurities and Areas for Improvement

The only perfectly secure system is one that is off. All real world systems have potential security weaknesses. If possible, we like to know what these are and acknowledge their existence.

In some cases we plan to improve these. In other cases we have deliberately decided to "punt" on them in favor of some other priority (see philosophy). We may or may not revisit this decision in the future.

* We don't implement forward secrecy / ephemeral keys. A [discussion of this can be found at the closed GitHub issue for this feature](https://github.com/zerotier/ZeroTierOne/issues/204). In short: we've decided to "punt" on this feature because it introduces complexity and state negotiation. One of the design goals of ZeroTier is "reliability convergence" -- the reliability of ZeroTier virtual networks should rapidly converge with that of the underlying physical wire. Any state that must be negotiated prior to communication multiplies the probability of delay or failure due to packet loss. We *may* revisit this decision at a later date.

## Secure Coding Practices

The first line of defense employed against remote vulnerabilities and other major security flaws is the use of secure coding practices. These are, in no particular order:

* All parsing of remote messages is performed via higher level safe bounds-checked data structures and interfaces. See node/Buffer.hpp for one of the core elements of this.
 
* C++ exceptions are used to ensure that any unhandled failure or error condition (such as a bounds checking violation) results in the safe and complete termination of message processing. Invalid messages are dropped and ignored.
 
* Minimalism is a secure coding practice. There is an exponential relationship between complexity and the probability of bugs, and complex designs are much harder to audit and reason about.

* Our build scripts try to enable any OS and compiler level security features such as ASLR and "stack canaries" on non-debug builds.

## Cryptographic Security Practices

* We use [boring crypto](https://cr.yp.to/talks/2015.10.05/slides-djb-20151005-a4.pdf). A single symmetric algorithm (Salsa20/12), a single asymmetric algorithm (Curve25519 ECDH-256), and a single MAC (Poly1305). The way these algorithms are used is identical to how they're used in the NaCl reference implementation. The protocol supports selection of alternative algorithms but only for "future proofing" in the case that a serious flaw is discovered in any of these. Avoding algorithm bloat and cryptographic state negotiation helps guard against down-grade, "oracle," and other protocol level attacks.

* Authenticated encryption is employed with authentication being performed prior to any other operations on received messages. See also: [the cryptographic doom principle](https://moxie.org/blog/the-cryptographic-doom-principle/).

* "Never branch on anything secret" -- deterministic-time comparisons and other operations are used in cryptographic operations. See Utils::secureEq() in node/Utils.hpp.

* OS-derived crypographic random numbers (/dev/urandom or Windows CryptGenRandom) are further randomized using encryption by a secondary key with a secondary source of entropy to guard against CSPRNG bugs. Such OS-level CSPRNG bugs have been found in the past. See Utils::getSecureRandom() in node/Utils.hpp.

