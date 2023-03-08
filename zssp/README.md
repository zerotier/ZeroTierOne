ZeroTier Secure Socket Protocol
======

# Introduction

ZeroTier Secure Socket Protocol (ZSSP) is a [Noise](http://noiseprotocol.org) protocol implementation using NIST/FIPS/CfSC compliant cryptographic primitives plus post-quantum forward secrecy via [Kyber1024](https://pq-crystals.org/kyber/). It also includes built-in support for fragmentation and defragmentation of large messages with strong resistance against denial of service attacks targeted against the fragmentation protocol.

Specifically ZSSP implements the [Noise XK](http://noiseprotocol.org/noise.html#interactive-handshake-patterns-fundamental) interactive handshake pattern which provides strong forward secrecy not only for data but for the identities of the two participants in the sesssion. The XK pattern was chosen instead of the more popular IK pattern used in popular Noise implementations like Wireguard due to ZeroTier identities being long lived and potentially tied to the real world identity of the user. As a result a Noise pattern providing identity forward secrecy was considered preferable as it offers some level of deniability for recorded traffic even after secrec key compromise.

Hybrid post-quantum forward secrecy using Kyber1024 is performed alongside Noise with the result being mixed in alongside an optional pre-shared key at the end of session negotiation.

ZSSP is designed for use in ZeroTier 2 but is payload-agnostic and could easily be adapted for use in other projects.

## Cryptographic Primitives Used

 - AES-256-GCM: Authenticated encryption
 - HMAC-SHA384: Key mixing, sub-key derivation in key-based KDF construction
 - NIST P-384 ECDH: Elliptic curve key exchange during initial handshake and for periodic re-keying during the session
 - Kyber1024: Quantum attack resistant lattice-based key exchange during initial handshake
 - AES-256-ECB: Single 128-bit block encryption of header information to harden the fragmentation protocol against denial of service attack (see section on header protection)

