/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2011-2014  ZeroTier Networks LLC
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

#include <vector>
#include <algorithm>
#include <utility>
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
#include "Mutex.hpp"

#define ZT_PEER_SERIALIZATION_VERSION 8

namespace ZeroTier {

/**
 * Peer on P2P Network
 */
class Peer : NonCopyable
{
	friend class SharedPtr<Peer>;

public:
	/**
	 * Construct an uninitialized peer (used with deserialize())
	 */
	Peer();

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
	inline uint64_t lastUsed() const
		throw()
	{
		return _lastUsed;
	}

	/**
	 * @param now New time of last use
	 */
	inline void setLastUsed(uint64_t now)
		throw()
	{
		_lastUsed = now;
	}

	/**
	 * @return This peer's ZT address (short for identity().address())
	 */
	inline const Address &address() const throw() { return _id.address(); }

	/**
	 * @return This peer's identity
	 */
	inline const Identity &identity() const throw() { return _id; }

	/**
	 * Must be called on authenticated packet receive from this peer
	 * 
	 * @param _r Runtime environment
	 * @param fromSock Socket from which packet was received
	 * @param remoteAddr Internet address of sender
	 * @param hops ZeroTier (not IP) hops
	 * @param packetId Packet ID
	 * @param verb Packet verb
	 * @param inRePacketId Packet ID in reply to (for OK/ERROR, 0 otherwise)
	 * @param inReVerb Verb in reply to (for OK/ERROR, VERB_NOP otherwise)
	 * @param now Current time
	 */
	void onReceive(
		const RuntimeEnvironment *_r,
		const SharedPtr<Socket> &fromSock,
		const InetAddress &remoteAddr,
		unsigned int hops,
		uint64_t packetId,
		Packet::Verb verb,
		uint64_t inRePacketId,
		Packet::Verb inReVerb,
		uint64_t now);

	/**
	 * Send a packet to this peer using the most recently active direct path
	 *
	 * This does not relay. It returns false if there are no available active
	 * paths.
	 *
	 * @param _r Runtime environment
	 * @param data Data to send
	 * @param len Length of packet
	 * @param now Current time
	 * @return True if packet appears to have been sent, false if no path or other error
	 */
	bool send(const RuntimeEnvironment *_r,const void *data,unsigned int len,uint64_t now);

	/**
	 * Send firewall opener to all UDP paths
	 * 
	 * @param _r Runtime environment
	 * @param now Current time
	 * @return True if send appears successful for at least one address type
	 */
	bool sendFirewallOpener(const RuntimeEnvironment *_r,uint64_t now);

	/**
	 * Send HELLO to a peer via all direct paths available
	 *
	 * This begins attempting to use TCP paths if no ping response has been
	 * received from any UDP path in more than ZT_TCP_FALLBACK_AFTER.
	 * 
	 * @param _r Runtime environment
	 * @param now Current time
	 * @param firstSinceReset If true, this is the first ping sent since a network reset
	 * @return True if send appears successful for at least one address type
	 */
	bool sendPing(const RuntimeEnvironment *_r,uint64_t now,bool firstSinceReset);

	/**
	 * @return All known direct paths to this peer
	 */
	std::vector<Path> paths() const
	{
		Mutex::Lock _l(_lock);
		return _paths;
	}

	/**
	 * @return Last successfully sent firewall opener for any path
	 */
	inline uint64_t lastFirewallOpener() const
		throw()
	{
		uint64_t x = 0;
		Mutex::Lock _l(_lock);
		for(std::vector<Path>::const_iterator p(_paths.begin());p!=_paths.end();++p) {
			uint64_t l = p->lastFirewallOpener();
			if (l > x)
				x = l;
		}
		return x;
	}

	/**
	 * @return Time of last direct packet receive for any path
	 */
	inline uint64_t lastDirectReceive() const
		throw()
	{
		uint64_t x = 0;
		Mutex::Lock _l(_lock);
		for(std::vector<Path>::const_iterator p(_paths.begin());p!=_paths.end();++p) {
			uint64_t l = p->lastReceived();
			if (l > x)
				x = l;
		}
		return x;
	}

	/**
	 * @return Time of last direct packet send for any path
	 */
	inline uint64_t lastDirectSend() const
		throw()
	{
		uint64_t x = 0;
		Mutex::Lock _l(_lock);
		for(std::vector<Path>::const_iterator p(_paths.begin());p!=_paths.end();++p) {
			uint64_t l = p->lastSend();
			if (l > x)
				x = l;
		}
		return x;
	}

	/**
	 * @return Time of most recent unicast frame received
	 */
	inline uint64_t lastUnicastFrame() const
		throw()
	{
		return _lastUnicastFrame;
	}

	/**
	 * @return Time of most recent multicast frame received
	 */
	inline uint64_t lastMulticastFrame() const
		throw()
	{
		return _lastMulticastFrame;
	}

	/**
	 * @return Time of most recent frame of any kind (unicast or multicast)
	 */
	inline uint64_t lastFrame() const
		throw()
	{
		return std::max(_lastUnicastFrame,_lastMulticastFrame);
	}

	/**
	 * @return Time we last announced state TO this peer, such as multicast LIKEs
	 */
	inline uint64_t lastAnnouncedTo() const
		throw()
	{
		return _lastAnnouncedTo;
	}

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
		if (l > 65535) l = 65535;
		unsigned int ol = _latency;
		if ((ol > 0)&&(ol < 10000))
			_latency = (ol + l) / 2;
		else _latency = l;
	}

	/**
	 * @return True if this peer has at least one direct IP address path
	 */
	inline bool hasDirectPath() const
		throw()
	{
		Mutex::Lock _l(_lock);
		return (!_paths.empty());
	}

	/**
	 * @param now Current time
	 * @return True if this peer has at least one active or fixed direct path
	 */
	inline bool hasActiveDirectPath(uint64_t now) const
		throw()
	{
		Mutex::Lock _l(_lock);
		for(std::vector<Path>::const_iterator p(_paths.begin());p!=_paths.end();++p) {
			if (p->active(now))
				return true;
		}
		return false;
	}

	/**
	 * Add a path (if we don't already have it)
	 *
	 * @param p New path to add
	 */
	inline void addPath(const Path &newp)
	{
		Mutex::Lock _l(_lock);
		for(std::vector<Path>::iterator p(_paths.begin());p!=_paths.end();++p) {
			if (*p == newp) {
				p->setFixed(newp.fixed());
				return;
			}
		}
		_paths.push_back(newp);
	}

	/**
	 * Clear paths
	 *
	 * @param fixedToo If true, clear fixed paths as well as learned ones
	 */
	inline void clearPaths(bool fixedToo)
	{
		std::vector<Path> npv;
		Mutex::Lock _l(_lock);
		if (!fixedToo) {
			for(std::vector<Path>::const_iterator p(_paths.begin());p!=_paths.end();++p) {
				if (p->fixed())
					npv.push_back(*p);
			}
		}
		_paths = npv;
	}

	/**
	 * @return 256-bit secret symmetric encryption key
	 */
	inline const unsigned char *key() const throw() { return _key; }

	/**
	 * Set the currently known remote version of this peer's client
	 *
	 * @param vmaj Major version
	 * @param vmin Minor version
	 * @param vrev Revision
	 */
	inline void setRemoteVersion(unsigned int vmaj,unsigned int vmin,unsigned int vrev)
	{
		_vMajor = vmaj;
		_vMinor = vmin;
		_vRevision = vrev;
	}

	/**
	 * @return Remote version in string form or '?' if unknown
	 */
	inline std::string remoteVersion() const
	{
		if ((_vMajor)||(_vMinor)||(_vRevision)) {
			char tmp[32];
			Utils::snprintf(tmp,sizeof(tmp),"%u.%u.%u",_vMajor,_vMinor,_vRevision);
			return std::string(tmp);
		}
		return std::string("?");
	}

	/**
	 * @return True if this Peer is initialized with something
	 */
	inline operator bool() const throw() { return (_id); }

	/**
	 * @param now Current time
	 * @param v4 Result parameter to receive active IPv4 address, if any
	 * @param v6 Result parameter to receive active IPv6 address, if any
	 */
	inline void getActiveUdpPathAddresses(uint64_t now,InetAddress &v4,InetAddress &v6) const
	{
		bool gotV4 = false,gotV6 = false;
		Mutex::Lock _l(_lock);
		for(std::vector<Path>::const_iterator p(_paths.begin());p!=_paths.end();++p) {
			if (!gotV4) {
				if ((!p->tcp())&&(p->address().isV4())&&(p->active(now))) {
					gotV4 = true;
					v4 = p->address();
				}
			} else if (!gotV6) {
				if ((!p->tcp())&&(p->address().isV6())&&(p->active(now))) {
					gotV6 = true;
					v6 = p->address();
				}
			} else break;
		}
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
		throw()
	{
		std::pair<InetAddress,InetAddress> v4,v6;
		b.getActiveUdpPathAddresses(now,v4.first,v6.first);
		a.getActiveUdpPathAddresses(now,v4.second,v6.second);
		if ((v6.first)&&(v6.second))
			return v6;
		if ((v4.first)&&(v4.second))
			return v4;
		return std::pair<InetAddress,InetAddress>();
	}

	template<unsigned int C>
	inline void serialize(Buffer<C> &b) const
	{
		Mutex::Lock _l(_lock);

		b.append((unsigned char)ZT_PEER_SERIALIZATION_VERSION);
		_id.serialize(b,false);
		b.append(_key,sizeof(_key));
		b.append(_lastUsed);
		b.append(_lastUnicastFrame);
		b.append(_lastMulticastFrame);
		b.append(_lastAnnouncedTo);
		b.append((uint16_t)_vMajor);
		b.append((uint16_t)_vMinor);
		b.append((uint16_t)_vRevision);
		b.append((uint16_t)_latency);
		b.append((uint16_t)_paths.size());
		for(std::vector<Path>::const_iterator p(_paths.begin());p!=_paths.end();++p)
			p->serialize(b);
	}
	template<unsigned int C>
	inline unsigned int deserialize(const Buffer<C> &b,unsigned int startAt = 0)
	{
		unsigned int p = startAt;

		if (b[p++] != ZT_PEER_SERIALIZATION_VERSION)
			throw std::invalid_argument("Peer: deserialize(): version mismatch");

		Mutex::Lock _l(_lock);

		p += _id.deserialize(b,p);
		memcpy(_key,b.field(p,sizeof(_key)),sizeof(_key)); p += sizeof(_key);
		_lastUsed = b.template at<uint64_t>(p); p += sizeof(uint64_t);
		_lastUnicastFrame = b.template at<uint64_t>(p); p += sizeof(uint64_t);
		_lastMulticastFrame = b.template at<uint64_t>(p); p += sizeof(uint64_t);
		_lastAnnouncedTo = b.template at<uint64_t>(p); p += sizeof(uint64_t);
		_vMajor = b.template at<uint16_t>(p); p += sizeof(uint16_t);
		_vMinor = b.template at<uint16_t>(p); p += sizeof(uint16_t);
		_vRevision = b.template at<uint16_t>(p); p += sizeof(uint16_t);
		_latency = b.template at<uint16_t>(p); p += sizeof(uint16_t);
		unsigned int npaths = (unsigned int)b.template at<uint16_t>(p); p += sizeof(uint16_t);
		_paths.clear();
		for(unsigned int i=0;i<npaths;++i) {
			_paths.push_back(Path());
			p += _paths.back().deserialize(b,p);
		}

		return (p - startAt);
	}

private:
	unsigned char _key[ZT_PEER_SECRET_KEY_LENGTH];
	Identity _id;

	std::vector<Path> _paths;

	volatile uint64_t _lastUsed;
	volatile uint64_t _lastUnicastFrame;
	volatile uint64_t _lastMulticastFrame;
	volatile uint64_t _lastAnnouncedTo;
	volatile unsigned int _vMajor;
	volatile unsigned int _vMinor;
	volatile unsigned int _vRevision;
	volatile unsigned int _latency;

	Mutex _lock;

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
