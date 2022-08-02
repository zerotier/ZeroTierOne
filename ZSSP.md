# ZeroTier Secure Session Protocol

ZeroTier V2 uses a secure session protocol providing forward security, identity anonymity, and opaqueness on the wire. To this it also adds obfuscation to make ZeroTier indistinguishable from other protocols on the wire by a naive observer who doesn't know participant identities.

## Design Goals and Principles

 - Strong forward secrecy
 - Privacy (e.g. identities are not visible in the clear)
 - Indistinguishability on the wire
 - Simplicity (both state machine and implementation)
 - Highest practical performance on common hardware (x86_64, ARM64, network accelerators)
 - NIST, FIPS, and NSA CNSA (formerly Suite B) compliant
 - Session init is trivially distinguishable from legacy ZeroTier traffic (by the correct recipient node at least) to allow efficient legacy support
 - General enough to be useful outside ZeroTier (perhaps with minor modifications)

## Packet Format

All packets have the following basic layout:

    -- begin common header
    [1]    KKK_UTTT: K == key index, _ == reserved, U == first field is unencrypted, T == packet type
    [4]    32-bit counter (little-endian)
    [6]    48-bit recipient session ID (all zero in INIT)
    -- end common header (11 bytes)
    [...]  unencrypted payload (packet type specific, usually absent)
    -- begin encrypted payload
    [...]  encrypted payload
    -- end encrypted payload
    [...]  cipher-specific authentication tag (16 or 48 bytes)

## Packet Types and Payload

    0: DATA      Session data
    1: INIT      Alice: start new session, send A's key(s)
    2: INIT_ACK  Bob: confirm INIT, send B's key(s)
    3: DATAGRAM  Sessionless datagram
    4: REKEY     New ephemeral key(s)
    5: REKEY_ACK Acknowledge new ephemeral key(s)
    6-7: reserved

DATA carries an arbitrary payload.

All other types carry a payload consisting of pair(s) of single byte field types followed by field data. Field order is not significant except for INIT and DATAGRAM, which place on ephemeral key in an unencrypted payload section before encryption starts. The unencrypted payload section is empty for all other packet types.

## Symmetric Cipher Modes and Key Management

Encryption and authentication are achieved using one of two available cipher modes: AES-256-CTR+HMAC-SHA384 or AES-256-GCM. The packet type determines which mode must be used. DATA packets use AES-256-GCM while all other packet types use AES-256-CTR+HMAC-SHA384. These two modes differ only by which MAC is used, since GCM is just CTR+GMAC.

HMAC authentication is computed over the entire packet after encryption but before obfuscation (see below). GCM authentication covers the encrypted payload only (there is no unencrypted payload in DATA), but since the header forms most of the nonce/IV it is also effectively included (without having to be added as AAD).

HMAC-based key derivation is used to derive sub-keys for each mode to avoid using the same key for multiple purposes or algorithms. The KDF used in ZeroTier is as follows:

    HMAC-SHA384(first 48 bytes of master key, [0, 0, 0, 0, 'Z', 'T', label, 0, 0, 0, 0, 0x01, 0x80])

This construction is based on [section 5.1 page 12 of NIST SP 800-108](https://csrc.nist.gov/publications/detail/sp/800-108/final) with the label prefixed by the ASCII characters "ZT" and the counter always being set to zero.

The following single byte labels are used:

    'c' - AES-256-CTR (first 32 bytes of derived key)
    'm' - HMAC-SHA384 (48 bytes, used in combination with AES-CTR but derive a different key)
    'g' - AES-256-GCM (first 32 bytes of derived key)

Both CTR and GCM take a 12-byte nonce that is constructed from the 11-byte common header and a single byte indicating whether this is "alice" (INIT sender) or "bob" (responder) to prevent nonce reuse when alice and bob share the same session key.

    Nonce bytes: TCCCCSSSSSSR
        T: Packed KKCCTTTT byte from header
        C: Counter (little-endian)
        S: Recipient session ID
        R: Role: 0 for Alice (INIT sender), 1 for Bob (INIT recipient)

The counter may be initialized to any value at session start and MUST be incremented for each packet sent.

Re-keying is initiated (by either side) after 1,073,741,824 (2^30) encryptions or one hour, whichever comes first. Communication may continue until re-keying is successful unless the key usage counter reaches 2^32. This is a hard error and causes abandonment of the session. Re-keying at 1/4 this number should allow plenty of time for re-key success before this limit is reached. See below for more details on re-keying.

## Obfuscation

The unencrypted header and any unencrypted payload (such as the first ephemeral setup key) are obfuscated.

Obfuscation is performed by applying AES-256-CTR using the last 12 bytes of the packet (part of the HMAC or GCM tag) as a nonce and the first 32 bytes of the recipient node's static identity fingerprint as a key.

Deobfuscation therefore requires knowledge of the full identity of the recipient. An observer who doesn't know this identity only sees noise and can't distinguish ZeroTier traffic from any other encrypted traffic.

Obfuscation isn't essential. The protocol would function normally without it and its security guarantees around data privacy and authentication would not be impacted. The purpose of obfuscation is to harden the protocol against tracking and de-anonymization of users through bulk traffic analysis and to provide a very low CPU overhead way for nodes to filter out unwanted packets and maintain "radio silence."

(Even if someone did manage to guess (with a one in 2^53 chance) a valid obfuscated INIT header, they would still receive no response since the rest of the packet wouldn't be correct. They'd just manage to use a few more CPU cycles on the recipient for this one guess to be further examined and discarded when payload decoding or full HMAC authentication failed.)

## Session Setup

## Re-Keying

## Sessionless Datagrams

DATAGRAM packets are not associated with a session and do not initiate one. They are used when nodes need to exchange sporadic control plane messages and the overhead of establishing a session is not justified.

DATAGRAM packets look just like INIT but contain only the sender's identity, a data payload, and an optional auth data payload. The ephemeral key is used to derive a setup key as in INIT but these keys are only used once and then discarded.

## Credits

This protocol design is based on the [Noise protocol framework](http://noiseprotocol.org) by Trevor Perrin and [Wireguard](https://www.wireguard.com) by Jason A. Donenfeld. Most of the credit goes to them and whomever else was involved in these designs for the way this is constructed.
