/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2017  ZeroTier, Inc.  https://www.zerotier.com/
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

#ifndef ZT_PEER_HPP
#define ZT_PEER_HPP

#include <stdint.h>

#include "Constants.hpp"

#include <algorithm>
#include <utility>
#include <vector>
#include <stdexcept>

#include "../include/ZeroTierOne.h"

#include "RuntimeEnvironment.hpp"
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
#include "NonCopyable.hpp"

namespace ZeroTier {

/**
 * Peer on P2P Network (virtual layer 1)
 */
class Peer : NonCopyable
{
	friend class SharedPtr<Peer>;

private:
	Peer() {} // disabled to prevent bugs -- should not be constructed uninitialized

public:
	~Peer() { Utils::burn(_key,sizeof(_key)); }

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
	inline const Address &address() const throw() { return _id.address(); }

	/**
	 * @return This peer's identity
	 */
	inline const Identity &identity() const throw() { return _id; }

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
	 * @param trustEstablished If true, some form of non-trivial trust (like allowed in network) has been established
	 */
	void received(
		void *tPtr,
		const SharedPtr<Path> &path,
		const unsigned int hops,
		const uint64_t packetId,
		const Packet::Verb verb,
		const uint64_t inRePacketId,
		const Packet::Verb inReVerb,
		const bool trustEstablished);

	/**
	 * @param now Current time
	 * @param addr Remote address
	 * @return True if we have an active path to this destination
	 */
	inline bool hasActivePathTo(uint64_t now,const InetAddress &addr) const
	{
		Mutex::Lock _l(_paths_m);
		return ( ((addr.ss_family == AF_INET)&&(_v4Path.p)&&(_v4Path.p->address() == addr)&&(_v4Path.p->alive(now))) || ((addr.ss_family == AF_INET6)&&(_v6Path.p)&&(_v6Path.p->address() == addr)&&(_v6Path.p->alive(now))) );
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
	bool sendDirect(void *tPtr,const void *data,unsigned int len,uint64_t now,bool force);

	/**
	 * Get the best current direct path
	 *
	 * This does not check Path::alive(), but does return the most recently
	 * active path and does check expiration (which is a longer timeout).
	 *
	 * @param now Current time
	 * @param includeExpired If true, include even expired paths
	 * @return Best current path or NULL if none
	 */
	SharedPtr<Path> getBestPath(uint64_t now,bool includeExpired);

	/**
	 * Send a HELLO to this peer at a specified physical address
	 *
	 * No statistics or sent times are updated here.
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param localAddr Local address
	 * @param atAddress Destination address
	 * @param now Current time
	 * @param counter Outgoing packet counter
	 */
	void sendHELLO(void *tPtr,const InetAddress &localAddr,const InetAddress &atAddress,uint64_t now,unsigned int counter);

	/**
	 * Send ECHO (or HELLO for older peers) to this peer at the given address
	 *
	 * No statistics or sent times are updated here.
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param localAddr Local address
	 * @param atAddress Destination address
	 * @param now Current time
	 * @param sendFullHello If true, always send a full HELLO instead of just an ECHO
	 * @param counter Outgoing packet counter
	 */
	void attemptToContactAt(void *tPtr,const InetAddress &localAddr,const InetAddress &atAddress,uint64_t now,bool sendFullHello,unsigned int counter);

	/**
	 * Try a memorized or statically defined path if any are known
	 *
	 * Under the hood this is done periodically based on ZT_TRY_MEMORIZED_PATH_INTERVAL.
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param now Current time
	 */
	void tryMemorizedPath(void *tPtr,uint64_t now);

	/**
	 * Send pings or keepalives depending on configured timeouts
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param now Current time
	 * @param inetAddressFamily Keep this address family alive, or -1 for any
	 * @return True if we have at least one direct path of the given family (or any if family is -1)
	 */
	bool doPingAndKeepalive(void *tPtr,uint64_t now,int inetAddressFamily);

	/**
	 * Reset paths within a given IP scope and address family
	 *
	 * Resetting a path involves sending an ECHO to it and then deactivating
	 * it until or unless it responds.
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param scope IP scope
	 * @param inetAddressFamily Family e.g. AF_INET
	 * @param now Current time
	 */
	inline void resetWithinScope(void *tPtr,InetAddress::IpScope scope,int inetAddressFamily,uint64_t now)
	{
		Mutex::Lock _l(_paths_m);
		if ((inetAddressFamily == AF_INET)&&(_v4Path.lr)&&(_v4Path.p->address().ipScope() == scope)) {
			attemptToContactAt(tPtr,_v4Path.p->localAddress(),_v4Path.p->address(),now,false,_v4Path.p->nextOutgoingCounter());
			_v4Path.p->sent(now);
			_v4Path.lr = 0; // path will not be used unless it speaks again
		} else if ((inetAddressFamily == AF_INET6)&&(_v6Path.lr)&&(_v6Path.p->address().ipScope() == scope)) {
			attemptToContactAt(tPtr,_v6Path.p->localAddress(),_v6Path.p->address(),now,false,_v6Path.p->nextOutgoingCounter());
			_v6Path.p->sent(now);
			_v6Path.lr = 0; // path will not be used unless it speaks again
		}
	}

	/**
	 * Indicate that the given address was provided by a cluster as a preferred destination
	 *
	 * @param addr Address cluster prefers that we use
	 */
	inline void setClusterPreferred(const InetAddress &addr)
	{
		if (addr.ss_family == AF_INET)
			_v4ClusterPreferred = addr;
		else if (addr.ss_family == AF_INET6)
			_v6ClusterPreferred = addr;
	}

	/**
	 * Fill parameters with V4 and V6 addresses if known and alive
	 *
	 * @param now Current time
	 * @param v4 Result parameter to receive active IPv4 address, if any
	 * @param v6 Result parameter to receive active IPv6 address, if any
	 */
	inline void getRendezvousAddresses(uint64_t now,InetAddress &v4,InetAddress &v6) const
	{
		Mutex::Lock _l(_paths_m);
		if (((now - _v4Path.lr) < ZT_PEER_PATH_EXPIRATION)&&(_v4Path.p->alive(now)))
			v4 = _v4Path.p->address();
		if (((now - _v6Path.lr) < ZT_PEER_PATH_EXPIRATION)&&(_v6Path.p->alive(now)))
			v6 = _v6Path.p->address();
	}

	/**
	 * @param now Current time
	 * @return All known paths to this peer
	 */
	inline std::vector< SharedPtr<Path> > paths(const uint64_t now) const
	{
		std::vector< SharedPtr<Path> > pp;
		Mutex::Lock _l(_paths_m);
		if (((now - _v4Path.lr) < ZT_PEER_PATH_EXPIRATION)&&(_v4Path.p->alive(now)))
			pp.push_back(_v4Path.p);
		if (((now - _v6Path.lr) < ZT_PEER_PATH_EXPIRATION)&&(_v6Path.p->alive(now)))
			pp.push_back(_v6Path.p);
		return pp;
	}

	/**
	 * @return Time of last receive of anything, whether direct or relayed
	 */
	inline uint64_t lastReceive() const { return _lastReceive; }

	/**
	 * @return True if we've heard from this peer in less than ZT_PEER_ACTIVITY_TIMEOUT
	 */
	inline bool isAlive(const uint64_t now) const { return ((now - _lastReceive) < ZT_PEER_ACTIVITY_TIMEOUT); }

	/**
	 * @return True if this peer has sent us real network traffic recently
	 */
	inline uint64_t isActive(uint64_t now) const { return ((now - _lastNontrivialReceive) < ZT_PEER_ACTIVITY_TIMEOUT); }

	/**
	 * @return Latency in milliseconds or 0 if unknown
	 */
	inline unsigned int latency() const { return _latency; }

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
	inline unsigned int relayQuality(const uint64_t now) const
	{
		const uint64_t tsr = now - _lastReceive;
		if (tsr >= ZT_PEER_ACTIVITY_TIMEOUT)
			return (~(unsigned int)0);
		unsigned int l = _latency;
		if (!l)
			l = 0xffff;
		return (l * (((unsigned int)tsr / (ZT_PEER_PING_PERIOD + 1000)) + 1));
	}

	/**
	 * Update latency with a new direct measurment
	 *
	 * @param l Direct latency measurment in ms
	 */
	inline void addDirectLatencyMeasurment(unsigned int l)
	{
		unsigned int ol = _latency;
		if ((ol > 0)&&(ol < 10000))
			_latency = (ol + std::min(l,(unsigned int)65535)) / 2;
		else _latency = std::min(l,(unsigned int)65535);
	}

#ifdef ZT_ENABLE_CLUSTER
	/**
	 * @param now Current time
	 * @return True if this peer has at least one active direct path that is not cluster-suboptimal
	 */
	inline bool hasLocalClusterOptimalPath(uint64_t now) const
	{
		Mutex::Lock _l(_paths_m);
		return ( ((_v4Path.p)&&(_v4Path.p->alive(now))&&(!_v4Path.localClusterSuboptimal)) || ((_v6Path.p)&&(_v6Path.p->alive(now))&&(!_v6Path.localClusterSuboptimal)) );
	}
#endif

	/**
	 * @return 256-bit secret symmetric encryption key
	 */
	inline const unsigned char *key() const { return _key; }

	/**
	 * Set the currently known remote version of this peer's client
	 *
	 * @param vproto Protocol version
	 * @param vmaj Major version
	 * @param vmin Minor version
	 * @param vrev Revision
	 */
	inline void setRemoteVersion(unsigned int vproto,unsigned int vmaj,unsigned int vmin,unsigned int vrev)
	{
		_vProto = (uint16_t)vproto;
		_vMajor = (uint16_t)vmaj;
		_vMinor = (uint16_t)vmin;
		_vRevision = (uint16_t)vrev;
	}

	inline unsigned int remoteVersionProtocol() const { return _vProto; }
	inline unsigned int remoteVersionMajor() const { return _vMajor; }
	inline unsigned int remoteVersionMinor() const { return _vMinor; }
	inline unsigned int remoteVersionRevision() const { return _vRevision; }

	inline bool remoteVersionKnown() const { return ((_vMajor > 0)||(_vMinor > 0)||(_vRevision > 0)); }

	/**
	 * @return True if peer has received a trust established packet (e.g. common network membership) in the past ZT_TRUST_EXPIRATION ms
	 */
	inline bool trustEstablished(const uint64_t now) const { return ((now - _lastTrustEstablishedPacketReceived) < ZT_TRUST_EXPIRATION); }

	/**
	 * Rate limit gate for VERB_PUSH_DIRECT_PATHS
	 */
	inline bool rateGatePushDirectPaths(const uint64_t now)
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
	inline bool rateGateCredentialsReceived(const uint64_t now)
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
	inline bool rateGateRequestCredentials(const uint64_t now)
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
	inline bool rateGateInboundWhoisRequest(const uint64_t now)
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
	inline bool rateGateEchoRequest(const uint64_t now)
	{
		if ((now - _lastEchoRequestReceived) >= ZT_PEER_GENERAL_RATE_LIMIT) {
			_lastEchoRequestReceived = now;
			return true;
		}
		return false;
	}

	/**
	 * Rate gate incoming requests for network COM
	 */
	inline bool rateGateIncomingComRequest(const uint64_t now)
	{
		if ((now - _lastComRequestReceived) >= ZT_PEER_GENERAL_RATE_LIMIT) {
			_lastComRequestReceived = now;
			return true;
		}
		return false;
	}

	/**
	 * Rate gate outgoing requests for network COM
	 */
	inline bool rateGateOutgoingComRequest(const uint64_t now)
	{
		if ((now - _lastComRequestSent) >= ZT_PEER_GENERAL_RATE_LIMIT) {
			_lastComRequestSent = now;
			return true;
		}
		return false;
	}

private:
	struct _PeerPath
	{
#ifdef ZT_ENABLE_CLUSTER
		_PeerPath() : lr(0),p(),localClusterSuboptimal(false) {}
#else
		_PeerPath() : lr(0),p() {}
#endif
		uint64_t lr; // time of last valid ZeroTier packet
		SharedPtr<Path> p;
#ifdef ZT_ENABLE_CLUSTER
		bool localClusterSuboptimal; // true if our cluster has determined that we should not be serving this peer
#endif
	};

	uint8_t _key[ZT_PEER_SECRET_KEY_LENGTH];

	const RuntimeEnvironment *RR;

	uint64_t _lastReceive; // direct or indirect
	uint64_t _lastNontrivialReceive; // frames, things like netconf, etc.
	uint64_t _lastTriedMemorizedPath;
	uint64_t _lastDirectPathPushSent;
	uint64_t _lastDirectPathPushReceive;
	uint64_t _lastCredentialRequestSent;
	uint64_t _lastWhoisRequestReceived;
	uint64_t _lastEchoRequestReceived;
	uint64_t _lastComRequestReceived;
	uint64_t _lastComRequestSent;
	uint64_t _lastCredentialsReceived;
	uint64_t _lastTrustEstablishedPacketReceived;

	uint16_t _vProto;
	uint16_t _vMajor;
	uint16_t _vMinor;
	uint16_t _vRevision;

	InetAddress _v4ClusterPreferred;
	InetAddress _v6ClusterPreferred;

	_PeerPath _v4Path; // IPv4 direct path
	_PeerPath _v6Path; // IPv6 direct path
	Mutex _paths_m;

	Identity _id;

	unsigned int _latency;
	unsigned int _directPathPushCutoffCount;
	unsigned int _credentialsCutoffCount;

	AtomicCounter __refCount;
};

} // namespace ZeroTier

// Add a swap() for shared ptr's to peers to speed up peer sorts
namespace std {
	template<>
	inline void swap(ZeroTier::SharedPtr<ZeroTier::Peer> &a,ZeroTier::SharedPtr<ZeroTier::Peer> &b)
	{
		a.swap(b);
	}
}

#endif
