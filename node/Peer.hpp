/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2023-01-01
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
#include "Packet.hpp"
#include "SharedPtr.hpp"
#include "AtomicCounter.hpp"
#include "Hashtable.hpp"
#include "Mutex.hpp"

#include <vector>

namespace ZeroTier {

/**
 * Peer on P2P Network (virtual layer 1)
 */
class Peer
{
	friend class SharedPtr<Peer>;

private:
	ZT_ALWAYS_INLINE Peer() {}

public:
	ZT_ALWAYS_INLINE ~Peer() { Utils::burn(_key,sizeof(_key)); }

	/**
	 * Construct a new peer
	 *
	 * @param renv Runtime environment
	 * @param myIdentity Identity of THIS node (for key agreement)
	 * @param peerIdentity Identity of peer
	 * @throws std::runtime_error Key agreement with peer's identity failed
	 */
	Peer(const RuntimeEnvironment *renv,const Identity &myIdentity,const Identity &peerIdentity);

	/**
	 * @return This peer's ZT address (short for identity().address())
	 */
	ZT_ALWAYS_INLINE const Address &address() const { return _id.address(); }

	/**
	 * @return This peer's identity
	 */
	ZT_ALWAYS_INLINE const Identity &identity() const { return _id; }

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
	 * @param inRePacketId Packet ID in reply to (default: none)
	 * @param inReVerb Verb in reply to (for OK/ERROR, default: VERB_NOP)
	 * @param networkId Network ID if this packet is related to a network, 0 otherwise
	 */
	void received(
		void *tPtr,
		const SharedPtr<Path> &path,
		unsigned int hops,
		uint64_t packetId,
		unsigned int payloadLength,
		Packet::Verb verb,
		uint64_t inRePacketId,
		Packet::Verb inReVerb,
		uint64_t networkId);

	/**
	 * Check whether a path to this peer should be tried if received via e.g. RENDEZVOUS OR PUSH_DIRECT_PATHS
	 *
	 * @param now Current time
	 * @param suggestingPeer Peer suggesting path (may be this peer)
	 * @param addr Remote address
	 * @return True if we have an active path to this destination
	 */
	bool shouldTryPath(void *tPtr,int64_t now,const SharedPtr<Peer> &suggestedBy,const InetAddress &addr) const;

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
	 * Send ping to this peer
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param now Current time
	 * @param v4SendCount Number of IPv4 packets sent (result parameter)
	 * @param v6SendCount Number of IPv6 packets sent (result parameter)
	 * @param pingAllAddressTypes If true, try to keep a link up for each address type/family
	 */
	void ping(void *tPtr,int64_t now,unsigned int &v4SendCount,unsigned int &v6SendCount,bool pingAllAddressTypes);

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
	 * @return Time of last receive of anything, whether direct or relayed
	 */
	ZT_ALWAYS_INLINE int64_t lastReceive() const { return _lastReceive; }

	/**
	 * @return True if we've heard from this peer in less than ZT_PEER_ACTIVITY_TIMEOUT
	 */
	ZT_ALWAYS_INLINE bool alive(const int64_t now) const { return ((now - _lastReceive) < ZT_PEER_ACTIVITY_TIMEOUT); }

	/**
	 * @return Latency in milliseconds of best/aggregate path or 0xffff if unknown
	 */
	ZT_ALWAYS_INLINE unsigned int latency() const { return _latency; }

	/**
	 * Update peer latency information
	 *
	 * This is called from packet parsing code.
	 *
	 * @param l New latency measurment (in milliseconds)
	 */
	void updateLatency(const unsigned int l);

	/**
	 * @return 256-bit secret symmetric encryption key
	 */
	ZT_ALWAYS_INLINE const unsigned char *key() const { return _key; }

	/**
	 * Set the currently known remote version of this peer's client
	 *
	 * @param vproto Protocol version
	 * @param vmaj Major version
	 * @param vmin Minor version
	 * @param vrev Revision
	 */
	ZT_ALWAYS_INLINE void setRemoteVersion(unsigned int vproto,unsigned int vmaj,unsigned int vmin,unsigned int vrev)
	{
		_vProto = (uint16_t)vproto;
		_vMajor = (uint16_t)vmaj;
		_vMinor = (uint16_t)vmin;
		_vRevision = (uint16_t)vrev;
	}

	ZT_ALWAYS_INLINE unsigned int remoteVersionProtocol() const { return _vProto; }
	ZT_ALWAYS_INLINE unsigned int remoteVersionMajor() const { return _vMajor; }
	ZT_ALWAYS_INLINE unsigned int remoteVersionMinor() const { return _vMinor; }
	ZT_ALWAYS_INLINE unsigned int remoteVersionRevision() const { return _vRevision; }
	ZT_ALWAYS_INLINE bool remoteVersionKnown() const { return ((_vMajor > 0)||(_vMinor > 0)||(_vRevision > 0)); }

	/**
	 * Rate limit gate for inbound WHOIS requests
	 */
	ZT_ALWAYS_INLINE bool rateGateInboundWhoisRequest(const int64_t now)
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
	ZT_ALWAYS_INLINE bool rateGateInboundPushDirectPaths(const int64_t now)
	{
		if ((now - _lastPushDirectPathsReceived) >= ZT_DIRECT_PATH_PUSH_INTERVAL) {
			_lastPushDirectPathsReceived = now;
			return true;
		}
		return false;
	}

	/**
	 * Rate limit gate for inbound ECHO requests
	 */
	ZT_ALWAYS_INLINE bool rateGateEchoRequest(const int64_t now)
	{
		if ((now - _lastEchoRequestReceived) >= ZT_PEER_GENERAL_RATE_LIMIT) {
			_lastEchoRequestReceived = now;
			return true;
		}
		return false;
	}

	/**
	 * Rate limit gate for trying externally defined or static path
	 */
	ZT_ALWAYS_INLINE bool rateGateTryStaticPath(const int64_t now)
	{
		if ((now - _lastTriedStaticPath) >= ZT_PEER_PING_PERIOD) {
			_lastTriedStaticPath = now;
			return true;
		}
		return false;
	}

	/**
	 * Send directly if a direct path exists
	 *
	 * @param tPtr Thread pointer supplied by user
	 * @param data Data to send
	 * @param len Length of data
	 * @param now Current time
	 * @return True if packet appears to have been sent, false if no path or send failed
	 */
	bool sendDirect(void *tPtr,const void *data,unsigned int len,const int64_t now);

	/**
	 * @return Current best path
	 */
	SharedPtr<Path> path(int64_t now);

	/**
	 * Get all paths
	 *
	 * @param paths Vector of paths with the first path being the current preferred path
	 */
	void getAllPaths(std::vector< SharedPtr<Path> > &paths);

private:
	void _prioritizePaths(int64_t now);

	uint8_t _key[ZT_PEER_SECRET_KEY_LENGTH];

	const RuntimeEnvironment *RR;

	volatile int64_t _lastReceive;
	volatile int64_t _lastWhoisRequestReceived;
	volatile int64_t _lastEchoRequestReceived;
	volatile int64_t _lastPushDirectPathsReceived;
	volatile int64_t _lastPushDirectPathsSent;
	volatile int64_t _lastTriedStaticPath;
	volatile int64_t _lastPrioritizedPaths;
	volatile unsigned int _latency;

	AtomicCounter __refCount;

	unsigned int _alivePathCount;
	SharedPtr<Path> _paths[ZT_MAX_PEER_NETWORK_PATHS];
	RWMutex _paths_l;

	Identity _id;

	uint16_t _vProto;
	uint16_t _vMajor;
	uint16_t _vMinor;
	uint16_t _vRevision;
};

} // namespace ZeroTier

#endif
