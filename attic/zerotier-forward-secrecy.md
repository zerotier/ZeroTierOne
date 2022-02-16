# ZeroTier Forward Secrecy Design (draft)

Author: Adam Ierymenko / adam.ierymenko@zerotier.com

## Design Goals

 - Implement security qualities of more modern protocols like Wireguard or Signal.
 - Support FIPS compliance because we have a bunch of customers who want it.
 - Continue to use a non-FIPS algorithm too because another huge camp of users are afraid of NIST ECC curves.
 - Improve hardening against DOS and replay attacks.

## Algorithms

 - AES-GMAC-SIV for authenticated symmetric encryption (not described here).
 - HMAC-SHA512 for key derivation.
 - Curve25519 for asymmetric key agreement
 - NIST P-521 for asymmetric key agreement
 - *Maybe* a PQ candidate algorithm to protect against scenarios in which data is warehoused until a QC is available. Considering SIDH, CRYSTALS-KYBER, or NTRU.
   - Can't easily use the "throw in a static secret" trick used by WireGuard since ZT VL1 sessions are shared among multiple trust boundaries if one is a member of multiple overlapping virtual networks. Which static secret would one use in that case? What if network membership changes? We don't want to set up N totally redundant VL1 sessions between Alice and Bob if they share membership in N networks.

## Hybrid Cryptography

Supporting both FIPS and non-FIPS and possibly a PQ algorithm requires the use of hybrid cryptography. This is achieved by performing each KEX in the re-key sequence with multiple algorithms and combining the results. Combination is via HMAC-SHA384(previous, current) where previous is the "key" and current is the "message."

Exchange uses all algorithms mutually supported by both sides (bitwise AND). This seems less vulnerable to a potential future downgrade attack and to strengthen key agreement overall. All algorithms would have to be broken to break the result of such a chain, making it always as strong as the strongest algorithm.

FIPS compliance can be achieved by always placing FIPS-compliant algorithms at the end of the list of chained algorithms. So if we are using curve25519 and NIST P-521 the final master key is HMAC-SHA384(curve25519 secret, NIST P-521 secret). FIPS would consider the curve25519 secret a "salt," and FIPS documents do not specify where the salt must originate or if it must be private or public. It doesn't matter cryptographically since the output of HMAC-SHA384(public, secret) is secret.

*At some point I must write a blog post on smuggling better crypto into FIPS environments through clever use of FIPS primitives.*

## Session Example

Notes:

 - Alice starts knowing Bob's identity (which contains long-lived identity key(s)), and vice versa. These are obtained via root nodes which act as identity caches.
 - Consider public and private keys to actually be bundles of keys containing a key for each algorithm.

1. Each side sends HELLO containing:
    - 64-bit counter / nonce
    - ZeroTier address information (authenticated but not encrypted)
    - Message type
    - Sender's ZeroTier identity with their long-lived public key(s)
    - Encrypted payload section:
      - Ephemeral public key(s) of initiating node
      - Wall clock timestamp (milliseconds since epoch)
      - Monotonic timestamp (milliseconds since some time in the past)
      - HMAC-SHA512(previous session key) (using static identity key)
      - Other ZeroTier-related fields (not cryptographically important)
    - SHA512 hash of recipient's ZeroTier identity
    - Full HMAC-SHA512 of entire HELLO packet (using static identity key)

2. Recipients of HELLO respond with OK(HELLO) containing:
    - Standard ZeroTier encrypted packet headers, addresses, etc.
    - Ephemeral public key(s) of responding node
    - Wall clock timestamp (milliseconds since epoch)
    - Monotonic timestamp (milliseconds since some time in the past)
    - HMAC-SHA512(new session key) (using static identity key)
    - Other ZeroTier-related fields (not cryptographically important)
    - Full HMAC-SHA512 of entire OK(HELLO) packet (using static identity key)

## Sources

 - https://soatok.blog/2022/01/27/the-controversy-surrounding-hybrid-cryptography/
 - https://www.wireguard.com/papers/wireguard.pdf
 - https://www.signal.org/blog/advanced-ratcheting/
