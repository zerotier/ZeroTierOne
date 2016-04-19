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
#include "CertificateOfMembership.hpp"
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

// Very rough computed estimate: (8 + 256 + 80 + (16 * 64) + (128 * 256) + (128 * 16))
// 1048576 provides tons of headroom -- overflow would just cause peer not to be persisted
#define ZT_PEER_SUGGESTED_SERIALIZATION_BUFFER_SIZE 1048576

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
	 * @param RR Runtime environment
	 * @param localAddr Local address
	 * @param remoteAddr Internet address of sender
	 * @param hops ZeroTier (not IP) hops
	 * @param packetId Packet ID
	 * @param verb Packet verb
	 * @param inRePacketId Packet ID in reply to (default: none)
	 * @param inReVerb Verb in reply to (for OK/ERROR, default: VERB_NOP)
	 */
	void received(
		const InetAddress &localAddr,
		const InetAddress &remoteAddr,
		unsigned int hops,
		uint64_t packetId,
		Packet::Verb verb,
		uint64_t inRePacketId = 0,
		Packet::Verb inReVerb = Packet::VERB_NOP);

	/**
	 * Get the current best direct path to this peer
	 *
	 * @param now Current time
	 * @return Best path or NULL if there are no active direct paths
	 */
	inline Path *getBestPath(uint64_t now) { return _getBestPath(now); }

	/**
	 * @param now Current time
	 * @param addr Remote address
	 * @return True if we have an active path to this destination
	 */
	inline bool hasActivePathTo(uint64_t now,const InetAddress &addr) const
	{
		for(unsigned int p=0;p<_numPaths;++p) {
			if ((_paths[p].active(now))&&(_paths[p].address() == addr))
				return true;
		}
		return false;
	}

	/**
	 * Set all paths in the same ss_family that are not this one to cluster suboptimal
	 *
	 * Addresses in other families are not affected.
	 *
	 * @param addr Address to make exclusive
	 */
	inline void setClusterOptimalPathForAddressFamily(const InetAddress &addr)
	{
		for(unsigned int p=0;p<_numPaths;++p) {
			if (_paths[p].address().ss_family == addr.ss_family) {
				_paths[p].setClusterSuboptimal(_paths[p].address() != addr);
			}
		}
	}

	/**
	 * Send via best path
	 *
	 * @param data Packet data
	 * @param len Packet length
	 * @param now Current time
	 * @return Path used on success or NULL on failure
	 */
	inline Path *send(const void *data,unsigned int len,uint64_t now)
	{
		Path *const bestPath = getBestPath(now);
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
	 * @param localAddr Local address
	 * @param atAddress Destination address
	 * @param now Current time
	 * @param ttl Desired IP TTL (default: 0 to leave alone)
	 */
	void sendHELLO(const InetAddress &localAddr,const InetAddress &atAddress,uint64_t now,unsigned int ttl = 0);

	/**
	 * Send pings or keepalives depending on configured timeouts
	 *
	 * @param now Current time
	 * @param inetAddressFamily Keep this address family alive, or 0 to simply pick current best ignoring family
	 * @return True if at least one direct path seems alive
	 */
	bool doPingAndKeepalive(uint64_t now,int inetAddressFamily);

	/**
	 * Push direct paths back to self if we haven't done so in the configured timeout
	 *
	 * @param localAddr Local address
	 * @param toAddress Remote address to send push to (usually from path)
	 * @param now Current time
	 * @param force If true, push regardless of rate limit
	 * @param includePrivatePaths If true, include local interface address paths (should only be done to peers with a trust relationship)
	 * @return True if something was actually sent
	 */
	bool pushDirectPaths(const InetAddress &localAddr,const InetAddress &toAddress,uint64_t now,bool force,bool includePrivatePaths);

	/**
	 * @return All known direct paths to this peer (active or inactive)
	 */
	inline std::vector<Path> paths() const
	{
		std::vector<Path> pp;
		for(unsigned int p=0,np=_numPaths;p<np;++p)
			pp.push_back(_paths[p]);
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
		return (l * (((unsigned int)tsr / (ZT_PEER_DIRECT_PING_DELAY + 1000)) + 1));
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

	/**
	 * @param now Current time
	 * @return True if this peer has at least one active direct path
	 */
	inline bool hasActiveDirectPath(uint64_t now) const
	{
		for(unsigned int p=0;p<_numPaths;++p) {
			if (_paths[p].active(now))
				return true;
		}
		return false;
	}

#ifdef ZT_ENABLE_CLUSTER
	/**
	 * @param now Current time
	 * @return True if this peer has at least one active direct path that is not cluster-suboptimal
	 */
	inline bool hasClusterOptimalPath(uint64_t now) const
	{
		for(unsigned int p=0,np=_numPaths;p<np;++p) {
			if ((_paths[p].active(now))&&(!_paths[p].isClusterSuboptimal()))
				return true;
		}
		return false;
	}
#endif

	/**
	 * Reset paths within a given scope
	 *
	 * @param scope IP scope of paths to reset
	 * @param now Current time
	 * @return True if at least one path was forgotten
	 */
	bool resetWithinScope(InetAddress::IpScope scope,uint64_t now);

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
	 * Check network COM agreement with this peer
	 *
	 * @param nwid Network ID
	 * @param com Another certificate of membership
	 * @return True if supplied COM agrees with ours, false if not or if we don't have one
	 */
	bool networkMembershipCertificatesAgree(uint64_t nwid,const CertificateOfMembership &com) const;

	/**
	 * Check the validity of the COM and add/update if valid and new
	 *
	 * @param nwid Network ID
	 * @param com Externally supplied COM
	 */
	bool validateAndSetNetworkMembershipCertificate(uint64_t nwid,const CertificateOfMembership &com);

	/**
	 * @param nwid Network ID
	 * @param now Current time
	 * @param updateLastPushedTime If true, go ahead and update the last pushed time regardless of return value
	 * @return Whether or not this peer needs another COM push from us
	 */
	bool needsOurNetworkMembershipCertificate(uint64_t nwid,uint64_t now,bool updateLastPushedTime);

	/**
	 * Perform periodic cleaning operations
	 *
	 * @param now Current time
	 */
	void clean(uint64_t now);

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

	template<unsigned int C>
	inline void serialize(Buffer<C> &b) const
	{
		Mutex::Lock _l(_networkComs_m);

		const unsigned int recSizePos = b.size();
		b.addSize(4); // space for uint32_t field length

		b.append((uint16_t)1); // version of serialized Peer data

		_id.serialize(b,false);

		b.append((uint64_t)_lastUsed);
		b.append((uint64_t)_lastReceive);
		b.append((uint64_t)_lastUnicastFrame);
		b.append((uint64_t)_lastMulticastFrame);
		b.append((uint64_t)_lastAnnouncedTo);
		b.append((uint64_t)_lastDirectPathPushSent);
		b.append((uint64_t)_lastDirectPathPushReceive);
		b.append((uint64_t)_lastPathSort);
		b.append((uint16_t)_vProto);
		b.append((uint16_t)_vMajor);
		b.append((uint16_t)_vMinor);
		b.append((uint16_t)_vRevision);
		b.append((uint32_t)_latency);
		b.append((uint16_t)_directPathPushCutoffCount);

		b.append((uint16_t)_numPaths);
		for(unsigned int i=0;i<_numPaths;++i)
			_paths[i].serialize(b);

		b.append((uint32_t)_networkComs.size());
		{
			uint64_t *k = (uint64_t *)0;
			_NetworkCom *v = (_NetworkCom *)0;
			Hashtable<uint64_t,_NetworkCom>::Iterator i(const_cast<Peer *>(this)->_networkComs);
			while (i.next(k,v)) {
				b.append((uint64_t)*k);
				b.append((uint64_t)v->ts);
				v->com.serialize(b);
			}
		}

		b.append((uint32_t)_lastPushedComs.size());
		{
			uint64_t *k = (uint64_t *)0;
			uint64_t *v = (uint64_t *)0;
			Hashtable<uint64_t,uint64_t>::Iterator i(const_cast<Peer *>(this)->_lastPushedComs);
			while (i.next(k,v)) {
				b.append((uint64_t)*k);
				b.append((uint64_t)*v);
			}
		}

		b.template setAt<uint32_t>(recSizePos,(uint32_t)(b.size() - (recSizePos + 4))); // set size
	}

	/**
	 * Create a new Peer from a serialized instance
	 *
	 * @param renv Runtime environment
	 * @param myIdentity This node's identity
	 * @param b Buffer containing serialized Peer data
	 * @param p Pointer to current position in buffer, will be updated in place as buffer is read (value/result)
	 * @return New instance of Peer or NULL if serialized data was corrupt or otherwise invalid (may also throw an exception via Buffer)
	 */
	template<unsigned int C>
	static inline SharedPtr<Peer> deserializeNew(const RuntimeEnvironment *renv,const Identity &myIdentity,const Buffer<C> &b,unsigned int &p)
	{
		const unsigned int recSize = b.template at<uint32_t>(p); p += 4;
		if ((p + recSize) > b.size())
			return SharedPtr<Peer>(); // size invalid
		if (b.template at<uint16_t>(p) != 1)
			return SharedPtr<Peer>(); // version mismatch
		p += 2;

		Identity npid;
		p += npid.deserialize(b,p);
		if (!npid)
			return SharedPtr<Peer>();

		SharedPtr<Peer> np(new Peer(renv,myIdentity,npid));

		np->_lastUsed = b.template at<uint64_t>(p); p += 8;
		np->_lastReceive = b.template at<uint64_t>(p); p += 8;
		np->_lastUnicastFrame = b.template at<uint64_t>(p); p += 8;
		np->_lastMulticastFrame = b.template at<uint64_t>(p); p += 8;
		np->_lastAnnouncedTo = b.template at<uint64_t>(p); p += 8;
		np->_lastDirectPathPushSent = b.template at<uint64_t>(p); p += 8;
		np->_lastDirectPathPushReceive = b.template at<uint64_t>(p); p += 8;
		np->_lastPathSort = b.template at<uint64_t>(p); p += 8;
		np->_vProto = b.template at<uint16_t>(p); p += 2;
		np->_vMajor = b.template at<uint16_t>(p); p += 2;
		np->_vMinor = b.template at<uint16_t>(p); p += 2;
		np->_vRevision = b.template at<uint16_t>(p); p += 2;
		np->_latency = b.template at<uint32_t>(p); p += 4;
		np->_directPathPushCutoffCount = b.template at<uint16_t>(p); p += 2;

		const unsigned int numPaths = b.template at<uint16_t>(p); p += 2;
		for(unsigned int i=0;i<numPaths;++i) {
			if (i < ZT_MAX_PEER_NETWORK_PATHS) {
				p += np->_paths[np->_numPaths++].deserialize(b,p);
			} else {
				// Skip any paths beyond max, but still read stream
				Path foo;
				p += foo.deserialize(b,p);
			}
		}

		const unsigned int numNetworkComs = b.template at<uint32_t>(p); p += 4;
		for(unsigned int i=0;i<numNetworkComs;++i) {
			_NetworkCom &c = np->_networkComs[b.template at<uint64_t>(p)]; p += 8;
			c.ts = b.template at<uint64_t>(p); p += 8;
			p += c.com.deserialize(b,p);
		}

		const unsigned int numLastPushed = b.template at<uint32_t>(p); p += 4;
		for(unsigned int i=0;i<numLastPushed;++i) {
			const uint64_t nwid = b.template at<uint64_t>(p); p += 8;
			const uint64_t ts = b.template at<uint64_t>(p); p += 8;
			np->_lastPushedComs.set(nwid,ts);
		}

		return np;
	}

private:
	void _doDeadPathDetection(Path &p,const uint64_t now);
	Path *_getBestPath(const uint64_t now);
	Path *_getBestPath(const uint64_t now,int inetAddressFamily);

	unsigned char _key[ZT_PEER_SECRET_KEY_LENGTH]; // computed with key agreement, not serialized

	const RuntimeEnvironment *RR;
	uint64_t _lastUsed;
	uint64_t _lastReceive; // direct or indirect
	uint64_t _lastUnicastFrame;
	uint64_t _lastMulticastFrame;
	uint64_t _lastAnnouncedTo;
	uint64_t _lastDirectPathPushSent;
	uint64_t _lastDirectPathPushReceive;
	uint64_t _lastPathSort;
	uint16_t _vProto;
	uint16_t _vMajor;
	uint16_t _vMinor;
	uint16_t _vRevision;
	Identity _id;
	Path _paths[ZT_MAX_PEER_NETWORK_PATHS];
	unsigned int _numPaths;
	unsigned int _latency;
	unsigned int _directPathPushCutoffCount;

	struct _NetworkCom
	{
		_NetworkCom() {}
		_NetworkCom(uint64_t t,const CertificateOfMembership &c) : ts(t),com(c) {}
		uint64_t ts;
		CertificateOfMembership com;
	};
	Hashtable<uint64_t,_NetworkCom> _networkComs;
	Hashtable<uint64_t,uint64_t> _lastPushedComs;
	Mutex _networkComs_m;

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
