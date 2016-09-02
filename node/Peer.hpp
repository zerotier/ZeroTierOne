/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2016  ZeroTier, Inc.  https://www.zerotier.com/
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
	 * @return Time peer record was last used in any way
	 */
	inline uint64_t lastUsed() const throw() { return _lastUsed; }

	/**
	 * Log a use of this peer record (done by Topology when peers are looked up)
	 *
	 * @param now New time of last use
	 */
	inline void use(uint64_t now) throw() { _lastUsed = now; }

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
	 * @param path Path over which packet was received
	 * @param hops ZeroTier (not IP) hops
	 * @param packetId Packet ID
	 * @param verb Packet verb
	 * @param inRePacketId Packet ID in reply to (default: none)
	 * @param inReVerb Verb in reply to (for OK/ERROR, default: VERB_NOP)
	 * @param trustEstablished If true, some form of non-trivial trust (like allowed in network) has been established
	 */
	void received(
		const SharedPtr<Path> &path,
		unsigned int hops,
		uint64_t packetId,
		Packet::Verb verb,
		uint64_t inRePacketId,
		Packet::Verb inReVerb,
		const bool trustEstablished);

	/**
	 * @param now Current time
	 * @param addr Remote address
	 * @return True if we have an active path to this destination
	 */
	bool hasActivePathTo(uint64_t now,const InetAddress &addr) const;

	/**
	 * If we have a confirmed path to this address, mark others as cluster suboptimal
	 *
	 * @param addr Address to make exclusive
	 */
	void setClusterOptimal(const InetAddress &addr);

	/**
	 * Send via best direct path
	 *
	 * @param data Packet data
	 * @param len Packet length
	 * @param now Current time
	 * @param forceEvenIfDead If true, send even if the path is not 'alive'
	 * @return True if we actually sent something
	 */
	bool sendDirect(const void *data,unsigned int len,uint64_t now,bool forceEvenIfDead);

	/**
	 * Get the best current direct path
	 *
	 * @param now Current time
	 * @return Best current path or NULL if none
	 */
	SharedPtr<Path> getBestPath(uint64_t now);

	/**
	 * Send a HELLO to this peer at a specified physical address
	 *
	 * No statistics or sent times are updated here.
	 *
	 * @param localAddr Local address
	 * @param atAddress Destination address
	 * @param now Current time
	 */
	void sendHELLO(const InetAddress &localAddr,const InetAddress &atAddress,uint64_t now);

	/**
	 * Send pings or keepalives depending on configured timeouts
	 *
	 * @param now Current time
	 * @param inetAddressFamily Keep this address family alive, or 0 to simply pick current best ignoring family
	 * @return True if we have at least one direct path
	 */
	bool doPingAndKeepalive(uint64_t now,int inetAddressFamily);

	/**
	 * @param now Current time
	 * @return True if this peer has at least one active direct path
	 */
	bool hasActiveDirectPath(uint64_t now) const;

	/**
	 * Reset paths within a given scope
	 *
	 * @param scope IP scope of paths to reset
	 * @param now Current time
	 * @return True if at least one path was forgotten
	 */
	bool resetWithinScope(InetAddress::IpScope scope,uint64_t now);

	/**
	 * Get most recently active path addresses for IPv4 and/or IPv6
	 *
	 * Note that v4 and v6 are not modified if they are not found, so
	 * initialize these to a NULL address to be able to check.
	 *
	 * @param now Current time
	 * @param v4 Result parameter to receive active IPv4 address, if any
	 * @param v6 Result parameter to receive active IPv6 address, if any
	 */
	void getBestActiveAddresses(uint64_t now,InetAddress &v4,InetAddress &v6) const;

	/**
	 * Perform periodic cleaning operations
	 *
	 * @param now Current time
	 */
	void clean(uint64_t now);

	/**
	 * @return All known direct paths to this peer (active or inactive)
	 */
	inline std::vector< SharedPtr<Path> > paths() const
	{
		std::vector< SharedPtr<Path> > pp;
		Mutex::Lock _l(_paths_m);
		for(unsigned int p=0,np=_numPaths;p<np;++p)
			pp.push_back(_paths[p].path);
		return pp;
	}

	/**
	 * @return Time of last receive of anything, whether direct or relayed
	 */
	inline uint64_t lastReceive() const throw() { return _lastReceive; }

	/**
	 * @return Time of most recent unicast frame received
	 */
	inline uint64_t lastUnicastFrame() const throw() { return _lastUnicastFrame; }

	/**
	 * @return Time of most recent multicast frame received
	 */
	inline uint64_t lastMulticastFrame() const throw() { return _lastMulticastFrame; }

	/**
	 * @return Time of most recent frame of any kind (unicast or multicast)
	 */
	inline uint64_t lastFrame() const throw() { return std::max(_lastUnicastFrame,_lastMulticastFrame); }

	/**
	 * @return True if this peer has sent us real network traffic recently
	 */
	inline uint64_t activelyTransferringFrames(uint64_t now) const throw() { return ((now - lastFrame()) < ZT_PEER_ACTIVITY_TIMEOUT); }

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
	inline bool hasClusterOptimalPath(uint64_t now) const
	{
		for(unsigned int p=0,np=_numPaths;p<np;++p) {
			if ( (_paths[p].path->alive(now)) && (!_paths[p].clusterSuboptimal) )
				return true;
		}
		return false;
	}
#endif

	/**
	 * @return 256-bit secret symmetric encryption key
	 */
	inline const unsigned char *key() const throw() { return _key; }

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

	inline unsigned int remoteVersionProtocol() const throw() { return _vProto; }
	inline unsigned int remoteVersionMajor() const throw() { return _vMajor; }
	inline unsigned int remoteVersionMinor() const throw() { return _vMinor; }
	inline unsigned int remoteVersionRevision() const throw() { return _vRevision; }

	inline bool remoteVersionKnown() const throw() { return ((_vMajor > 0)||(_vMinor > 0)||(_vRevision > 0)); }

	/**
	 * Update direct path push stats and return true if we should respond
	 *
	 * This is a circuit breaker to make VERB_PUSH_DIRECT_PATHS not particularly
	 * useful as a DDOS amplification attack vector. Otherwise a malicious peer
	 * could send loads of these and cause others to bombard arbitrary IPs with
	 * traffic.
	 *
	 * @param now Current time
	 * @return True if we should respond
	 */
	inline bool shouldRespondToDirectPathPush(const uint64_t now)
	{
		if ((now - _lastDirectPathPushReceive) <= ZT_PUSH_DIRECT_PATHS_CUTOFF_TIME)
			++_directPathPushCutoffCount;
		else _directPathPushCutoffCount = 0;
		_lastDirectPathPushReceive = now;
		return (_directPathPushCutoffCount < ZT_PUSH_DIRECT_PATHS_CUTOFF_LIMIT);
	}

	/**
	 * Find a common set of addresses by which two peers can link, if any
	 *
	 * @param a Peer A
	 * @param b Peer B
	 * @param now Current time
	 * @return Pair: B's address (to send to A), A's address (to send to B)
	 */
	static inline std::pair<InetAddress,InetAddress> findCommonGround(const Peer &a,const Peer &b,uint64_t now)
	{
		std::pair<InetAddress,InetAddress> v4,v6;
		b.getBestActiveAddresses(now,v4.first,v6.first);
		a.getBestActiveAddresses(now,v4.second,v6.second);
		if ((v6.first)&&(v6.second)) // prefer IPv6 if both have it since NAT-t is (almost) unnecessary
			return v6;
		else if ((v4.first)&&(v4.second))
			return v4;
		else return std::pair<InetAddress,InetAddress>();
	}

private:
	bool _pushDirectPaths(const SharedPtr<Path> &path,uint64_t now);

	inline uint64_t _pathScore(const unsigned int p) const
	{
		return ( (_paths[p].path->lastIn() + (_paths[p].path->preferenceRank() * (ZT_PEER_PING_PERIOD / ZT_PATH_MAX_PREFERENCE_RANK))) - ((ZT_PEER_PING_PERIOD * 10) * (uint64_t)_paths[p].clusterSuboptimal) );
	}

	unsigned char _key[ZT_PEER_SECRET_KEY_LENGTH];

	const RuntimeEnvironment *RR;
	uint64_t _lastUsed;
	uint64_t _lastReceive; // direct or indirect
	uint64_t _lastUnicastFrame;
	uint64_t _lastMulticastFrame;
	uint64_t _lastAnnouncedTo;
	uint64_t _lastDirectPathPushSent;
	uint64_t _lastDirectPathPushReceive;
	uint16_t _vProto;
	uint16_t _vMajor;
	uint16_t _vMinor;
	uint16_t _vRevision;
	Identity _id;
	struct {
		SharedPtr<Path> path;
		uint64_t lastReceive;
		bool clusterSuboptimal;
	} _paths[ZT_MAX_PEER_NETWORK_PATHS];
	Mutex _paths_m;
	unsigned int _numPaths;
	unsigned int _latency;
	unsigned int _directPathPushCutoffCount;

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
