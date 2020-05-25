/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2024-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_PEER_HPP
#define ZT_PEER_HPP

#include "Constants.hpp"
#include "RuntimeEnvironment.hpp"
#include "Node.hpp"
#include "Path.hpp"
#include "Address.hpp"
#include "Utils.hpp"
#include "Identity.hpp"
#include "InetAddress.hpp"
#include "SharedPtr.hpp"
#include "Mutex.hpp"
#include "Endpoint.hpp"
#include "Locator.hpp"
#include "Protocol.hpp"
#include "AES.hpp"
#include "EphemeralKey.hpp"
#include "SymmetricKey.hpp"
#include "Containers.hpp"

#define ZT_PEER_MARSHAL_SIZE_MAX (1 + ZT_ADDRESS_LENGTH + ZT_SYMMETRIC_KEY_SIZE + ZT_IDENTITY_MARSHAL_SIZE_MAX + ZT_LOCATOR_MARSHAL_SIZE_MAX + 1 + (ZT_MAX_PEER_NETWORK_PATHS * ZT_ENDPOINT_MARSHAL_SIZE_MAX) + (2*4) + 2)

#define ZT_PEER_DEDUP_BUFFER_SIZE 1024
#define ZT_PEER_DEDUP_BUFFER_MASK 1023U

namespace ZeroTier {

class Topology;

/**
 * Peer on P2P Network (virtual layer 1)
 */
class Peer
{
	friend class SharedPtr<Peer>;
	friend class Topology;

public:
	/**
	 * Create an uninitialized peer
	 *
	 * New peers must be initialized via either init() or unmarshal() prior to
	 * use or null pointer dereference may occur.
	 *
	 * @param renv Runtime environment
	 */
	explicit Peer(const RuntimeEnvironment *renv);

	~Peer();

	/**
	 * Initialize peer with an identity
	 *
	 * @param peerIdentity The peer's identity
	 * @return True if initialization was succcesful
	 */
	bool init(const Identity &peerIdentity);

	/**
	 * @return This peer's ZT address (short for identity().address())
	 */
	ZT_INLINE Address address() const noexcept { return m_id.address(); }

	/**
	 * @return This peer's identity
	 */
	ZT_INLINE const Identity &identity() const noexcept { return m_id; }

	/**
	 * @return Copy of current locator
	 */
	ZT_INLINE Locator locator() const noexcept
	{
		RWMutex::RLock l(m_lock);
		return m_locator;
	}

	/**
	 * Log receipt of an authenticated packet
	 *
	 * This is called by the decode pipe when a packet is proven to be authentic
	 * and appears to be valid.
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param path Path over which packet was received
	 * @param hops ZeroTier (not IP) hops
	 * @param packetId Packet ID
	 * @param verb Packet verb
	 * @param inReVerb In-reply verb for OK or ERROR verbs
	 */
	void received(
		void *tPtr,
		const SharedPtr<Path> &path,
		unsigned int hops,
		uint64_t packetId,
		unsigned int payloadLength,
		Protocol::Verb verb,
		Protocol::Verb inReVerb);

	/**
	 * Log sent data
	 *
	 * @param now Current time
	 * @param bytes Number of bytes written
	 */
	ZT_INLINE void sent(const int64_t now,const unsigned int bytes) noexcept
	{
		m_lastSend = now;
		m_outMeter.log(now, bytes);
	}

	/**
	 * Called when traffic destined for a different peer is sent to this one
	 *
	 * @param now Current time
	 * @param bytes Number of bytes relayed
	 */
	ZT_INLINE void relayed(const int64_t now,const unsigned int bytes) noexcept
	{
		m_relayedMeter.log(now, bytes);
	}

	/**
	 * Get the current best direct path or NULL if none
	 *
	 * @return Current best path or NULL if there is no direct path
	 */
	ZT_INLINE SharedPtr<Path> path(const int64_t now) noexcept
	{
		if (likely((now - m_lastPrioritizedPaths) < ZT_PEER_PRIORITIZE_PATHS_INTERVAL)) {
			RWMutex::RLock l(m_lock);
			if (m_alivePathCount > 0)
				return m_paths[0];
		} else {
			RWMutex::Lock l(m_lock);
			m_prioritizePaths(now);
			if (m_alivePathCount > 0)
				return m_paths[0];
		}
		return SharedPtr<Path>();
	}

	/**
	 * Send data to this peer over a specific path only
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param now Current time
	 * @param data Data to send
	 * @param len Length in bytes
	 * @param via Path over which to send data (may or may not be an already-learned path for this peer)
	 */
	ZT_INLINE void send(void *tPtr,int64_t now,const void *data,unsigned int len,const SharedPtr<Path> &via) noexcept
	{
		via->send(RR,tPtr,data,len,now);
		sent(now,len);
	}

	/**
	 * Send data to this peer over the best available path
	 *
	 * If there is a working direct path it will be used. Otherwise the data will be
	 * sent via a root server.
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param now Current time
	 * @param data Data to send
	 * @param len Length in bytes
	 */
	void send(void *tPtr,int64_t now,const void *data,unsigned int len) noexcept;

	/**
	 * Send a HELLO to this peer at a specified physical address.
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param localSocket Local source socket
	 * @param atAddress Destination address
	 * @param now Current time
	 * @return Number of bytes sent
	 */
	unsigned int hello(void *tPtr,int64_t localSocket,const InetAddress &atAddress,int64_t now);

	/**
	 * Ping this peer if needed and/or perform other periodic tasks.
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param now Current time
	 * @param isRoot True if this peer is a root
	 */
	void pulse(void *tPtr,int64_t now,bool isRoot);

	/**
	 * Attempt to contact this peer at a given endpoint.
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param now Current time
	 * @param ep Endpoint to attempt to contact
	 * @param bfg1024 Use BFG1024 brute force symmetric NAT busting algorithm if applicable
	 */
	void contact(void *tPtr,int64_t now,const Endpoint &ep,bool breakSymmetricBFG1024);

	/**
	 * Reset paths within a given IP scope and address family
	 *
	 * Resetting a path involves sending an ECHO to it and then deactivating
	 * it until or unless it responds. This is done when we detect a change
	 * to our external IP or another system change that might invalidate
	 * many or all current paths.
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param scope IP scope
	 * @param inetAddressFamily Family e.g. AF_INET
	 * @param now Current time
	 */
	void resetWithinScope(void *tPtr,InetAddress::IpScope scope,int inetAddressFamily,int64_t now);

	/**
	 * @return All currently memorized bootstrap endpoints
	 */
	ZT_INLINE FCV<Endpoint,ZT_MAX_PEER_NETWORK_PATHS> bootstrap() const noexcept
	{
		RWMutex::RLock l(m_lock);
		FCV<Endpoint,ZT_MAX_PEER_NETWORK_PATHS> r;
		for(SortedMap<Endpoint::Type,Endpoint>::const_iterator i(m_bootstrap.begin());i != m_bootstrap.end();++i) // NOLINT(hicpp-use-auto,modernize-use-auto,modernize-loop-convert)
			r.push_back(i->second);
		return r;
	}

	/**
	 * Set bootstrap endpoint
	 *
	 * @param ep Bootstrap endpoint
	 */
	ZT_INLINE void setBootstrap(const Endpoint &ep) noexcept
	{
		RWMutex::Lock l(m_lock);
		m_bootstrap[ep.type()] = ep;
	}

	/**
	 * @return Time of last receive of anything, whether direct or relayed
	 */
	ZT_INLINE int64_t lastReceive() const noexcept { return m_lastReceive; }

	/**
	 * @return Average latency of all direct paths or -1 if no direct paths or unknown
	 */
	ZT_INLINE int latency() const noexcept
	{
		int ltot = 0;
		int lcnt = 0;
		RWMutex::RLock l(m_lock);
		for(unsigned int i=0;i < m_alivePathCount;++i) {
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
		//if (m_vProto >= 11)
		//	return ZT_PROTO_CIPHER_SUITE__AES_GMAC_SIV;
		return ZT_PROTO_CIPHER_SUITE__POLY1305_SALSA2012;
	}

	/**
	 * @return The permanent shared key for this peer computed by simple identity agreement
	 */
	ZT_INLINE SharedPtr<SymmetricKey> identityKey() noexcept
	{
		return m_identityKey;
	}

	/**
	 * @return AES instance for HELLO dictionary / encrypted section encryption/decryption
	 */
	ZT_INLINE const AES &identityHelloDictionaryEncryptionCipher() noexcept
	{
		return m_helloCipher;
	}

	/**
	 * @return Key for HMAC on HELLOs
	 */
	ZT_INLINE const uint8_t *identityHelloHmacKey() noexcept
	{
		return m_helloMacKey;
	}

	/**
	 * @return Raw identity key bytes
	 */
	ZT_INLINE const uint8_t *rawIdentityKey() noexcept
	{
		RWMutex::RLock l(m_lock);
		return m_identityKey->secret;
	}

	/**
	 * @return Current best key: either the latest ephemeral or the identity key
	 */
	ZT_INLINE SharedPtr<SymmetricKey> key() noexcept
	{
		RWMutex::RLock l(m_lock);
		return m_key();
	}

	/**
	 * Check whether a key is ephemeral
	 *
	 * This is used to check whether a packet is received with forward secrecy enabled
	 * or not.
	 *
	 * @param k Key to check
	 * @return True if this key is ephemeral, false if it's the long-lived identity key
	 */
	ZT_INLINE bool isEphemeral(const SharedPtr<SymmetricKey> &k) const noexcept
	{
		return (m_identityKey != k);
	}

	/**
	 * Set the currently known remote version of this peer's client
	 *
	 * @param vproto Protocol version
	 * @param vmaj Major version
	 * @param vmin Minor version
	 * @param vrev Revision
	 */
	ZT_INLINE void setRemoteVersion(unsigned int vproto,unsigned int vmaj,unsigned int vmin,unsigned int vrev) noexcept
	{
		m_vProto = (uint16_t)vproto;
		m_vMajor = (uint16_t)vmaj;
		m_vMinor = (uint16_t)vmin;
		m_vRevision = (uint16_t)vrev;
	}

	ZT_INLINE unsigned int remoteVersionProtocol() const noexcept { return m_vProto; }
	ZT_INLINE unsigned int remoteVersionMajor() const noexcept { return m_vMajor; }
	ZT_INLINE unsigned int remoteVersionMinor() const noexcept { return m_vMinor; }
	ZT_INLINE unsigned int remoteVersionRevision() const noexcept { return m_vRevision; }
	ZT_INLINE bool remoteVersionKnown() const noexcept { return ((m_vMajor > 0) || (m_vMinor > 0) || (m_vRevision > 0)); }

	/**
	 * @return True if there is at least one alive direct path
	 */
	bool directlyConnected(int64_t now);

	/**
	 * Get all paths
	 *
	 * @param paths Vector of paths with the first path being the current preferred path
	 */
	void getAllPaths(Vector< SharedPtr<Path> > &paths);

	/**
	 * Save the latest version of this peer to the data store
	 */
	void save(void *tPtr) const;

	// NOTE: peer marshal/unmarshal only saves/restores the identity, locator, most
	// recent bootstrap address, and version information.
	static constexpr int marshalSizeMax() noexcept { return ZT_PEER_MARSHAL_SIZE_MAX; }
	int marshal(uint8_t data[ZT_PEER_MARSHAL_SIZE_MAX]) const noexcept;
	int unmarshal(const uint8_t *restrict data,int len) noexcept;

	/**
	 * Rate limit gate for inbound WHOIS requests
	 */
	ZT_INLINE bool rateGateInboundWhoisRequest(const int64_t now) noexcept
	{
		if ((now - m_lastWhoisRequestReceived) >= ZT_PEER_WHOIS_RATE_LIMIT) {
			m_lastWhoisRequestReceived = now;
			return true;
		}
		return false;
	}

	/**
	 * Rate limit gate for inbound ECHO requests
	 */
	ZT_INLINE bool rateGateEchoRequest(const int64_t now) noexcept
	{
		if ((now - m_lastEchoRequestReceived) >= ZT_PEER_GENERAL_RATE_LIMIT) {
			m_lastEchoRequestReceived = now;
			return true;
		}
		return false;
	}

	/**
	 * Rate limit gate for inbound probes
	 */
	ZT_INLINE bool rateGateProbeRequest(const int64_t now) noexcept
	{
		if((now - m_lastProbeReceived) > ZT_PEER_PROBE_RESPONSE_RATE_LIMIT) {
			m_lastProbeReceived = now;
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
		// TODO: should take instance ID into account too, but this isn't fully wired.
		return m_dedup[Utils::hash32((uint32_t)packetId) & ZT_PEER_DEDUP_BUFFER_MASK].exchange(packetId) == packetId;
	}

private:
	void m_prioritizePaths(int64_t now);
	unsigned int m_sendProbe(void *tPtr,int64_t localSocket,const InetAddress &atAddress,const uint16_t *ports,unsigned int numPorts,int64_t now);
	void m_deriveSecondaryIdentityKeys() noexcept;

	ZT_INLINE SharedPtr<SymmetricKey> m_key() noexcept
	{
		// assumes m_lock is locked (for read at least)
		return (m_ephemeralKeys[0]) ? m_ephemeralKeys[0] : m_identityKey;
	}

	const RuntimeEnvironment *RR;

	// Read/write mutex for non-atomic non-const fields.
	RWMutex m_lock;

	// Static identity key
	SharedPtr<SymmetricKey> m_identityKey;

	// Cipher for encrypting or decrypting the encrypted section of HELLO packets.
	AES m_helloCipher;

	// Key for HELLO HMAC-SHA384
	uint8_t m_helloMacKey[ZT_SYMMETRIC_KEY_SIZE];

	// Currently active ephemeral public key pair
	EphemeralKey m_ephemeralPair;
	int64_t m_ephemeralPairTimestamp;

	// Current and previous ephemeral key
	SharedPtr<SymmetricKey> m_ephemeralKeys[2];

	Identity m_id;
	Locator m_locator;

	// the last time something was sent or received from this peer (direct or indirect).
	std::atomic<int64_t> m_lastReceive;
	std::atomic<int64_t> m_lastSend;

	// The last time we sent a full HELLO to this peer.
	int64_t m_lastSentHello; // only checked while locked

	// The last time a WHOIS request was received from this peer (anti-DOS / anti-flood).
	std::atomic<int64_t> m_lastWhoisRequestReceived;

	// The last time an ECHO request was received from this peer (anti-DOS / anti-flood).
	std::atomic<int64_t> m_lastEchoRequestReceived;

	// The last time we sorted paths in order of preference. (This happens pretty often.)
	std::atomic<int64_t> m_lastPrioritizedPaths;

	// The last time we got a probe from this peer.
	std::atomic<int64_t> m_lastProbeReceived;

	// Deduplication buffer
	std::atomic<uint64_t> m_dedup[ZT_PEER_DEDUP_BUFFER_SIZE];

	// Meters measuring actual bandwidth in, out, and relayed via this peer (mostly if this is a root).
	Meter<> m_inMeter;
	Meter<> m_outMeter;
	Meter<> m_relayedMeter;

	// Direct paths sorted in descending order of preference.
	SharedPtr<Path> m_paths[ZT_MAX_PEER_NETWORK_PATHS];

	// For SharedPtr<>
	std::atomic<int> __refCount;

	// Number of paths current alive (number of non-NULL entries in _paths).
	unsigned int m_alivePathCount;

	// Remembered addresses by endpoint type (std::map is smaller for only a few keys).
	SortedMap<Endpoint::Type,Endpoint> m_bootstrap;

	// Addresses recieved via PUSH_DIRECT_PATHS etc. that we are scheduled to try.
	struct p_TryQueueItem
	{
		ZT_INLINE p_TryQueueItem() : target(), ts(0), breakSymmetricBFG1024(false) {}
		ZT_INLINE p_TryQueueItem(const int64_t now, const Endpoint &t, const bool bfg) : target(t), ts(now), breakSymmetricBFG1024(bfg) {}
		Endpoint target;
		int64_t ts;
		bool breakSymmetricBFG1024;
	};
	List<p_TryQueueItem> m_tryQueue;
	List<p_TryQueueItem>::iterator m_tryQueuePtr; // loops over _tryQueue like a circular buffer

	uint16_t m_vProto;
	uint16_t m_vMajor;
	uint16_t m_vMinor;
	uint16_t m_vRevision;
};

} // namespace ZeroTier

#endif
