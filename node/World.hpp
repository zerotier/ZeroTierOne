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

#ifndef ZT_WORLD_HPP
#define ZT_WORLD_HPP

#include <vector>
#include <string>

#include "Constants.hpp"
#include "InetAddress.hpp"
#include "Identity.hpp"
#include "Buffer.hpp"
#include "C25519.hpp"

/**
 * Maximum number of roots (sanity limit, okay to increase)
 *
 * A given root can (through multi-homing) be distributed across any number of
 * physical endpoints, but having more than one is good to permit total failure
 * of one root or its withdrawal due to compromise without taking the whole net
 * down.
 */
#define ZT_WORLD_MAX_ROOTS 4

/**
 * Maximum number of stable endpoints per root (sanity limit, okay to increase)
 */
#define ZT_WORLD_MAX_STABLE_ENDPOINTS_PER_ROOT 32

/**
 * The (more than) maximum length of a serialized World
 */
#define ZT_WORLD_MAX_SERIALIZED_LENGTH (((1024 + (32 * ZT_WORLD_MAX_STABLE_ENDPOINTS_PER_ROOT)) * ZT_WORLD_MAX_ROOTS) + ZT_C25519_PUBLIC_KEY_LEN + ZT_C25519_SIGNATURE_LEN + 64)

/**
 * World ID indicating null / empty World object
 */
#define ZT_WORLD_ID_NULL 0

/**
 * World ID for a test network with ephemeral or temporary roots
 */
#define ZT_WORLD_ID_TESTNET 1

/**
 * World ID for Earth -- its approximate distance from the sun in kilometers
 *
 * This is the ID for the ZeroTier World used on planet Earth. It is unrelated
 * to the public network 8056c2e21c000001 of the same name.
 *
 * It's advisable to create a new World for network regions spaced more than
 * 2-3 light seconds, since RTT times in excess of 5s are problematic for some
 * protocols. Earth could therefore include its low and high orbits, the Moon,
 * and nearby Lagrange points.
 */
#define ZT_WORLD_ID_EARTH 149604618

/**
 * World ID for Mars -- for future use by SpaceX or others
 */
#define ZT_WORLD_ID_MARS 227883110

namespace ZeroTier {

/**
 * A world definition (formerly known as a root topology)
 *
 * A world consists of a set of root servers and a signature scheme enabling
 * it to be updated going forward. It defines a single ZeroTier VL1 network
 * area within which any device can reach any other.
 */
class World
{
public:
	struct Root
	{
		Identity identity;
		std::vector<InetAddress> stableEndpoints;

		inline bool operator==(const Root &r) const throw() { return ((identity == r.identity)&&(stableEndpoints == r.stableEndpoints)); }
		inline bool operator!=(const Root &r) const throw() { return (!(*this == r)); }
		inline bool operator<(const Root &r) const throw() { return (identity < r.identity); } // for sorting
	};

	/**
	 * Construct an empty / null World
	 */
	World() :
		_id(ZT_WORLD_ID_NULL),
		_ts(0) {}

	/**
	 * @return Root servers for this world and their stable endpoints
	 */
	inline const std::vector<World::Root> &roots() const throw() { return _roots; }

	/**
	 * @return World unique identifier
	 */
	inline uint64_t id() const throw() { return _id; }

	/**
	 * @return World definition timestamp
	 */
	inline uint64_t timestamp() const throw() { return _ts; }

	/**
	 * Verify a world update
	 *
	 * A new world update is valid if it is for the same world ID, is newer,
	 * and is signed by the current world's signing key. If this world object
	 * is null, it can always be updated.
	 *
	 * @param update Candidate update
	 * @return True if update is newer than current and is properly signed
	 */
	inline bool verifyUpdate(const World &update)
	{
		if (_id == ZT_WORLD_ID_NULL)
			return true;
		if ((update._id != _id)||(update._ts <= _ts))
			return false;
		Buffer<ZT_WORLD_MAX_SERIALIZED_LENGTH> tmp;
		update.serialize(tmp);
		return C25519::verify(_updateSigningKey,tmp.data(),tmp.size(),update._signature);
	}

	/**
	 * @return True if this World is non-empty
	 */
	inline operator bool() const throw() { return (_id != ZT_WORLD_ID_NULL); }

	template<unsigned int C>
	inline void serialize(Buffer<C> &b) const
	{
		b.append((uint8_t)0x01); // version -- only one valid value for now
		b.append((uint64_t)_id);
		b.append((uint64_t)_ts);
		b.append(_updateSigningKey.data,ZT_C25519_PUBLIC_KEY_LEN);
		b.append(_signature.data,ZT_C25519_SIGNATURE_LEN);
		b.append((uint8_t)_roots.size());
		for(std::vector<Root>::const_iterator r(_roots.begin());r!=_roots.end();++r) {
			r->identity.serialize(b);
			b.append((uint8_t)r->stableEndpoints.size());
			for(std::vector<InetAddress>::const_iterator ep(r->stableEndpoints.begin());ep!=r->stableEndpoints.end();++ep)
				ep->serialize(b);
		}
	}

	template<unsigned int C>
	inline unsigned int deserialize(const Buffer<C> &b,unsigned int startAt = 0)
	{
		unsigned int p = startAt;

		_roots.clear();

		if (b[p++] != 0x01)
			throw std::invalid_argument("invalid World serialized version");

		_id = b.template at<uint64_t>(p); p += 8;
		_ts = b.template at<uint64_t>(p); p += 8;
		memcpy(_updateSigningKey.data,b.field(p,ZT_C25519_PUBLIC_KEY_LEN),ZT_C25519_PUBLIC_KEY_LEN); p += ZT_C25519_PUBLIC_KEY_LEN;
		memcpy(_signature.data,b.field(p,ZT_C25519_SIGNATURE_LEN),ZT_C25519_SIGNATURE_LEN); p += ZT_C25519_SIGNATURE_LEN;
		unsigned int numRoots = b[p++];
		if (numRoots > ZT_WORLD_MAX_ROOTS)
			throw std::invalid_argument("too many roots in World");
		for(unsigned int k=0;k<numRoots;++k) {
			_roots.push_back(Root());
			Root &r = _roots.back();
			p += r.identity.deserialize(b,p);
			unsigned int numStableEndpoints = b[p++];
			if (numStableEndpoints > ZT_WORLD_MAX_STABLE_ENDPOINTS_PER_ROOT)
				throw std::invalid_argument("too many stable endpoints in World/Root");
			for(unsigned int kk=0;kk<numStableEndpoints;++kk) {
				r.stableEndpoints.push_back(InetAddress());
				p += r.stableEndpoints.back().deserialize(b,p);
			}
		}

		return (p - startAt);
	}

	inline bool operator==(const World &w) const throw() { return ((_id == w._id)&&(_ts == w._ts)&&(_roots == w._roots)); }
	inline bool operator!=(const World &w) const throw() { return (!(*this == w)); }

protected:
	uint64_t _id;
	uint64_t _ts;
	C25519::Public _updateSigningKey;
	C25519::Signature _signature;
	std::vector<Root> _roots;
};

} // namespace ZeroTier

#endif
