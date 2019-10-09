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

#include <vector>

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

#define ZT_PEER_MAX_SERIALIZED_STATE_SIZE (sizeof(Peer) + 32 + (sizeof(Path) * 2))

namespace ZeroTier {

/**
 * Peer on P2P Network (virtual layer 1)
 */
class Peer
{
	friend class SharedPtr<Peer>;

private:
	inline Peer() {} // disabled to prevent bugs -- should not be constructed uninitialized

public:
	inline ~Peer() { Utils::burn(_key,sizeof(_key)); }

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
		const unsigned int hops,
		const uint64_t packetId,
		const unsigned int payloadLength,
		const Packet::Verb verb,
		const uint64_t inRePacketId,
		const Packet::Verb inReVerb,
		const uint64_t networkId);

	/**
	 * Check whether we have an active path to this peer via the given address
	 *
	 * @param now Current time
	 * @param addr Remote address
	 * @return True if we have an active path to this destination
	 */
	ZT_ALWAYS_INLINE bool hasActivePathTo(int64_t now,const InetAddress &addr) const
	{
		Mutex::Lock _l(_paths_m);
		for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
			if (_paths[i]) {
				if ((_paths[i]->address() == addr)&&(_paths[i]->alive(now)))
					return true;
			} else break;
		}
		return false;
	}

	/**
	 * Send via best direct path
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param data Packet data
	 * @param len Packet length
	 * @param now Current time
	 * @param force If true, send even if path is not alive
	 * @return True if we actually sent something
	 */
	ZT_ALWAYS_INLINE bool sendDirect(void *tPtr,const void *data,unsigned int len,int64_t now,bool force)
	{
		SharedPtr<Path> bp(getAppropriatePath(now,force));
		if (bp)
			return bp->send(RR,tPtr,data,len,now);
		return false;
	}

	/**
	 * Record statistics on outgoing packets
	 *
	 * @param path Path over which packet was sent
	 * @param id Packet ID
	 * @param len Length of packet payload
	 * @param verb Packet verb
	 * @param now Current time
	 */
	void recordOutgoingPacket(const SharedPtr<Path> &path, const uint64_t packetId, uint16_t payloadLength, const Packet::Verb verb, int64_t now);

	/**
	 * Record statistics on incoming packets
	 *
	 * @param path Path over which packet was sent
	 * @param id Packet ID
	 * @param len Length of packet payload
	 * @param verb Packet verb
	 * @param now Current time
	 */
	void recordIncomingPacket(void *tPtr, const SharedPtr<Path> &path, const uint64_t packetId, uint16_t payloadLength, const Packet::Verb verb, int64_t now);

	/**
	 * Send an ACK to peer for the most recent packets received
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param localSocket Raw socket the ACK packet will be sent over
	 * @param atAddress Destination for the ACK packet
	 * @param now Current time
	 */
	void sendACK(void *tPtr, const SharedPtr<Path> &path, const int64_t localSocket,const InetAddress &atAddress,int64_t now);

	/**
	 * Send a QoS packet to peer so that it can evaluate the quality of this link
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param localSocket Raw socket the QoS packet will be sent over
	 * @param atAddress Destination for the QoS packet
	 * @param now Current time
	 */
	void sendQOS_MEASUREMENT(void *tPtr, const SharedPtr<Path> &path, const int64_t localSocket,const InetAddress &atAddress,int64_t now);

	/**
	 * Compute relative quality values and allocations for the components of the aggregate link
	 *
	 * @param now Current time
	 */
	void computeAggregateProportionalAllocation(int64_t now);

	/**
	 * @return The aggregate link Packet Delay Variance (PDV)
	 */
	int computeAggregateLinkPacketDelayVariance();

	/**
	 * @return The aggregate link mean latency
	 */
	int computeAggregateLinkMeanLatency();

	/**
	 * @return The number of currently alive "physical" paths in the aggregate link
	 */
	int aggregateLinkPhysicalPathCount();

	/**
	 * @return The number of currently alive "logical" paths in the aggregate link
	 */
	int aggregateLinkLogicalPathCount();

	/**
	 * Get the most appropriate direct path based on current multipath and QoS configuration
	 *
	 * @param now Current time
	 * @param includeExpired If true, include even expired paths
	 * @return Best current path or NULL if none
	 */
	SharedPtr<Path> getAppropriatePath(int64_t now, bool includeExpired);

	/**
	 * Generate a human-readable string of interface names making up the aggregate link, also include
	 * moving allocation and IP version number for each (for tracing)
	 */
	char *interfaceListStr();

	/**
	 * Send VERB_RENDEZVOUS to this and another peer via the best common IP scope and path
	 */
	void introduce(void *const tPtr,const int64_t now,const SharedPtr<Peer> &other) const;

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
	void sendHELLO(void *tPtr,const int64_t localSocket,const InetAddress &atAddress,int64_t now);

	/**
	 * Send pings to active paths
	 *
	 * This also cleans up some internal data structures. It's called periodically from Node.
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param now Current time
	 * @param v4SendCount Number of IPv4 packets sent (result parameter)
	 * @param v6SendCount Number of IPv6 packets sent (result parameter)
	 */
	void ping(void *tPtr,int64_t now,unsigned int &v4SendCount,unsigned int &v6SendCount);

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
	 * @param now Current time
	 * @return All known paths to this peer
	 */
	ZT_ALWAYS_INLINE std::vector< SharedPtr<Path> > paths(const int64_t now) const
	{
		std::vector< SharedPtr<Path> > pp;
		Mutex::Lock _l(_paths_m);
		for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
			if (!_paths[i]) break;
			pp.push_back(_paths[i]);
		}
		return pp;
	}

	/**
	 * @return Time of last receive of anything, whether direct or relayed
	 */
	ZT_ALWAYS_INLINE int64_t lastReceive() const { return _lastReceive; }

	/**
	 * @return True if we've heard from this peer in less than ZT_PEER_ACTIVITY_TIMEOUT
	 */
	ZT_ALWAYS_INLINE bool alive(const int64_t now) const { return ((now - _lastReceive) < ZT_PEER_ACTIVITY_TIMEOUT); }

	/**
	 * @return Latency in milliseconds of best/aggregate path or 0xffff if unknown / no paths
	 */
	ZT_ALWAYS_INLINE unsigned int latency(const int64_t now)
	{
		if (_canUseMultipath) {
			return (int)computeAggregateLinkMeanLatency();
		} else {
			SharedPtr<Path> bp(getAppropriatePath(now,false));
			if (bp)
				return bp->latency();
			return 0xffff;
		}
	}

	/**
	 * This computes a quality score for relays and root servers
	 *
	 * If we haven't heard anything from these in ZT_PEER_ACTIVITY_TIMEOUT, they
	 * receive the worst possible quality (max unsigned int). Otherwise the
	 * quality is a product of latency and the number of potential missed
	 * pings. This causes roots and relays to switch over a bit faster if they
	 * fail.
	 *
	 * @return Relay quality score computed from latency and other factors, lower is better
	 */
	ZT_ALWAYS_INLINE unsigned int relayQuality(const int64_t now)
	{
		const uint64_t tsr = now - _lastReceive;
		if (tsr >= ZT_PEER_ACTIVITY_TIMEOUT)
			return (~(unsigned int)0);
		unsigned int l = latency(now);
		if (!l)
			l = 0xffff;
		return (l * (((unsigned int)tsr / (ZT_PEER_PING_PERIOD + 1000)) + 1));
	}

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
	 * Periodically update known multipath activation constraints. This is done so that we know when and when
	 * not to use multipath logic. Doing this once every few seconds is sufficient.
	 *
	 * @param now Current time
	 */
	void processBackgroundPeerTasks(const int64_t now);

	/**
	 * Record that the remote peer does have multipath enabled. As is evident by the receipt of a VERB_ACK
	 * or a VERB_QOS_MEASUREMENT packet at some point in the past. Until this flag is set, the local client
	 * shall assume that multipath is not enabled and should only use classical Protocol 9 logic.
	 */
	ZT_ALWAYS_INLINE void inferRemoteMultipathEnabled() { _remotePeerMultipathEnabled = true; }

	/**
	 * @return Whether the local client supports and is configured to use multipath
	 */
	ZT_ALWAYS_INLINE bool localMultipathSupport() { return _localMultipathSupported; }

	/**
	 * @return Whether the remote peer supports and is configured to use multipath
	 */
	ZT_ALWAYS_INLINE bool remoteMultipathSupport() { return _remoteMultipathSupported; }

	/**
	 * @return Whether this client can use multipath to communicate with this peer. True if both peers are using
	 * the correct protocol and if both peers have multipath enabled. False if otherwise.
	 */
	ZT_ALWAYS_INLINE bool canUseMultipath() { return _canUseMultipath; }

	/**
	 * Rate limit gate for VERB_PUSH_DIRECT_PATHS
	 */
	ZT_ALWAYS_INLINE bool rateGatePushDirectPaths(const int64_t now)
	{
		if ((now - _lastDirectPathPushReceive) <= ZT_PUSH_DIRECT_PATHS_CUTOFF_TIME)
			++_directPathPushCutoffCount;
		else _directPathPushCutoffCount = 0;
		_lastDirectPathPushReceive = now;
		return (_directPathPushCutoffCount < ZT_PUSH_DIRECT_PATHS_CUTOFF_LIMIT);
	}

	/**
	 * Rate limit gate for VERB_NETWORK_CREDENTIALS
	 */
	ZT_ALWAYS_INLINE bool rateGateCredentialsReceived(const int64_t now)
	{
		if ((now - _lastCredentialsReceived) <= ZT_PEER_CREDENTIALS_CUTOFF_TIME)
			++_credentialsCutoffCount;
		else _credentialsCutoffCount = 0;
		_lastCredentialsReceived = now;
		return (_directPathPushCutoffCount < ZT_PEER_CREDEITIALS_CUTOFF_LIMIT);
	}

	/**
	 * Rate limit gate for sending of ERROR_NEED_MEMBERSHIP_CERTIFICATE
	 */
	ZT_ALWAYS_INLINE bool rateGateRequestCredentials(const int64_t now)
	{
		if ((now - _lastCredentialRequestSent) >= ZT_PEER_GENERAL_RATE_LIMIT) {
			_lastCredentialRequestSent = now;
			return true;
		}
		return false;
	}

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
	 * Rate limit gate for VERB_ACK
	 */
	ZT_ALWAYS_INLINE bool rateGateACK(const int64_t now)
	{
		if ((now - _lastACKWindowReset) >= ZT_PATH_QOS_ACK_CUTOFF_TIME) {
			_lastACKWindowReset = now;
			_ACKCutoffCount = 0;
		} else {
			++_ACKCutoffCount;
		}
		return (_ACKCutoffCount < ZT_PATH_QOS_ACK_CUTOFF_LIMIT);
	}

	/**
	 * Rate limit gate for VERB_QOS_MEASUREMENT
	 */
	ZT_ALWAYS_INLINE bool rateGateQoS(const int64_t now)
	{
		if ((now - _lastQoSWindowReset) >= ZT_PATH_QOS_ACK_CUTOFF_TIME) {
			_lastQoSWindowReset = now;
			_QoSCutoffCount = 0;
		} else {
			++_QoSCutoffCount;
		}
		return (_QoSCutoffCount < ZT_PATH_QOS_ACK_CUTOFF_LIMIT);
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
	 * @return Whether this peer is reachable via an aggregate link
	 */
	ZT_ALWAYS_INLINE bool hasAggregateLink() const
	{
		return _localMultipathSupported && _remoteMultipathSupported && _remotePeerMultipathEnabled;
	}

private:
	uint8_t _key[ZT_PEER_SECRET_KEY_LENGTH];

	const RuntimeEnvironment *RR;

	int64_t _lastReceive; // direct or indirect
	int64_t _lastDirectPathPushSent;
	int64_t _lastDirectPathPushReceive;
	int64_t _lastCredentialRequestSent;
	int64_t _lastWhoisRequestReceived;
	int64_t _lastEchoRequestReceived;
	int64_t _lastCredentialsReceived;
	int64_t _lastACKWindowReset;
	int64_t _lastQoSWindowReset;
	int64_t _lastMultipathCompatibilityCheck;
	int64_t _lastTriedStaticPath;

	int _uniqueAlivePathCount;

	bool _localMultipathSupported;
	bool _remoteMultipathSupported;
	bool _canUseMultipath;
	uint8_t _freeRandomByte;

	uint16_t _vProto;
	uint16_t _vMajor;
	uint16_t _vMinor;
	uint16_t _vRevision;

	SharedPtr<Path> _paths[ZT_MAX_PEER_NETWORK_PATHS];
	Mutex _paths_m;

	Identity _id;

	unsigned int _directPathPushCutoffCount;
	unsigned int _credentialsCutoffCount;
	unsigned int _QoSCutoffCount;
	unsigned int _ACKCutoffCount;

	RingBuffer<int,ZT_MULTIPATH_PROPORTION_WIN_SZ> _pathChoiceHist;

	bool _linkIsBalanced;
	bool _linkIsRedundant;
	bool _remotePeerMultipathEnabled;

	int64_t _lastAggregateStatsReport;
	int64_t _lastAggregateAllocation;

	char _interfaceListStr[256]; // 16 characters * 16 paths in a link

	AtomicCounter __refCount;
};

} // namespace ZeroTier

// Add a swap() for shared ptr's to peers to speed up peer sorts
namespace std {
	template<>
	ZT_ALWAYS_INLINE void swap(ZeroTier::SharedPtr<ZeroTier::Peer> &a,ZeroTier::SharedPtr<ZeroTier::Peer> &b) { a.swap(b); }
}

#endif
