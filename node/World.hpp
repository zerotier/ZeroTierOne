/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

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
#define ZT_WORLD_MAX_SERIALIZED_LENGTH (((1024 + (32 * ZT_WORLD_MAX_STABLE_ENDPOINTS_PER_ROOT)) * ZT_WORLD_MAX_ROOTS) + ZT_C25519_PUBLIC_KEY_LEN + ZT_C25519_SIGNATURE_LEN + 128)

/**
 * World ID for Earth
 *
 * This is the ID for the ZeroTier World used on planet Earth. It is unrelated
 * to the public network 8056c2e21c000001 of the same name. It was chosen
 * from Earth's approximate distance from the sun in kilometers.
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
 * Think of a World as a single data center. Within this data center a set
 * of distributed fault tolerant root servers provide stable anchor points
 * for a peer to peer network that provides VLAN service. Updates to a world
 * definition can be published by signing them with the previous revision's
 * signing key, and should be very infrequent.
 *
 * The maximum data center size is approximately 2.5 cubic light seconds,
 * since many protocols have issues with >5s RTT latencies.
 *
 * ZeroTier operates a World for Earth capable of encompassing the planet, its
 * orbits, the Moon (about 1.3 light seconds), and nearby Lagrange points. A
 * world ID for Mars and nearby space is defined but not yet used, and a test
 * world ID is provided for testing purposes.
 */
class World
{
public:
	/**
	 * World type -- do not change IDs
	 */
	enum Type
	{
		TYPE_NULL = 0,
		TYPE_PLANET = 1, // Planets, of which there is currently one (Earth)
		TYPE_MOON = 127  // Moons, which are user-created and many
	};

	/**
	 * Upstream server definition in world/moon
	 */
	struct Root
	{
		Identity identity;
		std::vector<InetAddress> stableEndpoints;

		inline bool operator==(const Root &r) const { return ((identity == r.identity)&&(stableEndpoints == r.stableEndpoints)); }
		inline bool operator!=(const Root &r) const { return (!(*this == r)); }
		inline bool operator<(const Root &r) const { return (identity < r.identity); } // for sorting
	};

	/**
	 * Construct an empty / null World
	 */
	World() :
		_id(0),
		_ts(0),
		_type(TYPE_NULL) {}

	/**
	 * @return Root servers for this world and their stable endpoints
	 */
	inline const std::vector<World::Root> &roots() const { return _roots; }

	/**
	 * @return World type: planet or moon
	 */
	inline Type type() const { return _type; }

	/**
	 * @return World unique identifier
	 */
	inline uint64_t id() const { return _id; }

	/**
	 * @return World definition timestamp
	 */
	inline uint64_t timestamp() const { return _ts; }

	/**
	 * @return C25519 signature
	 */
	inline const C25519::Signature &signature() const { return _signature; }

	/**
	 * @return Public key that must sign next update
	 */
	inline const C25519::Public &updatesMustBeSignedBy() const { return _updatesMustBeSignedBy; }

	/**
	 * Check whether a world update should replace this one
	 *
	 * @param update Candidate update
	 * @return True if update is newer than current, matches its ID and type, and is properly signed (or if current is NULL)
	 */
	inline bool shouldBeReplacedBy(const World &update)
	{
		if ((_id == 0)||(_type == TYPE_NULL)) {
			return true;
		}
		if ((_id == update._id)&&(_ts < update._ts)&&(_type == update._type)) {
			Buffer<ZT_WORLD_MAX_SERIALIZED_LENGTH> tmp;
			update.serialize(tmp,true);
			return C25519::verify(_updatesMustBeSignedBy,tmp.data(),tmp.size(),update._signature);
		}
		return false;
	}

	/**
	 * @return True if this World is non-empty
	 */
	inline operator bool() const { return (_type != TYPE_NULL); }

	template<unsigned int C>
	inline void serialize(Buffer<C> &b,bool forSign = false) const
	{
		if (forSign) {
			b.append((uint64_t)0x7f7f7f7f7f7f7f7fULL);
		}

		b.append((uint8_t)_type);
		b.append((uint64_t)_id);
		b.append((uint64_t)_ts);
		b.append(_updatesMustBeSignedBy.data,ZT_C25519_PUBLIC_KEY_LEN);
		if (!forSign) {
			b.append(_signature.data,ZT_C25519_SIGNATURE_LEN);
		}
		b.append((uint8_t)_roots.size());
		for(std::vector<Root>::const_iterator r(_roots.begin());r!=_roots.end();++r) {
			r->identity.serialize(b);
			b.append((uint8_t)r->stableEndpoints.size());
			for(std::vector<InetAddress>::const_iterator ep(r->stableEndpoints.begin());ep!=r->stableEndpoints.end();++ep) {
				ep->serialize(b);
			}
		}
		if (_type == TYPE_MOON) {
			b.append((uint16_t)0); // no attached dictionary (for future use)
		}

		if (forSign) {
			b.append((uint64_t)0xf7f7f7f7f7f7f7f7ULL);
		}
	}

	template<unsigned int C>
	inline unsigned int deserialize(const Buffer<C> &b,unsigned int startAt = 0)
	{
		unsigned int p = startAt;

		_roots.clear();

		switch((Type)b[p++]) {
			case TYPE_NULL: // shouldn't ever really happen in serialized data but it's not invalid
				_type = TYPE_NULL;
				break;
			case TYPE_PLANET:
				_type = TYPE_PLANET;
				break;
			case TYPE_MOON:
				_type = TYPE_MOON;
				break;
			default:
				throw ZT_EXCEPTION_INVALID_SERIALIZED_DATA_INVALID_TYPE;
		}

		_id = b.template at<uint64_t>(p);
		p += 8;
		_ts = b.template at<uint64_t>(p);
		p += 8;
		memcpy(_updatesMustBeSignedBy.data,b.field(p,ZT_C25519_PUBLIC_KEY_LEN),ZT_C25519_PUBLIC_KEY_LEN);
		p += ZT_C25519_PUBLIC_KEY_LEN;
		memcpy(_signature.data,b.field(p,ZT_C25519_SIGNATURE_LEN),ZT_C25519_SIGNATURE_LEN);
		p += ZT_C25519_SIGNATURE_LEN;
		const unsigned int numRoots = (unsigned int)b[p++];
		if (numRoots > ZT_WORLD_MAX_ROOTS) {
			throw ZT_EXCEPTION_INVALID_SERIALIZED_DATA_OVERFLOW;
		}
		for(unsigned int k=0;k<numRoots;++k) {
			_roots.push_back(Root());
			Root &r = _roots.back();
			p += r.identity.deserialize(b,p);
			unsigned int numStableEndpoints = b[p++];
			if (numStableEndpoints > ZT_WORLD_MAX_STABLE_ENDPOINTS_PER_ROOT) {
				throw ZT_EXCEPTION_INVALID_SERIALIZED_DATA_OVERFLOW;
			}
			for(unsigned int kk=0;kk<numStableEndpoints;++kk) {
				r.stableEndpoints.push_back(InetAddress());
				p += r.stableEndpoints.back().deserialize(b,p);
			}
		}
		if (_type == TYPE_MOON) {
			p += b.template at<uint16_t>(p) + 2;
		}

		return (p - startAt);
	}

	inline bool operator==(const World &w) const { return ((_id == w._id)&&(_ts == w._ts)&&(memcmp(_updatesMustBeSignedBy.data,w._updatesMustBeSignedBy.data,ZT_C25519_PUBLIC_KEY_LEN) == 0)&&(memcmp(_signature.data,w._signature.data,ZT_C25519_SIGNATURE_LEN) == 0)&&(_roots == w._roots)&&(_type == w._type)); }
	inline bool operator!=(const World &w) const { return (!(*this == w)); }

	/**
	 * Create a World object signed with a key pair
	 *
	 * @param t World type
	 * @param id World ID
	 * @param ts World timestamp / revision
	 * @param sk Key that must be used to sign the next future update to this world
	 * @param roots Roots and their stable endpoints
	 * @param signWith Key to sign this World with (can have the same public as the next-update signing key, but doesn't have to)
	 * @return Signed World object
	 */
	static inline World make(World::Type t,uint64_t id,uint64_t ts,const C25519::Public &sk,const std::vector<World::Root> &roots,const C25519::Pair &signWith)
	{
		World w;
		w._id = id;
		w._ts = ts;
		w._type = t;
		w._updatesMustBeSignedBy = sk;
		w._roots = roots;

		Buffer<ZT_WORLD_MAX_SERIALIZED_LENGTH> tmp;
		w.serialize(tmp,true);
		w._signature = C25519::sign(signWith,tmp.data(),tmp.size());

		return w;
	}

protected:
	uint64_t _id;
	uint64_t _ts;
	Type _type;
	C25519::Public _updatesMustBeSignedBy;
	C25519::Signature _signature;
	std::vector<Root> _roots;
};

} // namespace ZeroTier

#endif
