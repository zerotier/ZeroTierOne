/*
 * Copyright (c)2013-2021 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_PEER_HPP
#define ZT_PEER_HPP

#include "AES.hpp"
#include "Address.hpp"
#include "Constants.hpp"
#include "Containers.hpp"
#include "Context.hpp"
#include "Endpoint.hpp"
#include "Identity.hpp"
#include "InetAddress.hpp"
#include "Locator.hpp"
#include "Mutex.hpp"
#include "Node.hpp"
#include "Path.hpp"
#include "Protocol.hpp"
#include "SharedPtr.hpp"
#include "SymmetricKey.hpp"
#include "Utils.hpp"

#define ZT_PEER_MARSHAL_SIZE_MAX                                                                                       \
    (1 + ZT_ADDRESS_LENGTH + ZT_SYMMETRIC_KEY_SIZE + ZT_IDENTITY_MARSHAL_SIZE_MAX + 1 + ZT_LOCATOR_MARSHAL_SIZE_MAX    \
     + (2 * 4) + 2)

#define ZT_PEER_DEDUP_BUFFER_SIZE         1024
#define ZT_PEER_DEDUP_BUFFER_MASK         1023U
#define ZT_PEER_EPHEMERAL_KEY_BUFFER_SIZE 3
#define ZT_PEER_EPHEMERAL_KEY_COUNT_MAX   (ZT_PEER_EPHEMERAL_KEY_BUFFER_SIZE + 1)

namespace ZeroTier {

class Topology;

/**
 * Peer on P2P Network (virtual layer 1)
 */
class Peer {
    friend class SharedPtr<Peer>;
    friend class Topology;

  public:
    /**
     * Create an uninitialized peer
     *
     * New peers must be initialized via either init() or unmarshal() prior to
     * use or null pointer dereference may occur.
     */
    Peer();

    ~Peer();

    /**
     * Initialize peer with an identity
     *
     * @param peerIdentity The peer's identity
     * @return True if initialization was succcesful
     */
    bool init(const Context &ctx, const CallContext &cc, const Identity &peerIdentity);

    /**
     * @return This peer's ZT address (short for identity().address())
     */
    ZT_INLINE Address address() const noexcept { return m_id.address(); }

    /**
     * @return This peer's identity
     */
    ZT_INLINE const Identity &identity() const noexcept { return m_id; }

    /**
     * @return Current locator or NULL if no locator is known
     */
    ZT_INLINE const SharedPtr<const Locator> locator() const noexcept
    {
        RWMutex::RLock l(m_lock);
        return m_locator;
    }

    /**
     * Set or update peer locator
     *
     * This checks the locator's timestamp against the current locator and
     * replace it if newer.
     *
     * @param loc Locator update
     * @param verify If true, verify locator's signature and structure
     * @return New locator or previous if it was not replaced.
     */
    ZT_INLINE SharedPtr<const Locator> setLocator(const SharedPtr<const Locator> &loc, const bool verify) noexcept
    {
        RWMutex::Lock l(m_lock);
        if ((loc) && ((!m_locator) || (m_locator->revision() < loc->revision()))) {
            if ((!verify) || loc->verify(m_id))
                m_locator = loc;
        }
        return m_locator;
    }

    /**
     * Log receipt of an authenticated packet
     *
     * This is called by the decode pipe when a packet is proven to be authentic
     * and appears to be valid.
     *
     * @param path Path over which packet was received
     * @param hops ZeroTier (not IP) hops
     * @param packetId Packet ID
     * @param verb Packet verb
     * @param inReVerb In-reply verb for OK or ERROR verbs
     */
    void received(
        const Context &ctx, const CallContext &cc, const SharedPtr<Path> &path, unsigned int hops, uint64_t packetId,
        unsigned int payloadLength, Protocol::Verb verb, Protocol::Verb inReVerb);

    /**
     * Log sent data
     *
     * @param bytes Number of bytes written
     */
    ZT_INLINE void sent(const CallContext &cc, const unsigned int bytes) noexcept
    {
        m_lastSend.store(cc.ticks, std::memory_order_relaxed);
        m_outMeter.log(cc.ticks, bytes);
    }

    /**
     * Called when traffic destined for a different peer is sent to this one
     *
     * @param bytes Number of bytes relayed
     */
    ZT_INLINE void relayed(const CallContext &cc, const unsigned int bytes) noexcept
    {
        m_relayedMeter.log(cc.ticks, bytes);
    }

    /**
     * Get the current best direct path or NULL if none
     *
     * @return Current best path or NULL if there is no direct path
     */
    ZT_INLINE SharedPtr<Path> path(const CallContext &cc) noexcept
    {
        return SharedPtr<Path>(reinterpret_cast<Path *>(m_bestPath.load(std::memory_order_acquire)));
    }

    /**
     * Send data to this peer over a specific path only
     *
     * @param data Data to send
     * @param len Length in bytes
     * @param via Path over which to send data (may or may not be an already-learned path for this peer)
     */
    ZT_INLINE void send(
        const Context &ctx, const CallContext &cc, const void *data, unsigned int len,
        const SharedPtr<Path> &via) noexcept
    {
        via->send(ctx, cc, data, len);
        sent(cc, len);
    }

    /**
     * Send data to this peer over the best available path
     *
     * If there is a working direct path it will be used. Otherwise the data will be
     * sent via a root server.
     *
     * @param data Data to send
     * @param len Length in bytes
     */
    void send(const Context &ctx, const CallContext &cc, const void *data, unsigned int len) noexcept;

    /**
     * Do ping, probes, re-keying, and keepalive with this peer, as needed.
     */
    void pulse(const Context &ctx, const CallContext &cc);

    /**
     * Attempt to contact this peer at a given endpoint.
     *
     * The attempt doesn't happen immediately. It's added to a queue for the
     * next invocation of pulse().
     *
     * @param ep Endpoint to attempt to contact
     * @param tries Number of times to try (default: 1)
     */
    void contact(const Context &ctx, const CallContext &cc, const Endpoint &ep, int tries = 1);

    /**
     * Reset paths within a given IP scope and address family
     *
     * Resetting a path involves sending an ECHO to it and then deactivating
     * it until or unless it responds. This is done when we detect a change
     * to our external IP or another system change that might invalidate
     * many or all current paths.
     *
     * @param scope IP scope
     * @param inetAddressFamily Family e.g. AF_INET
     */
    void resetWithinScope(const Context &ctx, const CallContext &cc, InetAddress::IpScope scope, int inetAddressFamily);

    /**
     * @return Time of last receive of anything, whether direct or relayed
     */
    ZT_INLINE int64_t lastReceive() const noexcept { return m_lastReceive.load(std::memory_order_relaxed); }

    /**
     * @return Average latency of all direct paths or -1 if no direct paths or unknown
     */
    ZT_INLINE int latency() const noexcept
    {
        RWMutex::RLock l(m_lock);
        int ltot = 0;
        int lcnt = 0;
        for (unsigned int i = 0; i < m_alivePathCount; ++i) {
            int lat = m_paths[i]->latency();
            if (lat > 0) {
                ltot += lat;
                ++lcnt;
            }
        }
        return (ltot > 0) ? (lcnt / ltot) : -1;
    }

    /**
     * @return Cipher suite that should be used to communicate with this peer
     */
    ZT_INLINE uint8_t cipher() const noexcept
    {
        // if (m_vProto >= 11)
        //	return ZT_PROTO_CIPHER_SUITE__AES_GMAC_SIV;
        return ZT_PROTO_CIPHER_POLY1305_SALSA2012;
    }

    /**
     * @return The permanent shared key for this peer computed by simple identity agreement
     */
    ZT_INLINE SymmetricKey &identityKey() noexcept { return m_identityKey; }

    /**
     * @return AES instance for HELLO dictionary / encrypted section encryption/decryption
     */
    ZT_INLINE const AES &identityHelloDictionaryEncryptionCipher() const noexcept { return m_helloCipher; }

    /**
     * @return Key for HMAC on HELLOs
     */
    ZT_INLINE const uint8_t *identityHelloHmacKey() const noexcept { return m_helloMacKey; }

    /**
     * @return Raw identity key bytes
     */
    ZT_INLINE const uint8_t *rawIdentityKey() const noexcept { return m_identityKey.key(); }

    /**
     * @return Current best key: either the latest ephemeral or the identity key
     */
    ZT_INLINE SymmetricKey &key() noexcept
    {
        return *reinterpret_cast<SymmetricKey *>(m_key.load(std::memory_order_relaxed));
    }

    /**
     * Get keys other than a key we have already tried.
     *
     * This is used when a packet arrives that doesn't decrypt with the preferred
     * key. It fills notYetTried[] with other keys that haven't been tried yet,
     * which can include the identity key and any older session keys.
     *
     * @param alreadyTried Key we've already tried or NULL if none
     * @param notYetTried All keys known (long lived or session) other than alreadyTried
     * @return Number of pointers written to notYetTried[]
     */
    ZT_INLINE int getOtherKeys(
        const SymmetricKey *const alreadyTried, SymmetricKey *notYetTried[ZT_PEER_EPHEMERAL_KEY_COUNT_MAX]) noexcept
    {
        RWMutex::RLock l(m_lock);
        int cnt = 0;
        if (alreadyTried != &m_identityKey)
            notYetTried[cnt++] = &m_identityKey;
        for (unsigned int k = 0; k < ZT_PEER_EPHEMERAL_KEY_BUFFER_SIZE; ++k) {
            SymmetricKey *const kk = &m_ephemeralSessions[k].key;
            if (m_ephemeralSessions[k].established && (alreadyTried != kk))
                notYetTried[cnt++] = kk;
        }
        return cnt;
    }

    /**
     * Set a flag ordering a key renegotiation ASAP.
     *
     * This can be called if there's any hint of an issue with the current key.
     * It's also called if any of the secondary possible keys returned by
     * getOtherKeys() decrypt a valid packet, indicating a desynchronization
     * in which key should be used.
     */
    ZT_INLINE void setKeyRenegotiationNeeded() noexcept
    {
        RWMutex::Lock l(m_lock);
        m_keyRenegotiationNeeded = true;
    }

    /**
     * Set the currently known remote version of this peer's client
     *
     * @param vproto Protocol version
     * @param vmaj Major version
     * @param vmin Minor version
     * @param vrev Revision
     */
    ZT_INLINE void
    setRemoteVersion(unsigned int vproto, unsigned int vmaj, unsigned int vmin, unsigned int vrev) noexcept
    {
        RWMutex::Lock l(m_lock);
        m_vProto    = (uint16_t)vproto;
        m_vMajor    = (uint16_t)vmaj;
        m_vMinor    = (uint16_t)vmin;
        m_vRevision = (uint16_t)vrev;
    }

    /**
     * Get the remote version of this peer.
     *
     * If false is returned, the value of the value-result variables is
     * undefined.
     *
     * @param vProto Set to protocol version
     * @param vMajor Set to major version
     * @param vMinor Set to minor version
     * @param vRevision Set to revision
     * @return True if remote version is known
     */
    ZT_INLINE bool remoteVersion(uint16_t &vProto, uint16_t &vMajor, uint16_t &vMinor, uint16_t &vRevision)
    {
        RWMutex::RLock l(m_lock);
        return (((vProto = m_vProto) | (vMajor = m_vMajor) | (vMinor = m_vMinor) | (vRevision = m_vRevision)) != 0);
    }

    /**
     * @return True if there is at least one alive direct path
     */
    ZT_INLINE bool directlyConnected() const noexcept
    {
        RWMutex::RLock l(m_lock);
        return m_alivePathCount > 0;
    }

    /**
     * Get all paths
     *
     * @param paths Vector of paths with the first path being the current preferred path
     */
    ZT_INLINE void getAllPaths(Vector<SharedPtr<Path>> &paths) const
    {
        RWMutex::RLock l(m_lock);
        paths.assign(m_paths, m_paths + m_alivePathCount);
    }

    /**
     * Save the latest version of this peer to the data store
     */
    void save(const Context &ctx, const CallContext &cc) const;

    static constexpr int marshalSizeMax() noexcept { return ZT_PEER_MARSHAL_SIZE_MAX; }

    int marshal(const Context &ctx, uint8_t data[ZT_PEER_MARSHAL_SIZE_MAX]) const noexcept;
    int unmarshal(const Context &ctx, int64_t ticks, const uint8_t *restrict data, int len) noexcept;

    /**
     * Rate limit gate for inbound WHOIS requests
     */
    ZT_INLINE bool rateGateInboundWhoisRequest(CallContext &cc) noexcept
    {
        if ((cc.ticks - m_lastWhoisRequestReceived.load(std::memory_order_relaxed)) >= ZT_PEER_WHOIS_RATE_LIMIT) {
            m_lastWhoisRequestReceived.store(cc.ticks, std::memory_order_relaxed);
            return true;
        }
        return false;
    }

    /**
     * Rate limit gate for inbound ECHO requests
     */
    ZT_INLINE bool rateGateEchoRequest(CallContext &cc) noexcept
    {
        if ((cc.ticks - m_lastEchoRequestReceived.load(std::memory_order_relaxed)) >= ZT_PEER_GENERAL_RATE_LIMIT) {
            m_lastEchoRequestReceived.store(cc.ticks, std::memory_order_relaxed);
            return true;
        }
        return false;
    }

    /**
     * Rate limit gate for inbound probes
     */
    ZT_INLINE bool rateGateProbeRequest(CallContext &cc) noexcept
    {
        if ((cc.ticks - m_lastProbeReceived.load(std::memory_order_relaxed)) > ZT_PEER_PROBE_RESPONSE_RATE_LIMIT) {
            m_lastProbeReceived.store(cc.ticks, std::memory_order_relaxed);
            return true;
        }
        return false;
    }

    /**
     * Packet deduplication filter for incoming packets
     *
     * This flags a packet ID and returns true if the same packet ID was already
     * flagged. This is done in an atomic operation if supported.
     *
     * @param packetId Packet ID to check/flag
     * @return True if this is a duplicate
     */
    ZT_INLINE bool deduplicateIncomingPacket(const uint64_t packetId) noexcept
    {
        return m_dedup[Utils::hash32((uint32_t)packetId) & ZT_PEER_DEDUP_BUFFER_MASK].exchange(
                   packetId, std::memory_order_relaxed)
               == packetId;
    }

  private:
    struct p_EphemeralPublic {
        uint8_t type;
        uint8_t c25519Public[ZT_C25519_ECDH_PUBLIC_KEY_SIZE];
        uint8_t p384Public[ZT_ECC384_PUBLIC_KEY_SIZE];
    };

    static_assert(
        sizeof(p_EphemeralPublic) == (1 + ZT_C25519_ECDH_PUBLIC_KEY_SIZE + ZT_ECC384_PUBLIC_KEY_SIZE),
        "p_EphemeralPublic has extra padding");

    struct p_EphemeralPrivate {
        ZT_INLINE p_EphemeralPrivate() noexcept : creationTime(-1) {}

        ZT_INLINE ~p_EphemeralPrivate() { Utils::burn(this, sizeof(p_EphemeralPublic)); }

        int64_t creationTime;
        uint64_t sha384OfPublic[6];
        p_EphemeralPublic pub;
        uint8_t c25519Private[ZT_C25519_ECDH_PRIVATE_KEY_SIZE];
        uint8_t p384Private[ZT_ECC384_PRIVATE_KEY_SIZE];
    };

    struct p_EphemeralSession {
        ZT_INLINE p_EphemeralSession() noexcept : established(false) {}

        uint64_t sha384OfPeerPublic[6];
        SymmetricKey key;
        bool established;
    };

    void m_prioritizePaths(const CallContext &cc);
    unsigned int m_sendProbe(
        const Context &ctx, const CallContext &cc, int64_t localSocket, const InetAddress &atAddress,
        const uint16_t *ports, unsigned int numPorts);
    void m_deriveSecondaryIdentityKeys() noexcept;
    unsigned int m_hello(
        const Context &ctx, const CallContext &cc, int64_t localSocket, const InetAddress &atAddress, bool forceNewKey);

    // Guards all fields except those otherwise indicated (and atomics of course).
    RWMutex m_lock;

    // Long lived key resulting from agreement with this peer's identity.
    SymmetricKey m_identityKey;

    // Cipher for encrypting or decrypting the encrypted section of HELLO packets.
    AES m_helloCipher;

    // Key for HELLO HMAC-SHA384
    uint8_t m_helloMacKey[ZT_SYMMETRIC_KEY_SIZE];

    // Keys we have generated and sent.
    p_EphemeralPrivate m_ephemeralKeysSent[ZT_PEER_EPHEMERAL_KEY_BUFFER_SIZE];

    // Sessions created when OK(HELLO) is received.
    p_EphemeralSession m_ephemeralSessions[ZT_PEER_EPHEMERAL_KEY_BUFFER_SIZE];

    // Pointer to active key (SymmetricKey).
    std::atomic<uintptr_t> m_key;

    // Flag indicating that we should rekey at next pulse().
    bool m_keyRenegotiationNeeded;

    // This peer's public identity.
    Identity m_id;

    // This peer's most recent (by revision) locator, or NULL if none on file.
    SharedPtr<const Locator> m_locator;

    // The last time something was received or sent.
    std::atomic<int64_t> m_lastReceive;
    std::atomic<int64_t> m_lastSend;

    // The last time we sent a full HELLO to this peer.
    int64_t m_lastSentHello;   // only checked while locked

    // The last time a WHOIS request was received from this peer (anti-DOS / anti-flood).
    std::atomic<int64_t> m_lastWhoisRequestReceived;

    // The last time an ECHO request was received from this peer (anti-DOS / anti-flood).
    std::atomic<int64_t> m_lastEchoRequestReceived;

    // The last time we got a probe from this peer.
    std::atomic<int64_t> m_lastProbeReceived;

    // Deduplication buffer.
    std::atomic<uint64_t> m_dedup[ZT_PEER_DEDUP_BUFFER_SIZE];

    // Meters measuring actual bandwidth in, out, and relayed via this peer (mostly if this is a root).
    Meter<> m_inMeter;
    Meter<> m_outMeter;
    Meter<> m_relayedMeter;

    // Direct paths sorted in descending order of preference.
    SharedPtr<Path> m_paths[ZT_MAX_PEER_NETWORK_PATHS];

    // Size of m_paths[] in non-NULL paths (max: MAX_PEER_NETWORK_PATHS).
    unsigned int m_alivePathCount;

    // Current best path (pointer to Path).
    std::atomic<uintptr_t> m_bestPath;

    // For SharedPtr<>
    std::atomic<int> __refCount;

    struct p_TryQueueItem {
        ZT_INLINE p_TryQueueItem() : target(), iteration(0) {}

        ZT_INLINE p_TryQueueItem(const Endpoint &t, int iter) : target(t), iteration(iter) {}

        Endpoint target;
        int iteration;
    };

    // Queue of endpoints to try.
    List<p_TryQueueItem> m_tryQueue;

    // Time each endpoint was last tried, for rate limiting.
    Map<Endpoint, int64_t> m_lastTried;

    // Version of remote peer, if known.
    uint16_t m_vProto;
    uint16_t m_vMajor;
    uint16_t m_vMinor;
    uint16_t m_vRevision;
};

}   // namespace ZeroTier

#endif
