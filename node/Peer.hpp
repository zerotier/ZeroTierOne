/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2015  ZeroTier, Inc.
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
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

#ifndef ZT_PEER_HPP
#define ZT_PEER_HPP

#include <stdint.h>

#include <algorithm>
#include <utility>
#include <vector>
#include <stdexcept>

#include "../include/ZeroTierOne.h"

#include "Constants.hpp"
#include "RuntimeEnvironment.hpp"
#include "Path.hpp"
#include "Address.hpp"
#include "Utils.hpp"
#include "Identity.hpp"
#include "InetAddress.hpp"
#include "Packet.hpp"
#include "SharedPtr.hpp"
#include "AtomicCounter.hpp"
#include "NonCopyable.hpp"

/**
 * Maximum number of paths a peer can have
 */
#define ZT_PEER_MAX_PATHS 3

namespace ZeroTier {

/**
 * Peer on P2P Network
 *
 * This struture is not locked, volatile, and memcpy-able. NonCopyable
 * semantics are just there to prevent bugs, not because it isn't safe
 * to copy.
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
	 * @param myIdentity Identity of THIS node (for key agreement)
	 * @param peerIdentity Identity of peer
	 * @throws std::runtime_error Key agreement with peer's identity failed
	 */
	Peer(const Identity &myIdentity,const Identity &peerIdentity)
		throw(std::runtime_error);

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
	 * @param RR Runtime environment
	 * @param remoteAddr Internet address of sender
	 * @param linkDesperation Link desperation level
	 * @param hops ZeroTier (not IP) hops
	 * @param packetId Packet ID
	 * @param verb Packet verb
	 * @param inRePacketId Packet ID in reply to (default: none)
	 * @param inReVerb Verb in reply to (for OK/ERROR, default: VERB_NOP)
	 */
	void received(
		const RuntimeEnvironment *RR,
		const InetAddress &remoteAddr,
		int linkDesperation,
		unsigned int hops,
		uint64_t packetId,
		Packet::Verb verb,
		uint64_t inRePacketId = 0,
		Packet::Verb inReVerb = Packet::VERB_NOP);

	/**
	 * Get the best direct path to this peer
	 *
	 * @param now Current time
	 * @return Best path or NULL if there are no active (or fixed) direct paths
	 */
	inline Path *getBestPath(uint64_t now)
	{
		Path *bestPath = (Path *)0;
		uint64_t lrMax = 0;
		for(unsigned int p=0,np=_numPaths;p<np;++p) {
			if ((_paths[p].active(now))&&(_paths[p].lastReceived() >= lrMax)) {
				lrMax = _paths[p].lastReceived();
				bestPath = &(_paths[p]);
			}
		}
		return bestPath;
	}

	/**
	 * Send via best path
	 *
	 * @param RR Runtime environment
	 * @param data Packet data
	 * @param len Packet length
	 * @param now Current time
	 * @return Path used on success or NULL on failure
	 */
	inline Path *send(const RuntimeEnvironment *RR,const void *data,unsigned int len,uint64_t now)
	{
		Path *bestPath = getBestPath(now);
		if (bestPath) {
			if (bestPath->send(RR,data,len,now))
				return bestPath;
		}
		return (Path *)0;
	}

	/**
	 * Send a HELLO to this peer at a specified physical address
	 *
	 * This does not update any statistics. It's used to send initial HELLOs
	 * for NAT traversal and path verification.
	 *
	 * @param RR Runtime environment
	 * @param atAddress Destination address
	 * @param linkDesperation Link desperation
	 * @param now Current time
	 */
	void attemptToContactAt(const RuntimeEnvironment *RR,const InetAddress &atAddress,unsigned int linkDesperation,uint64_t now);

	/**
	 * Send pings or keepalives depending on configured timeouts
	 *
	 * @param RR Runtime environment
	 * @param now Current time
	 */
	void doPingAndKeepalive(const RuntimeEnvironment *RR,uint64_t now);

	/**
	 * @return All known direct paths to this peer
	 */
	std::vector<Path> paths() const
	{
		std::vector<Path> pp;
		for(unsigned int p=0,np=_numPaths;p<np;++p)
			pp.push_back(_paths[p]);
		return pp;
	}

	/**
	 * @return Time of last direct packet receive for any path
	 */
	inline uint64_t lastDirectReceive() const
		throw()
	{
		uint64_t x = 0;
		for(unsigned int p=0,np=_numPaths;p<np;++p)
			x = std::max(x,_paths[p].lastReceived());
		return x;
	}

	/**
	 * @return Time of last direct packet send for any path
	 */
	inline uint64_t lastDirectSend() const
		throw()
	{
		uint64_t x = 0;
		for(unsigned int p=0,np=_numPaths;p<np;++p)
			x = std::max(x,_paths[p].lastSend());
		return x;
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
	 * @return Time we last announced state TO this peer, such as multicast LIKEs
	 */
	inline uint64_t lastAnnouncedTo() const throw() { return _lastAnnouncedTo; }

	/**
	 * @return True if peer has received an actual data frame within ZT_PEER_ACTIVITY_TIMEOUT milliseconds
	 */
	inline uint64_t alive(uint64_t now) const throw() { return ((now - lastFrame()) < ZT_PEER_ACTIVITY_TIMEOUT); }

	/**
	 * @return Current latency or 0 if unknown (max: 65535)
	 */
	inline unsigned int latency() const
		throw()
	{
		unsigned int l = _latency;
		return std::min(l,(unsigned int)65535);
	}

	/**
	 * Update latency with a new direct measurment
	 *
	 * @param l Direct latency measurment in ms
	 */
	inline void addDirectLatencyMeasurment(unsigned int l)
		throw()
	{
		unsigned int ol = _latency;
		if ((ol > 0)&&(ol < 10000))
			_latency = (ol + std::min(l,(unsigned int)65535)) / 2;
		else _latency = std::min(l,(unsigned int)65535);
	}

	/**
	 * @return True if this peer has at least one direct IP address path
	 */
	inline bool hasDirectPath() const throw() { return (_numPaths != 0); }

	/**
	 * @param now Current time
	 * @return True if this peer has at least one active or fixed direct path
	 */
	inline bool hasActiveDirectPath(uint64_t now) const
		throw()
	{
		for(unsigned int p=0,np=_numPaths;p<np;++p) {
			if (_paths[p].active(now))
				return true;
		}
		return false;
	}

	/**
	 * Add a path (if we don't already have it)
	 *
	 * @param p New path to add
	 */
	void addPath(const Path &newp);

	/**
	 * Clear paths
	 *
	 * @param fixedToo If true, clear fixed paths as well as learned ones
	 */
	void clearPaths(bool fixedToo);

	/**
	 * Reset paths within a given scope
	 *
	 * For fixed paths in this scope, a packet is sent. Non-fixed paths in this
	 * scope are forgotten. If there are no paths remaining, a message is sent
	 * indirectly to reestablish connectivity if we're actively exchanging
	 * data with this peer (alive).
	 *
	 * @param RR Runtime environment
	 * @param scope IP scope of paths to reset
	 * @param now Current time
	 */
	void resetWithinScope(const RuntimeEnvironment *RR,InetAddress::IpScope scope,uint64_t now);

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

	/**
	 * Check whether this peer's version is both known and is at least what is specified
	 *
	 * @param major Major version to check against
	 * @param minor Minor version
	 * @param rev Revision
	 * @return True if peer's version is at least supplied tuple
	 */
	inline bool atLeastVersion(unsigned int major,unsigned int minor,unsigned int rev)
		throw()
	{
		if ((_vMajor > 0)||(_vMinor > 0)||(_vRevision > 0)) {
			if (_vMajor > major)
				return true;
			else if (_vMajor == major) {
				if (_vMinor > minor)
					return true;
				else if (_vMinor == minor) {
					if (_vRevision >= rev)
						return true;
				}
			}
		}
		return false;
	}

	inline bool remoteVersionKnown() const throw() { return ((_vMajor > 0)||(_vMinor > 0)||(_vRevision > 0)); }

	/**
	 * Get most recently active UDP path addresses for IPv4 and/or IPv6
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

	/**
	 * Compare Peer version tuples
	 */
	static inline int compareVersion(unsigned int maj1,unsigned int min1,unsigned int rev1,unsigned int maj2,unsigned int min2,unsigned int rev2)
		throw()
	{
		if (maj1 > maj2)
			return 1;
		else if (maj1 < maj2)
			return -1;
		else {
			if (min1 > min2)
				return 1;
			else if (min1 < min2)
				return -1;
			else {
				if (rev1 > rev2)
					return 1;
				else if (rev1 < rev2)
					return -1;
				else return 0;
			}
		}
	}

private:
	void _announceMulticastGroups(const RuntimeEnvironment *RR,uint64_t now);

	unsigned char _key[ZT_PEER_SECRET_KEY_LENGTH];
	uint64_t _lastUsed;
	uint64_t _lastReceive; // direct or indirect
	uint64_t _lastUnicastFrame;
	uint64_t _lastMulticastFrame;
	uint64_t _lastAnnouncedTo;
	uint16_t _vProto;
	uint16_t _vMajor;
	uint16_t _vMinor;
	uint16_t _vRevision;
	Identity _id;
	Path _paths[ZT_PEER_MAX_PATHS];
	unsigned int _numPaths;
	unsigned int _latency;

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
