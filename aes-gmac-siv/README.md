AES-GMAC-SIV
======

Rust implementation for ZeroTier 2.0.

*WARNING: while the AES-GMAC-SIV construction has been subjected to peer review, this code has not (yet!). Use at your own risk.*

## Introduction

AES-GMAC-SIV is a "synthetic IV" (SIV) cipher construction implemented using only FIPS140 and NIST accepted cryptographic building blocks: AES-ECB (used to mix input IV and GMAC result), AES-CTR, and GMAC (the MAC part of GCM, which can also be used separately).

AES-GMAC-SIV is almost identical to [AES-GCM-SIV](https://en.wikipedia.org/wiki/AES-GCM-SIV), but that mode uses a non-standard MAC called POLYVAL in place of GMAC. POLYVAL is basically little-endian GMAC but the fact that it is not standard GMAC means it's not found in most cryptographic libraries and is not approved by FIPS140 and many other sets of compliance guidelines.

## Why SIV? Why not just GCM?

Stream ciphers like AES-CTR, ChaCha20, and others require a number called an initialization vector (IV) for each use. The IV is sometimes called a nonce, or *number used once*, because using the same value for different messages with the same key is a major no-no.

Repeating an IV/nonce with the same key allows both messages to be decrypted. This is because XOR, which is used to apply the stream cipher's pseudorandom bits as a one time pad, is commutative. Repeating a nonce can in some cases also allow an attacker to attack the MAC (e.g. GMAC or Poly1305) and forge messages that will appear valid. Message forgery is often more serious than loss of plaintext secrecy for a few messages as it permits active attacks.

SIV modes provide strong protection against IV reuse by generating a *synthetic IV* from the plaintext. This means that two different plaintexts will result in different IVs even if the input IV is duplicated.

With SIV a duplicate IV has no effect at all except in the case where the same IV is used to encrypt the same message twice. In this case the encrypted messages would also be identical, revealing that a duplicate was sent, but because both the IV and message are the same this would not compromise security like IV reuse does in standard modes. It's technically possible to use SIV modes with no IV at all without compromising plaintext secrecy, but this isn't recommended as it would still always leak message duplication.

We recommend treating AES-GMAC-SIV (and other SIV modes) as if they were normal stream ciphers and endeavoring to make the IV unique as those would require.

SIV modes might seem like paranoia, but accidental IV reuse is easier than you might think. Here's a few scenarios where it might happen:

 * Live cloning of virtual machines or application state, resulting in two clones with identical random number generator states.
 * Embedded devices that initialize PRNGs from deterministic sources.
 * Forgetting to use atomics or a mutex to synchronize an IV counter variable in multithreaded code. 
 * Edge case memory model differences between processors affecting synchronization of an IV counter variable.
 * Concurrent use of a non-thread-safe random number generator.
 * Multiple dependencies in a project stomping on the same random number generator state.
 * Memory errors that corrupt an IV counter variable, especially if they can be triggered remotely.
 * "Rowhammer" and similar hardware attacks targeting the IV counter.
 * Time changes or attacks against NTP if a clock is used as input in genreating an IV.

... and so on. "Sudden death" on IV re-use is a foot-gun that's worth removing.

## AES-GMAC-SIV construction

![AES-GMAC-SIV block diagram](AES-GMAC-SIV.png)

Initialization parameters:

 1. K0, an AES key used to initialize AES-GMAC.
 2. K1, a second (and different) AES key used to initialize AES-ECB and AES-CTR.

Per-message parameters:

 1. A per-message unique 64-bit IV (can be a counter or random).
 2. Optional additional associated data (AAD) to authenticate but not encrypt.
 3. A plaintext message to encrypt.

Encryption steps:

 1. Pad 64-bit IV to 96 bits and use it to initialize GMAC.
 2. Feed AAD (if any) into GMAC.
 3. Pad AAD length to a multiple of 16 by feeding zeroes into GMAC to ensure unique encoding.
 4. Feed plaintext into GMAC to compute final MAC.
 5. XOR lower 64 bits and higher 64 bits of GMAC tag.
 6. Concatenate original input IV and 64-bit shortened tag to form a 128-bit block.
 7. AES-ECB encrypt this IV+tag, yielding an opaque 128-bit message tag and AES-CTR IV.
 8. Clear bit 31 (from the right) in the tag and use this to initialize AES-CTR. Bit 31 is cleared so AES-CTR implementations that use a 32-bit counter will not overflow for messages less than 2^31 bytes in length.
 9. Encrypt plaintext with AES-CTR.

The message tag is the 128-bit encrypted block from step 7 before bit 31 is cleared. Only this 128-bit tag needs to be sent with the message. The IV supplied for encryption should not be sent, as it's obtained by decrypting the tag.

Decryption steps:

 1. Initialize AES-CTR with the tag after clearning bit 31 as in step 8 above.
 2. Decrypt ciphertext with AES-CTR.
 3. AES-ECB *decrypt* 128-bit tag to yield original IV and 64-bit shortened GMAC tag.
 4. Initialize GMAC as in encryption step 1 using first 64 bits of the decrypted message tag from step 3.
 5. Feed AAD into GMAC (if any).
 6. Zero-pad AAD to a multiple of 16 as in encryption.
 7. Feed *decrypted plaintext* into GMAC.
 8. Generate GMAC tag, XOR least and most significant 64 bits, and check MAC. Discard packet if these do not match.

Notes:

 * The extra step (7 in encrypt, 3 in decrypt) of AES-ECB encrypting (mixing) the bits of the input IV and GMAC tag is there because two identical plaintexts would yield identical GMAC tags. Without this single block encryption step this would leak plaintext duplication to anyone monitoring traffic. This step has no security impact on AES-CTR.
 * Clearing of bit 31 in the AES-CTR IV as in step 8 of encryption is technically not required but is there for compatibility with some AES-CTR implementations. If message size exceeds (16 * 2^31) bytes this becomes moot, but that's huge for a single message. If an AES-CTR implementation supports 64-bit or 128-bit counter semantics then this construction can be used for messages of effectively unlimited size.

## Performance

Performance is very close to AES-GCM on a given platform. It's very slightly slower because encryption requires two passes, but for short messages the second pass will operate on data already in the CPU's L0 cache which minimizes the actual overhead.

You can run tests with `cargo test --release -- --nocapture` and see encrypt and decrypt performance. Here's some single core benchmarks:

 * AMD Threadripper 2990WX: **2055.60 MiB/sec**, decrypt **2050.09 MiB/sec** (per core).
 * Apple M1 Mac Mini (2021): encrypt **4868.89 MiB/sec**, decrypt **5059.39 MiB/sec** (per performance core).

In general this construction performs better than ChaChaPoly or other ARX ciphers on processors that have AES hardware acceleration and considerably worse on processors that lack it. Performance on systems without hardware acceleration is generally still good enough for most applications.

## Cryptanalysis

[Trail of Bits](https://www.trailofbits.com) analyzed AES-GMAC-SIV as part of their [design analysis of ZeroTier 2.0](https://github.com/trailofbits/publications/blob/master/reviews/ZeroTierProtocol.pdf) and concluded that its security is equivalent to the original AES-SIV and AES-GCM-SIV constructions.

The algorithms on which this is built, namely AES, AES-CTR, and GMAC, are well known and standard.

## FIPS-140 Compliance

AES-ECB, AES-CTR, and GMAC are all algorithms allowed by FIPS-140. For FIPS purposes AES-GMAC-SIV would be described as AES-CTR with GMAC authentication. Since this is built out of compliant components, a standard FIPS-certified cryptographic library could be used.

## Dependencies

This is implemented using the [libgcrypt](https://github.com/gpg/libgcrypt) library (via Rust bindings) on Linux, BSD, and Windows, and built-in CommonCrypto libraries on MacOS and iOS. CommonCrypto was used on Mac because libgcrypt has issues on the ARM64 platform with Apple's clang. An OpenSSL based implementation is also included but it is only used on a few platforms where libgcrypt does not yet support native hardware acceleration, such as IBM S390x.

## License

This Rust implementation of AES-GMAC-SIV is released under the BSD 2-clause license.

(c) 2021 ZeroTier, Inc.
