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
#include "SymmetricKey.hpp"
#include "Containers.hpp"

// version, identity, locator, bootstrap, version info, length of any additional fields
#define ZT_PEER_MARSHAL_SIZE_MAX (1 + ZT_SYMMETRICKEY_MARSHAL_SIZE_MAX + ZT_IDENTITY_MARSHAL_SIZE_MAX + ZT_LOCATOR_MARSHAL_SIZE_MAX + 1 + (ZT_MAX_PEER_NETWORK_PATHS * ZT_ENDPOINT_MARSHAL_SIZE_MAX) + (2*4) + 2)

#define ZT_PEER_BFG1024_PORT_SCAN_CHUNK_SIZE 128

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
	 * The peer will need to be initialized with init() or unmarshal() before
	 * it can be used.
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
	ZT_INLINE Address address() const noexcept { return _id.address(); }

	/**
	 * @return This peer's identity
	 */
	ZT_INLINE const Identity &identity() const noexcept { return _id; }

	/**
	 * @return Copy of current locator
	 */
	ZT_INLINE Locator locator() const noexcept
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
	 * Log sent data
	 *
	 * @param now Current time
	 * @param bytes Number of bytes written
	 */
	ZT_INLINE void sent(const int64_t now,const unsigned int bytes) noexcept
	{
		_lastSend = now;
		_outMeter.log(now,bytes);
	}

	/**
	 * Called when traffic destined for a different peer is sent to this one
	 *
	 * @param now Current time
	 * @param bytes Number of bytes relayed
	 */
	ZT_INLINE void relayed(const int64_t now,const unsigned int bytes) noexcept
	{
		_relayedMeter.log(now,bytes);
	}

	/**
	 * Get the current best direct path or NULL if none
	 *
	 * @return Current best path or NULL if there is no direct path
	 */
	ZT_INLINE SharedPtr<Path> path(const int64_t now) noexcept
	{
		if ((now - _lastPrioritizedPaths) > ZT_PEER_PRIORITIZE_PATHS_INTERVAL) {
			RWMutex::Lock l(_lock);
			_prioritizePaths(now);
			if (_alivePathCount > 0)
				return _paths[0];
		} else {
			RWMutex::RLock l(_lock);
			if (_alivePathCount > 0)
				return _paths[0];
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
	void send(void *tPtr,int64_t now,const void *data,unsigned int len,const SharedPtr<Path> &via) noexcept;

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
	 * Send a NOP message to e.g. probe a new link
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param localSocket Local source socket
	 * @param atAddress Destination address
	 * @param now Current time
	 * @return Number of bytes sent
	 */
	unsigned int sendNOP(void *tPtr,int64_t localSocket,const InetAddress &atAddress,int64_t now);

	/**
	 * Ping this peer if needed and/or perform other periodic tasks.
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param now Current time
	 * @param isRoot True if this peer is a root
	 */
	void pulse(void *tPtr,int64_t now,bool isRoot);

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
		FCV<Endpoint,ZT_MAX_PEER_NETWORK_PATHS> r;
		for(std::map< Endpoint::Type,Endpoint,std::less<Endpoint::Type>,Utils::Mallocator< std::pair<const Endpoint::Type,Endpoint> > >::const_iterator i(_bootstrap.begin());i!=_bootstrap.end();++i) // NOLINT(hicpp-use-auto,modernize-use-auto,modernize-loop-convert)
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
		RWMutex::Lock l(_lock);
		_bootstrap[ep.type()] = ep;
	}

	/**
	 * @return Time of last receive of anything, whether direct or relayed
	 */
	ZT_INLINE int64_t lastReceive() const noexcept { return _lastReceive; }

	/**
	 * @return Average latency of all direct paths or -1 if no direct paths or unknown
	 */
	ZT_INLINE int latency() const noexcept
	{
		int ltot = 0;
		int lcnt = 0;
		RWMutex::RLock l(_lock);
		for(unsigned int i=0;i<_alivePathCount;++i) {
			int lat = _paths[i]->latency();
			if (lat > 0) {
				ltot += lat;
				++lcnt;
			}
		}
		return (ltot > 0) ? (lcnt / ltot) : -1;
	}

	/**
	 * @return Preferred cipher suite for normal encrypted P2P communication
	 */
	ZT_INLINE uint8_t cipher() const noexcept
	{
		return ZT_PROTO_CIPHER_SUITE__POLY1305_SALSA2012;
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
		_vProto = (uint16_t)vproto;
		_vMajor = (uint16_t)vmaj;
		_vMinor = (uint16_t)vmin;
		_vRevision = (uint16_t)vrev;
	}

	ZT_INLINE unsigned int remoteVersionProtocol() const noexcept { return _vProto; }
	ZT_INLINE unsigned int remoteVersionMajor() const noexcept { return _vMajor; }
	ZT_INLINE unsigned int remoteVersionMinor() const noexcept { return _vMinor; }
	ZT_INLINE unsigned int remoteVersionRevision() const noexcept { return _vRevision; }
	ZT_INLINE bool remoteVersionKnown() const noexcept { return ((_vMajor > 0) || (_vMinor > 0) || (_vRevision > 0)); }

	/**
	 * @return True if there is at least one alive direct path
	 */
	bool directlyConnected(int64_t now);

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
	void tryToContactAt(void *tPtr,const Endpoint &ep,int64_t now,bool bfg1024);

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

	/**
	 * Rate limit gate for inbound WHOIS requests
	 */
	ZT_INLINE bool rateGateInboundWhoisRequest(const int64_t now) noexcept
	{
		if ((now - _lastWhoisRequestReceived) >= ZT_PEER_WHOIS_RATE_LIMIT) {
			_lastWhoisRequestReceived = now;
			return true;
		}
		return false;
	}

	/**
	 * Rate limit attempts in response to incoming short probe packets
	 */
	ZT_INLINE bool rateGateInboundProbe(const int64_t now) noexcept
	{
		if ((now - _lastProbeReceived) >= ZT_DIRECT_CONNECT_ATTEMPT_INTERVAL) {
			_lastProbeReceived = now;
			return true;
		}
		return false;
	}

	/**
	 * Rate limit gate for inbound ECHO requests
	 */
	ZT_INLINE bool rateGateEchoRequest(const int64_t now) noexcept
	{
		if ((now - _lastEchoRequestReceived) >= ZT_PEER_GENERAL_RATE_LIMIT) {
			_lastEchoRequestReceived = now;
			return true;
		}
		return false;
	}

private:
	void _prioritizePaths(int64_t now);

	const RuntimeEnvironment *RR;

	// Read/write mutex for non-atomic non-const fields.
	RWMutex _lock;

	// The permanent identity key resulting from agreement between our identity and this peer's identity.
	SymmetricKey< AES,0,0 > _identityKey;

	// Most recently successful (for decrypt) ephemeral key and one previous key.
	SymmetricKey< AES,ZT_SYMMETRIC_KEY_TTL,ZT_SYMMETRIC_KEY_TTL_MESSAGES > _ephemeralKeys[2];

	Identity _id;
	Locator _locator;

	// the last time something was sent or received from this peer (direct or indirect).
	std::atomic<int64_t> _lastReceive;
	std::atomic<int64_t> _lastSend;

	// The last time we sent a full HELLO to this peer.
	int64_t _lastSentHello; // only checked while locked

	// The last time a WHOIS request was received from this peer (anti-DOS / anti-flood).
	std::atomic<int64_t> _lastWhoisRequestReceived;

	// The last time an ECHO request was received from this peer (anti-DOS / anti-flood).
	std::atomic<int64_t> _lastEchoRequestReceived;

	// The last time a probe was received from this peer (for anti-DOS / anti-flood use).
	std::atomic<int64_t> _lastProbeReceived;

	// The last time we tried to init P2P connectivity with this peer.
	std::atomic<int64_t> _lastAttemptedP2PInit;

	// The last time we sorted paths in order of preference. (This happens pretty often.)
	std::atomic<int64_t> _lastPrioritizedPaths;

	// The last time we opened a can of whupass against this peer's NAT (if enabled).
	std::atomic<int64_t> _lastAttemptedAggressiveNATTraversal;

	// Meters measuring actual bandwidth in, out, and relayed via this peer (mostly if this is a root).
	Meter<> _inMeter;
	Meter<> _outMeter;
	Meter<> _relayedMeter;

	// For SharedPtr<>
	std::atomic<int> __refCount;

	// Direct paths sorted in descending order of preference.
	SharedPtr<Path> _paths[ZT_MAX_PEER_NETWORK_PATHS];

	// Number of paths current alive (number of non-NULL entries in _paths).
	unsigned int _alivePathCount;

	// Queue of batches of one or more physical addresses to try at some point in the future (for NAT traversal logic).
	struct _ContactQueueItem
	{
		ZT_INLINE _ContactQueueItem() {} // NOLINT(cppcoreguidelines-pro-type-member-init,hicpp-member-init,hicpp-use-equals-default,modernize-use-equals-default)
		ZT_INLINE _ContactQueueItem(const InetAddress &a,const uint16_t *pstart,const uint16_t *pend,const unsigned int apt) :
			address(a),
			ports(pstart,pend),
			alivePathThreshold(apt) {}
		ZT_INLINE _ContactQueueItem(const InetAddress &a,const unsigned int apt) :
			address(a),
			ports(),
			alivePathThreshold(apt) {}
		InetAddress address;
		FCV<uint16_t,ZT_PEER_BFG1024_PORT_SCAN_CHUNK_SIZE> ports; // if non-empty try these ports, otherwise use the one in address
		unsigned int alivePathThreshold; // skip and forget if alive path count is >= this
	};
	List<_ContactQueueItem> _contactQueue;

	// Remembered addresses by endpoint type (std::map is smaller for only a few keys).
	std::map< Endpoint::Type,Endpoint,std::less<Endpoint::Type>,Utils::Mallocator< std::pair<const Endpoint::Type,Endpoint> > > _bootstrap;

	// 32-bit probe or 0 if unknown.
	uint32_t _probe;

	uint16_t _vProto;
	uint16_t _vMajor;
	uint16_t _vMinor;
	uint16_t _vRevision;
};

} // namespace ZeroTier

#endif
