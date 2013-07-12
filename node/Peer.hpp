/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2012-2013  ZeroTier Networks LLC
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

#ifndef _ZT_PEER_HPP
#define _ZT_PEER_HPP

#include <algorithm>
#include <utility>
#include <stdexcept>
#include <stdint.h>
#include "Address.hpp"
#include "Utils.hpp"
#include "Identity.hpp"
#include "Constants.hpp"
#include "Logger.hpp"
#include "Demarc.hpp"
#include "RuntimeEnvironment.hpp"
#include "InetAddress.hpp"
#include "EllipticCurveKey.hpp"
#include "Packet.hpp"
#include "SharedPtr.hpp"
#include "AtomicCounter.hpp"
#include "NonCopyable.hpp"
#include "Mutex.hpp"

/**
 * Max length of serialized peer record
 */
#define ZT_PEER_MAX_SERIALIZED_LENGTH ( \
	64 + \
	IDENTITY_MAX_BINARY_SERIALIZED_LENGTH + \
	(( \
		(sizeof(uint64_t) * 5) + \
		sizeof(uint16_t) + \
		1 + \
		sizeof(uint16_t) + \
		16 + \
		1 \
	) * 2) + \
	64 \
)

namespace ZeroTier {

/**
 * A peer on the network
 * 
 * Threading note:
 *
 * This structure contains no locks at the moment, but also performs no
 * memory allocation or pointer manipulation. As a result is is technically
 * "safe" for threads, as in won't crash. Right now it's only changed from
 * the core I/O thread so this isn't an issue. If multiple I/O threads are
 * introduced it ought to have a lock of some kind.
 */
class Peer : NonCopyable
{
	friend class SharedPtr<Peer>;

private:
	~Peer() {}

public:
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
	 * @param localPort Local port on which packet was received
	 * @param fromAddr Internet address of sender
	 * @param hops ZeroTier (not IP) hops
	 * @param verb Packet verb
	 * @param now Current time
	 */
	void onReceive(const RuntimeEnvironment *_r,Demarc::Port localPort,const InetAddress &fromAddr,unsigned int hops,Packet::Verb verb,uint64_t now);

	/**
	 * Send a UDP packet to this peer
	 * 
	 * If the active link is timed out (no receives for ping timeout ms), then
	 * the active link number is incremented after send. This causes sends to
	 * cycle through links if there is no clear active link. This also happens
	 * if the send fails for some reason.
	 * 
	 * @param _r Runtime environment
	 * @param data Data to send
	 * @param len Length of packet
	 * @param relay This is a relay on behalf of another peer (verb is ignored)
	 * @param verb Packet verb (if not relay)
	 * @param now Current time
	 * @return True if packet appears to have been sent, false on local failure
	 */
	bool send(const RuntimeEnvironment *_r,const void *data,unsigned int len,bool relay,Packet::Verb verb,uint64_t now);

	/**
	 * Send firewall opener to active link
	 * 
	 * @param _r Runtime environment
	 * @param now Current time
	 * @return True if send appears successful for at least one address type
	 */
	bool sendFirewallOpener(const RuntimeEnvironment *_r,uint64_t now);

	/**
	 * Set an address to reach this peer
	 *
	 * @param addr Address to set
	 * @param fixed If true, address is fixed (won't be changed on packet receipt)
	 */
	void setPathAddress(const InetAddress &addr,bool fixed);

	/**
	 * Clear the fixed flag for an address type
	 *
	 * @param t Type to clear, or TYPE_NULL to clear flag on all types
	 */
	void clearFixedFlag(InetAddress::AddressType t);

	/**
	 * @return Last successfully sent firewall opener
	 */
	uint64_t lastFirewallOpener() const
		throw()
	{
		return std::max(_ipv4p.lastFirewallOpener,_ipv6p.lastFirewallOpener);
	}

	/**
	 * @return Time of last direct packet receive
	 */
	uint64_t lastDirectReceive() const
		throw()
	{
		return std::max(_ipv4p.lastReceive,_ipv6p.lastReceive);
	}

	/**
	 * @return Time of last direct packet send
	 */
	uint64_t lastDirectSend() const
		throw()
	{
		return std::max(_ipv4p.lastSend,_ipv6p.lastSend);
	}

	/**
	 * @return Time of most recent unicast frame (actual data transferred)
	 */
	uint64_t lastUnicastFrame() const
		throw()
	{
		return std::max(_ipv4p.lastUnicastFrame,_ipv6p.lastUnicastFrame);
	}

	/**
	 * @return Lowest of measured latencies of all paths or 0 if unknown
	 */
	unsigned int latency() const
		throw()
	{
		if (_ipv4p.latency) {
			if (_ipv6p.latency)
				return std::min(_ipv4p.latency,_ipv6p.latency);
			else return _ipv4p.latency;
		} else if (_ipv6p.latency)
			return _ipv6p.latency;
		return 0;
	}

	/**
	 * @param addr Remote address
	 * @param latency Latency measurment
	 */
	void setLatency(const InetAddress &addr,unsigned int latency)
	{
		if (addr == _ipv4p.addr) {
			_ipv4p.latency = latency;
			_dirty = true;
		} else if (addr == _ipv6p.addr) {
			_ipv6p.latency = latency;
			_dirty = true;
		}
	}

	/**
	 * @return True if this peer has at least one direct IP address path
	 */
	inline bool hasDirectPath() const
		throw()
	{
		return ((_ipv4p.addr)||(_ipv6p.addr));
	}

	/**
	 * @param now Current time
	 * @return True if hasDirectPath() is true and at least one path is active
	 */
	inline bool hasActiveDirectPath(uint64_t now) const
		throw()
	{
		return ((_ipv4p.isActive(now))||(_ipv6p.isActive(now)));
	}

	/**
	 * @return IPv4 direct address or null InetAddress if none
	 */
	inline InetAddress ipv4Path() const
		throw()
	{
		return _ipv4p.addr;
	}

	/**
	 * @return IPv6 direct address or null InetAddress if none
	 */
	inline InetAddress ipv6Path() const
		throw()
	{
		return _ipv4p.addr;
	}

	/**
	 * @return IPv4 direct address or null InetAddress if none
	 */
	inline InetAddress ipv4ActivePath(uint64_t now) const
		throw()
	{
		if (_ipv4p.isActive(now))
			return _ipv4p.addr;
		return InetAddress();
	}

	/**
	 * @return IPv6 direct address or null InetAddress if none
	 */
	inline InetAddress ipv6ActivePath(uint64_t now) const
		throw()
	{
		if (_ipv6p.isActive(now))
			return _ipv6p.addr;
		return InetAddress();
	}

	/**
	 * @return 256-bit encryption key
	 */
	inline const unsigned char *cryptKey() const
		throw()
	{
		return _keys; // crypt key is first 32-byte key
	}

	/**
	 * @return 256-bit MAC (message authentication code) key
	 */
	inline const unsigned char *macKey() const
		throw()
	{
		return (_keys + 32); // mac key is second 32-byte key
	}

	/**
	 * Set the remote version of the peer (not persisted)
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
	 * Get and reset dirty flag
	 * 
	 * @return Previous value of dirty flag before reset
	 */
	inline bool getAndResetDirty()
		throw()
	{
		bool d = _dirty;
		_dirty = false;
		return d;
	}

	/**
	 * @return Current value of dirty flag
	 */
	inline bool dirty() const throw() { return _dirty; }

	template<unsigned int C>
	inline void serialize(Buffer<C> &b)
		throw(std::out_of_range)
	{
		b.append((unsigned char)1); // version
		b.append(_keys,sizeof(_keys));
		_id.serialize(b,false);
		_ipv4p.serialize(b);
		_ipv6p.serialize(b);
	}

	template<unsigned int C>
	inline unsigned int deserialize(const Buffer<C> &b,unsigned int startAt = 0)
		throw(std::out_of_range,std::invalid_argument)
	{
		unsigned int p = startAt;

		if (b[p++] != 1)
			throw std::invalid_argument("Peer: deserialize(): version mismatch");

		memcpy(_keys,b.field(p,sizeof(_keys)),sizeof(_keys)); p += sizeof(_keys);
		p += _id.deserialize(b,p);
		p += _ipv4p.deserialize(b,p);
		p += _ipv6p.deserialize(b,p);

		_dirty = false;

		return (p - startAt);
	}

	/**
	 * @return True if this Peer is initialized with something
	 */
	inline operator bool() const throw() { return (_id); }

	/**
	 * Find a common set of addresses by which two peers can link, if any
	 *
	 * @param a Peer A
	 * @param b Peer B
	 * @param now Current time
	 * @return Pair: B's address to send to A, A's address to send to B
	 */
	static inline std::pair<InetAddress,InetAddress> findCommonGround(const Peer &a,const Peer &b,uint64_t now)
		throw()
	{
		if ((a._ipv6p.isActive(now))&&(b._ipv6p.isActive(now)))
			return std::pair<InetAddress,InetAddress>(b._ipv6p.addr,a._ipv6p.addr);
		else if ((a._ipv4p.isActive(now))&&(b._ipv4p.isActive(now)))
			return std::pair<InetAddress,InetAddress>(b._ipv4p.addr,a._ipv4p.addr);
		else if ((a._ipv6p.addr)&&(b._ipv6p.addr))
			return std::pair<InetAddress,InetAddress>(b._ipv6p.addr,a._ipv6p.addr);
		else if ((a._ipv4p.addr)&&(b._ipv4p.addr))
			return std::pair<InetAddress,InetAddress>(b._ipv4p.addr,a._ipv4p.addr);
		return std::pair<InetAddress,InetAddress>();
	}

private:
	class WanPath
	{
	public:
		WanPath() :
			lastSend(0),
			lastReceive(0),
			lastUnicastFrame(0),
			lastFirewallOpener(0),
			localPort(Demarc::ANY_PORT),
			latency(0),
			addr(),
			fixed(false)
		{
		}

		inline bool isActive(const uint64_t now) const
			throw()
		{
			return ((addr)&&((now - lastReceive) < ZT_PEER_LINK_ACTIVITY_TIMEOUT));
		}

		template<unsigned int C>
		inline void serialize(Buffer<C> &b)
			throw(std::out_of_range)
		{
			b.append(lastSend);
			b.append(lastReceive);
			b.append(lastUnicastFrame);
			b.append(lastFirewallOpener);
			b.append(Demarc::portToInt(localPort));
			b.append((uint16_t)latency);

			b.append((unsigned char)addr.type());
			switch(addr.type()) {
				case InetAddress::TYPE_NULL:
					break;
				case InetAddress::TYPE_IPV4:
					b.append(addr.rawIpData(),4);
					b.append((uint16_t)addr.port());
					break;
				case InetAddress::TYPE_IPV6:
					b.append(addr.rawIpData(),16);
					b.append((uint16_t)addr.port());
					break;
			}

			b.append(fixed ? (unsigned char)1 : (unsigned char)0);
		}

		template<unsigned int C>
		inline unsigned int deserialize(const Buffer<C> &b,unsigned int startAt = 0)
			throw(std::out_of_range,std::invalid_argument)
		{
			unsigned int p = startAt;

			lastSend = b.template at<uint64_t>(p); p += sizeof(uint64_t);
			lastReceive = b.template at<uint64_t>(p); p += sizeof(uint64_t);
			lastUnicastFrame = b.template at<uint64_t>(p); p += sizeof(uint64_t);
			lastFirewallOpener = b.template at<uint64_t>(p); p += sizeof(uint64_t);
			localPort = Demarc::intToPort(b.template at<uint64_t>(p)); p += sizeof(uint64_t);
			latency = b.template at<uint16_t>(p); p += sizeof(uint16_t);

			switch ((InetAddress::AddressType)b[p++]) {
				case InetAddress::TYPE_NULL:
					addr.zero();
					break;
				case InetAddress::TYPE_IPV4:
					addr.set(b.field(p,4),4,b.template at<uint16_t>(p + 4));
					p += 4 + sizeof(uint16_t);
					break;
				case InetAddress::TYPE_IPV6:
					addr.set(b.field(p,16),16,b.template at<uint16_t>(p + 16));
					p += 16 + sizeof(uint16_t);
					break;
			}

			fixed = (b[p++] != 0);

			return (p - startAt);
		}

		uint64_t lastSend;
		uint64_t lastReceive;
		uint64_t lastUnicastFrame;
		uint64_t lastFirewallOpener;
		Demarc::Port localPort; // ANY_PORT if not defined
		unsigned int latency; // 0 if never determined
		InetAddress addr; // null InetAddress if path is undefined
		bool fixed; // do not learn address from received packets
	};

	unsigned char _keys[32 * 2]; // crypt key[32], mac key[32]
	Identity _id;

	WanPath _ipv4p;
	WanPath _ipv6p;

	// Fields below this line are not persisted with serialize()

	unsigned int _vMajor,_vMinor,_vRevision;
	bool _dirty;

	AtomicCounter __refCount;
};

} // namespace ZeroTier

#endif
