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
#include "Hashtable.hpp"
#include "Mutex.hpp"
#include "Endpoint.hpp"
#include "Locator.hpp"
#include "Protocol.hpp"

#include <vector>
#include <list>

// version, identity, locator, bootstrap, version info, length of any additional fields
#define ZT_PEER_MARSHAL_SIZE_MAX (1 + ZT_IDENTITY_MARSHAL_SIZE_MAX + ZT_LOCATOR_MARSHAL_SIZE_MAX + ZT_INETADDRESS_MARSHAL_SIZE_MAX + (2*4) + 2)

namespace ZeroTier {

class Topology;

/**
 * Peer on P2P Network (virtual layer 1)
 */
class Peer
{
	friend class SharedPtr<Peer>;
	friend class Topology;

private:
	ZT_ALWAYS_INLINE Peer() {}

public:
	/**
	 * Create an uninitialized peer
	 *
	 * The peer will need to be initialized with init() or unmarshal() before
	 * it can be used.
	 *
	 * @param renv Runtime environment
	 */
	explicit Peer(const RuntimeEnvironment *renv);

	ZT_ALWAYS_INLINE ~Peer() { Utils::burn(_key,sizeof(_key)); }

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
	ZT_ALWAYS_INLINE const Address &address() const noexcept { return _id.address(); }

	/**
	 * @return This peer's identity
	 */
	ZT_ALWAYS_INLINE const Identity &identity() const noexcept { return _id; }

	/**
	 * @return Copy of current locator
	 */
	ZT_ALWAYS_INLINE Locator locator() const noexcept
	{
		RWMutex::RLock l(_lock);
		return _locator;
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
	 * Send a HELLO to this peer at a specified physical address
	 *
	 * No statistics or sent times are updated here.
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param localSocket Local source socket
	 * @param atAddress Destination address
	 * @param now Current time
	 */
	void sendHELLO(void *tPtr,int64_t localSocket,const InetAddress &atAddress,int64_t now);

	/**
	 * Send a NOP message to e.g. probe a new link
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param localSocket Local source socket
	 * @param atAddress Destination address
	 * @param now Current time
	 */
	void sendNOP(void *tPtr,int64_t localSocket,const InetAddress &atAddress,int64_t now);

	/**
	 * Send ping to this peer
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param now Current time
	 * @param pingAllAddressTypes If true, try to keep a link up for each address type/family
	 */
	void ping(void *tPtr,int64_t now,bool pingAllAddressTypes);

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
	 * Update peer latency information
	 *
	 * This is called from packet parsing code.
	 *
	 * @param l New latency measurment (in milliseconds)
	 */
	void updateLatency(unsigned int l) noexcept;

	/**
	 * @return Bootstrap address or NULL if none
	 */
	ZT_ALWAYS_INLINE const Endpoint &bootstrap() const noexcept
	{
		RWMutex::RLock l(_lock);
		return _bootstrap;
	}

	/**
	 * Set bootstrap endpoint
	 *
	 * @param ep Bootstrap endpoint
	 */
	ZT_ALWAYS_INLINE void setBootstrap(const Endpoint &ep) noexcept
	{
		RWMutex::Lock l(_lock);
		_bootstrap = ep;
	}

	/**
	 * @return Time of last receive of anything, whether direct or relayed
	 */
	ZT_ALWAYS_INLINE int64_t lastReceive() const noexcept { return _lastReceive; }

	/**
	 * @return True if we've heard from this peer in less than ZT_PEER_ALIVE_TIMEOUT
	 */
	ZT_ALWAYS_INLINE bool alive(const int64_t now) const noexcept { return ((now - _lastReceive) < ZT_PEER_ALIVE_TIMEOUT); }

	/**
	 * @return True if we've heard from this peer in less than ZT_PEER_ACTIVITY_TIMEOUT
	 */
	ZT_ALWAYS_INLINE bool active(const int64_t now) const noexcept { return ((now - _lastReceive) < ZT_PEER_ACTIVITY_TIMEOUT); }

	/**
	 * @return Latency in milliseconds of best/aggregate path or 0xffff if unknown
	 */
	ZT_ALWAYS_INLINE unsigned int latency() const noexcept { return _latency; }

	/**
	 * @return 256-bit secret symmetric encryption key
	 */
	ZT_ALWAYS_INLINE const unsigned char *key() const noexcept { return _key; }

	/**
	 * @return Preferred cipher suite for normal encrypted P2P communication
	 */
	ZT_ALWAYS_INLINE uint8_t cipher() const noexcept
	{
		return ZT_PROTO_CIPHER_SUITE__POLY1305_SALSA2012;
	}

	/**
	 * @return Incoming probe packet (in big-endian byte order)
0	 */
	ZT_ALWAYS_INLINE uint64_t incomingProbe() const noexcept { return _incomingProbe; }

	/**
	 * Set the currently known remote version of this peer's client
	 *
	 * @param vproto Protocol version
	 * @param vmaj Major version
	 * @param vmin Minor version
	 * @param vrev Revision
	 */
	ZT_ALWAYS_INLINE void setRemoteVersion(unsigned int vproto,unsigned int vmaj,unsigned int vmin,unsigned int vrev) noexcept
	{
		_vProto = (uint16_t)vproto;
		_vMajor = (uint16_t)vmaj;
		_vMinor = (uint16_t)vmin;
		_vRevision = (uint16_t)vrev;
	}

	ZT_ALWAYS_INLINE unsigned int remoteVersionProtocol() const noexcept { return _vProto; }
	ZT_ALWAYS_INLINE unsigned int remoteVersionMajor() const noexcept { return _vMajor; }
	ZT_ALWAYS_INLINE unsigned int remoteVersionMinor() const noexcept { return _vMinor; }
	ZT_ALWAYS_INLINE unsigned int remoteVersionRevision() const noexcept { return _vRevision; }
	ZT_ALWAYS_INLINE bool remoteVersionKnown() const noexcept { return ((_vMajor > 0)||(_vMinor > 0)||(_vRevision > 0)); }

	/**
	 * Rate limit gate for inbound WHOIS requests
	 */
	ZT_ALWAYS_INLINE bool rateGateInboundWhoisRequest(const int64_t now) noexcept
	{
		if ((now - _lastWhoisRequestReceived) >= ZT_PEER_WHOIS_RATE_LIMIT) {
			_lastWhoisRequestReceived = now;
			return true;
		}
		return false;
	}

	/**
	 * Rate limit gate for inbound PUSH_DIRECT_PATHS requests
	 */
	ZT_ALWAYS_INLINE bool rateGateInboundPushDirectPaths(const int64_t now) noexcept
	{
		if ((now - _lastPushDirectPathsReceived) >= ZT_DIRECT_PATH_PUSH_INTERVAL) {
			_lastPushDirectPathsReceived = now;
			return true;
		}
		return false;
	}

	/**
	 * Rate limit attempts in response to incoming short probe packets
	 */
	ZT_ALWAYS_INLINE bool rateGateInboundProbe(const int64_t now) noexcept
	{
		if ((now - _lastProbeReceived) >= ZT_DIRECT_PATH_PUSH_INTERVAL) {
			_lastProbeReceived = now;
			return true;
		}
		return false;
	}

	/**
	 * Rate limit gate for inbound ECHO requests
	 */
	ZT_ALWAYS_INLINE bool rateGateEchoRequest(const int64_t now) noexcept
	{
		if ((now - _lastEchoRequestReceived) >= ZT_PEER_GENERAL_RATE_LIMIT) {
			_lastEchoRequestReceived = now;
			return true;
		}
		return false;
	}

	/**
	 * @return Current best path
	 */
	SharedPtr<Path> path(int64_t now);

	/**
	 * @return True if there is at least one alive direct path
	 */
	bool direct(int64_t now);

	/**
	 * Get all paths
	 *
	 * @param paths Vector of paths with the first path being the current preferred path
	 */
	void getAllPaths(std::vector< SharedPtr<Path> > &paths);

	/**
	 * Save the latest version of this peer to the data store
	 */
	void save(void *tPtr) const;

	/**
	 * Attempt to contact this peer at a physical address, subject to internal checks
	 *
	 * @param tPtr External user pointer we pass around
	 * @param ep Endpoint to attempt to contact
	 * @param now Current time
	 * @param bfg1024 Use BFG1024 brute force symmetric NAT busting algorithm if applicable
	 */
	void contact(void *tPtr,const Endpoint &ep,int64_t now,bool bfg1024);

	/**
	 * Called by Node when an alarm set by this peer goes off
	 *
	 * @param tPtr External user pointer we pass around
	 * @param now Current time
	 */
	void alarm(void *tPtr,int64_t now);

	// NOTE: peer marshal/unmarshal only saves/restores the identity, locator, most
	// recent bootstrap address, and version information.
	static constexpr int marshalSizeMax() noexcept { return ZT_PEER_MARSHAL_SIZE_MAX; }
	int marshal(uint8_t data[ZT_PEER_MARSHAL_SIZE_MAX]) const noexcept;
	int unmarshal(const uint8_t *restrict data,int len) noexcept;

private:
	void _prioritizePaths(int64_t now);

	uint8_t _key[ZT_PEER_SECRET_KEY_LENGTH];

	const RuntimeEnvironment *RR;

	// The last time various things happened, for rate limiting and periodic events.
	std::atomic<int64_t> _lastReceive;
	std::atomic<int64_t> _lastWhoisRequestReceived;
	std::atomic<int64_t> _lastEchoRequestReceived;
	std::atomic<int64_t> _lastPushDirectPathsReceived;
	std::atomic<int64_t> _lastProbeReceived;
	std::atomic<int64_t> _lastAttemptedP2PInit;
	std::atomic<int64_t> _lastTriedStaticPath;
	std::atomic<int64_t> _lastPrioritizedPaths;
	std::atomic<int64_t> _lastAttemptedAggressiveNATTraversal;

	// Latency in milliseconds
	std::atomic<unsigned int> _latency;

	// For SharedPtr<>
	std::atomic<int> __refCount;

	// Read/write mutex for non-atomic non-const fields.
	RWMutex _lock;

	// Number of paths current alive as of last _prioritizePaths
	unsigned int _alivePathCount;

	// Direct paths sorted in descending order of preference (can be NULL, if first is NULL there's no direct path)
	SharedPtr<Path> _paths[ZT_MAX_PEER_NETWORK_PATHS];

	// Queue of batches of one or more physical addresses to try at some point in the future (for NAT traversal logic)
	struct _ContactQueueItem
	{
		ZT_ALWAYS_INLINE _ContactQueueItem() {}
		ZT_ALWAYS_INLINE _ContactQueueItem(const InetAddress &a,const uint16_t *pstart,const uint16_t *pend,const unsigned int apt) :
			address(a),
			ports(pstart,pend),
			alivePathThreshold(apt) {}
		ZT_ALWAYS_INLINE _ContactQueueItem(const InetAddress &a,const unsigned int apt) :
			address(a),
			ports(),
			alivePathThreshold(apt) {}
		InetAddress address;
		std::vector<uint16_t> ports; // if non-empty try these ports, otherwise use the one in address
		unsigned int alivePathThreshold; // skip and forget if alive path count is >= this
	};
	std::list<_ContactQueueItem> _contactQueue;

	Identity _id;
	uint64_t _incomingProbe;
	Locator _locator;
	Endpoint _bootstrap; // right now only InetAddress endpoints are supported for bootstrap

	uint16_t _vProto;
	uint16_t _vMajor;
	uint16_t _vMinor;
	uint16_t _vRevision;
};

} // namespace ZeroTier

#endif
