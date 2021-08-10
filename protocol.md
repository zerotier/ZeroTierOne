GVSP (ZeroTier) Protocol Specification
------

*(c) 2021 ZeroTier, Inc.*

# Introduction

The ZeroTier protocol, or *Global Virtual Switch Protocol* (GVSP), provides what is essentially a planetary scale Ethernet switch with VLAN support and sophisticated security monitoring, access control, and traffic control capabilities. Its purpose is to provide a single software defined networking layer that can carry almost any protocol and that achieves the goals of numerous discrete products and services in a single system.

GVSP is organized into two closely coupled but separate layers: VL1 and VL2.

VL1 (Virtual Layer 1) provides a global scale link layer in the form of a secure cryptographically addressed peer to peer protocol. VL1 only concerns itself with efficiently moving packets and providing transport level encryption and authentication. Its operation could be compared to the transport security aspects of IPSec, WireGuard, or OpenSSL, but with a global address space. VL1 exposes its cryptographic tokens, identifiers, assurances, and primitives to higher layers, which makes implementation of security features at VL2 considerably easier.

VL2 (Virtual Layer 2) is an Ethernet emulation layer somewhat similar to VXLAN. It provides virtual Ethernet enclaves with certificate access control, globally configurable rules, and numerous other features normally only found in enterprise smart switches and SDN systems. Placing an Ethernet virtualization protocol on top of a global peer to peer protocol creates a kind of "planetary data center" or "planetary cloud region."

## VL1: Virtual Layer 1 (Cryptogaphically Addressed P2P Link Layer)

### Identities and Addressing

#### Identity Type 0: Curve25519 ECDH / Ed25519 EDDSA

#### Identity Type 1: Curve25519 ECDH + NIST P-521 ECDH / Ed25519 EDDSA + NIST P-521 ECDSA

### Packet Structure

### Fragmentation

### Encryption and Authentication (AEAD)

### Forward Secrecy (Ephemeral Keys)

#### AES-GMAC-SIV

#### Salsa20/12-Poly1305 (deprecated)

#### NONE/Poly1305 (used with HELLO, otherwise deprecated)

In this mode Poly1305 is initialized as it is with Salsa20/12-Poly1305 but is then used to authenticate the payload as plaintext. The payload is not encrypted, but a Poly1305 MAC that can be checked by the receiver is still generated.

### Root Nodes

### Establishment of Peer to Peer Connectivity

### Root Fall-Back and Re-Establishment of Connectivity on Failure

### Packet Types ("Verbs")

#### 0x00 / NOP

NOP, as the name suggests, does nothing. Any payload is ignored.

#### 0x01 / HELLO

| [Size] Type   | Description                                       |
| ------------- | ------------------------------------------------- |
| [1] u8        | Protocol version                                  |
| [1] u8        | Software major version (0 if unspecified)         |
| [1] u8        | Software minor version (0 if unspecified)         |
| [2] u16       | Software revision (0 if unspecified)              |
| [8] u64       | Timestamp (milliseconds since epoch)              |
| Identity      | Binary serialized sender identity                 |
| Endpoint      | Physical endpoint to which HELLO was sent         |
| [12] [u8; 12] | 96-bit IV for CTR-encrypted section               |
| [6] [u8; 6]   | *(reserved)* (contains legacy compatibility data) |
| --            | -- START of AES-256-CTR encrypted section --      |
| [2] u16       | Length of encrypted Dictionary in bytes           |
| Dictionary    | Key/value dictionary containing additional fields |
| --            | -- END of AES-256-CTR encrypted section --        |
| [48] [u8; 48] | HMAC-SHA384 extended strength MAC                 |

HELLO establishes a full session with another peer and carries information such as protocol and software versions, the full identity of the peer, and ephemeral keys for forward secrecy. Without a HELLO exchange only limited communication with the most conservative assumptions is possible, and communication without a session may be completely removed in the future. (It's only allowed now for backward compatibility with ZeroTier 1.x, and must be disabled in FIPS mode.)

HELLO is sent without payload encryption. Poly1305 MAC alone is applied to the plaintext. The OK(HELLO) reply is sent with normal payload encryption.

Legacy Poly1305 MAC is always used when sending HELLO for backward compatiblity with version 1.4 and earlier, since we don't know the remote peer's version until the HELLO exchange is complete. Version 2.0 and newer add HMAC-SHA384 authentication to the HELLO exchange to provide much stronger (and FIPS compliant) authentication for session establishment.

HELLO and OK(HELLO) are always sent using the static secret (no forward secrecy), since the HELLO exchange is the mechanism by which ephemeral keys are negotiated. There is no information in HELLO or OK(HELLO) packets that would have significant impact if the static secret were compromised.

These messages contain a dictionary that is used for a variety of additional fields. Since HELLO is sent in the clear, this section is encrypted in HELLO packets with AES-256-CTR. The key for AES is derived from the static secret using KBKDF (label `H`), and a 96-bit random IV is included as well. The dictionary in OK(HELLO) doesn't need this treatment as OK is encrypted normally. Encrypting the dictionary is only a defense in depth measure. It would technically be safe to send it in the clear, but it can contain system and node information that it's prudent to avoid revealing.

OK(HELLO) response payload, which must be sent if the HELLO receipient wishes to communicate:

| Type(s)       | Description                                       |
| ------------- | ------------------------------------------------- |
| [8] u64       | HELLO timestamp (echoed to determine latency)     |
| [1] u8        | Responding node protocol version                  |
| [1] u8        | Responding node major version (0 if unspecified)  |
| [1] u8        | Responding node minor version (0 if unspecified)  |
| [2] u16       | Responding node revision (0 if unspecified)       |
| Endpoint      | Physical endpoint where OK(HELLO) was sent        |
| [2] u16       | *(reserved)* (set to zero for legacy reasons)     |
| [2] u16       | Length of encrypted Dictionary in bytes           |
| Dictionary    | Key/value dictionary containing additional fields |
| [48] [u8; 48] | HMAC-SHA384 extended strength MAC                 |

HMAC-SHA384 authentication is computed over the payload of HELLO and OK(HELLO). For HELLO it is computed after AES-256-CTR encryption is applied to the dictionary section. and is checked before anything is done with a payload. For OK(HELLO) it is computed prior to normal packet armoring and is itself included in the encrypted payload.

Recommended dictionary fields in both HELLO and OK(HELLO):

| Name                 | Key  | Type         | Description                                      |
| -------------------- | ---  | ------------ | ------------------------------------------------ |
| INSTANCE_ID          | `I`  | u64          | Random integer generated at node startup         |
| CLOCK                | `C`  | u64          | Clock at sending node (milliseconds since epoch) |
| LOCATOR              | `L`  | Locator      | Signed locator for sending node                  |
| EPHEMERAL_C25519     | `E0` | [u8; 32]     | Curve25519 ECDH public key                       | 
| EPHEMERAL_P521       | `E1` | [u8; 132]    | NIST P-521 ECDH public key                       |

Dictionary fields that are only meaningful in OK(HELLO):

| Name                 | Key  | Type         | Description                                      |
| -------------------- | ---  | ------------ | ------------------------------------------------ |
| EPHEMERAL_ACK        | `e`  | [u8; 48]     | SHA384(KBKDF(shared secret, label: `A`))         |
| HELLO_ORIGIN         | `@`  | Endpoint     | Endpoint from which HELLO was received           |

Optional dictionary fields that can be included in either HELLO or OK(HELLO):

| Name                 | Key  | Type         | Description                                                  |
| -------------------- | ---  | ------------ | ------------------------------------------------------------ |
| SYS_ARCH             | `Sa` | string       | Host architecture (e.g. x86_64, aarch64)                     |
| SYS_BITS             | `Sb` | u64          | sizeof(pointer), e.g. 32 or 64                               |
| OS_NAME              | `On` | string       | Name of host operating system                                |
| OS_VERSION           | `Ov` | string       | Operating system version                                     |
| VENDOR               | `V`  | string       | Node software vendor if not ZeroTier, Inc.                   |
| FLAGS                | `+`  | string       | Flags (see below)                                            |

FLAGS is a string that can contain the following boolean flags: `F` to indicate that the node is running in FIPS compliant mode, and `w` to indicate that the node is a "wimp." "Wimpy" nodes are things like mobile phones, and this flag can be used to exempt these devices from selection for any intensive role (such as use in VL2 to propagate multicasts).

#### 0x02 / ERROR

| [Size] Type   | Description                                       |
| ------------- | ------------------------------------------------- |
| [1] u8        | Verb (message type) that generated the error      |
| [8] [u8; 8]   | Packet ID of packet that generated the error      |
| [1] u8        | Error code                                        |
| ...           | Error code dependent payload (if any)             |

If an error is unrelated to any specific packet, both the verb and packet ID field will be zero.

#### 0x03 / OK

| [Size] Type   | Description                                       |
| ------------- | ------------------------------------------------- |
| [1] u8        | Verb (message type) that generated the response   |
| [8] [u8; 8]   | Packet ID of packet that generated the response   |
| ...           | Response-specific payload (based on verb)         |

#### 0x04 / WHOIS

| [Size] Type   | Description                                       |
| ------------- | ------------------------------------------------- |
| ...           | One or more 5-byte ZeroTier addresses to look up  |

OK(WHOIS) response payload:

| [Size] Type   | Description                                       |
| ------------- | ------------------------------------------------- |
| Identity      | Identity of address                               |
| [1] bool      | If non-zero, a locator is included                |
| Locator       | Locator associated with node (if any)             |
| ...           | Additional tuples of identity, [locator]          |

#### 0x05 / RENDEZVOUS

| [Size] Type   | Description                                       |
| ------------- | ------------------------------------------------- |
| [1] u8        | Flags, currently unused and always 0              |
| [5] Address   | ZeroTier address of counter-party (i.e. not you)  |
| [2] u16       | 16-bit IP port                                    |
| [1] u8        | Length of IP address, 4 or 16                     |
| ...           | IP address bytes                                  |

RENDEZVOUS is only sent by roots, and is sent to help peers engage in time-synchronized UDP hole punching. Since it's only used for that purpose the endpoint type and protocol are assumed to be IP/UDP. (This message also predates the Endpoint type and canonical InetAddress serialized formats, but there's no need to change it as it is only needed for IP/UDP.)

When received from a trusted root and if the node wishes to communicate with the counter-party, it should send a HELLO to the indicated IP and port. Since the root will send both sides a RENDEZVOUS packet at the same time, it's likely that the two HELLOs sent by the two sides will "pass in the air." This assists with UDP hole punching with some routers.

RENDEZVOUS should be ignored if it does not come from a trusted root. It does not generate an OK response.

#### 0x08 / ECHO

Echo payload is arbitrary and may be (but is not required to be) echoed back in an OK(ECHO) response.

#### 0x10 / PUSH_DIRECT_PATHS

| [Size] Type   | Description                                       |
| ------------- | ------------------------------------------------- |
| [2] u16       | Number of endpoints                               |
| ...           | One or more Endpoint object(s)                    |

PUSH_DIRECT_PATHS is sent by a peer to explicitly suggest to another peer physical endpoints where it may be reached. This is how peers learn of direct LAN connections, uPnP mappings, or other endpoints not handled by RENDEZVOUS. If the receiver wishes to communicate with the sender it may try one or more of these endpoints.

If the recipient peer is older than ZeroTier 2.0, the following alternate format is used:

| [Size] Type   | Description                                       |
| ------------- | ------------------------------------------------- |
| [2] u16       | Number of IPs                                     |
|               | -- One or more of the following record --         |
| [3] [u8; 3]   | Reserved bytes, always zero                       |
| [1] u8        | Address type (4 or 6)                             |
| [1] u8        | Address length in bytes                           |
| ...           | 6-byte IPv4 or 18-byte IPv6 IP/port address       |

Older versions used this older format, and any suggested endpoints are assumed to be IP/UDP of course.

No OK or ERROR response is generated.

#### 0x14 / USER_MESSAGE

## VL2: Virtual Layer 2 (Ethernet Virtualization with Secure Access Control)
