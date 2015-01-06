/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2011-2015  ZeroTier Networks
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

#include "Constants.hpp"
#include "Path.hpp"
#include "Address.hpp"
#include "Utils.hpp"
#include "Identity.hpp"
#include "Logger.hpp"
#include "RuntimeEnvironment.hpp"
#include "InetAddress.hpp"
#include "Packet.hpp"
#include "SharedPtr.hpp"
#include "Socket.hpp"
#include "AtomicCounter.hpp"
#include "NonCopyable.hpp"

/**
 * Maximum number of paths a peer can have
 */
#define ZT_PEER_MAX_PATHS 8

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
	 * @param fromSock Socket from which packet was received
	 * @param remoteAddr Internet address of sender
	 * @param hops ZeroTier (not IP) hops
	 * @param packetId Packet ID
	 * @param verb Packet verb
	 * @param inRePacketId Packet ID in reply to (for OK/ERROR, 0 otherwise)
	 * @param inReVerb Verb in reply to (for OK/ERROR, VERB_NOP otherwise)
	 * @param now Current time
	 */
	void received(
		const RuntimeEnvironment *RR,
		const SharedPtr<Socket> &fromSock,
		const InetAddress &remoteAddr,
		unsigned int hops,
		uint64_t packetId,
		Packet::Verb verb,
		uint64_t inRePacketId,
		Packet::Verb inReVerb,
		uint64_t now);

	/**
	 * Send a packet directly to this peer
	 *
	 * This sends only via direct paths if available and does not handle
	 * finding of relays. That is done in the send logic in Switch.
	 *
	 * @param RR Runtime environment
	 * @param data Data to send
	 * @param len Length of packet
	 * @param now Current time
	 * @return Type of path used or Path::PATH_TYPE_NULL on failure
	 */
	Path::Type send(const RuntimeEnvironment *RR,const void *data,unsigned int len,uint64_t now);

	/**
	 * Send HELLO to a peer via all direct paths available
	 *
	 * This begins attempting to use TCP paths if no ping response has been
	 * received from any UDP path in more than ZT_TCP_FALLBACK_AFTER.
	 * 
	 * @param RR Runtime environment
	 * @param now Current time
	 * @return True if send appears successful for at least one address type
	 */
	bool sendPing(const RuntimeEnvironment *RR,uint64_t now);

	/**
	 * Called periodically by Topology::clean() to remove stale paths and do other cleanup
	 */
	void clean(uint64_t now);

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
	 * @param addr IP:port
	 * @return True if we have a UDP path to this address
	 */
	inline bool haveUdpPath(const InetAddress &addr) const
	{
		for(unsigned int p=0,np=_numPaths;p<np;++p) {
			if ((_paths[p].type() == Path::PATH_TYPE_UDP)&&(_paths[p].address() == addr))
				return true;
		}
		return false;
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
	 * Get max timestamp of last ping and max timestamp of last receive in a single pass
	 *
	 * @param lp Last ping result parameter (init to 0 before calling)
	 * @param lr Last receive result parameter (init to 0 before calling)
	 */
	inline void lastPingAndDirectReceive(uint64_t &lp,uint64_t &lr)
		throw()
	{
		for(unsigned int p=0,np=_numPaths;p<np;++p) {
			lp = std::max(lp,_paths[p].lastPing());
			lr = std::max(lr,_paths[p].lastReceived());
		}
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
	 * @param now Current time
	 * @return True if peer has received something within ZT_PEER_ACTIVITY_TIMEOUT ms
	 */
	inline bool alive(uint64_t now) const throw() { return ((now - _lastReceive) < ZT_PEER_ACTIVITY_TIMEOUT); }

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
	 * Get most recently active UDP path addresses for IPv4 and/or IPv6
	 *
	 * Note that v4 and v6 are not modified if they are not found, so
	 * initialize these to a NULL address to be able to check.
	 *
	 * @param now Current time
	 * @param v4 Result parameter to receive active IPv4 address, if any
	 * @param v6 Result parameter to receive active IPv6 address, if any
	 */
	void getBestActiveUdpPathAddresses(uint64_t now,InetAddress &v4,InetAddress &v6) const;

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
		b.getBestActiveUdpPathAddresses(now,v4.first,v6.first);
		a.getBestActiveUdpPathAddresses(now,v4.second,v6.second);
		if ((v6.first)&&(v6.second)) // prefer IPv6 if both have it since NAT-t is (almost) unnecessary
			return v6;
		else if ((v4.first)&&(v4.second))
			return v4;
		else return std::pair<InetAddress,InetAddress>();
	}

private:
	void _announceMulticastGroups(const RuntimeEnvironment *RR,uint64_t now);

	volatile uint64_t _lastUsed;
	volatile uint64_t _lastReceive; // direct or indirect
	volatile uint64_t _lastUnicastFrame;
	volatile uint64_t _lastMulticastFrame;
	volatile uint64_t _lastAnnouncedTo;
	volatile uint16_t _vProto;
	volatile uint16_t _vMajor;
	volatile uint16_t _vMinor;
	volatile uint16_t _vRevision;

	Path _paths[ZT_PEER_MAX_PATHS];
	volatile unsigned int _numPaths;

	volatile unsigned int _latency;
	unsigned char _key[ZT_PEER_SECRET_KEY_LENGTH];
	Identity _id;

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
