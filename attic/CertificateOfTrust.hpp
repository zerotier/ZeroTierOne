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

#ifndef ZT_CERTIFICATEOFTRUST_HPP
#define ZT_CERTIFICATEOFTRUST_HPP

#include "Constants.hpp"
#include "Identity.hpp"
#include "C25519.hpp"
#include "Buffer.hpp"

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * Certificate of peer to peer trust
 */
class CertificateOfTrust
{
public:
	/**
	 * Trust levels, with 0 indicating anti-trust
	 */
	enum Level
	{
		/**
		 * Negative trust is reserved for informing peers that another peer is misbehaving, etc. Not currently used.
		 */
		LEVEL_NEGATIVE = 0,

		/**
		 * Default trust -- for most peers
		 */
		LEVEL_DEFAULT = 1,

		/**
		 * Above normal trust, e.g. common network membership
		 */
		LEVEL_MEDIUM = 25,

		/**
		 * High trust -- e.g. an upstream or a controller
		 */
		LEVEL_HIGH = 50,

		/**
		 * Right now ultimate is only for roots
		 */
		LEVEL_ULTIMATE = 100
	};

	/**
	 * Role bit masks
	 */
	enum Role
	{
		/**
		 * Target is permitted to represent issuer on the network as a federated root / relay
		 */
		ROLE_UPSTREAM = 0x00000001
	};

	CertificateOfTrust() :
		_timestamp(0),
		_roles(0),
		_issuer(),
		_target(),
		_level(LEVEL_DEFAULT),
		_signature() {}

	/**
	 * Create and sign this certificate of trust
	 *
	 * @param ts Cert timestamp
	 * @param rls Roles bitmap
	 * @param iss Issuer identity (must have secret key!)
	 * @param tgt Target identity
	 * @param l Trust level
	 * @return True on successful signature
	 */
	bool create(uint64_t ts,uint64_t rls,const Identity &iss,const Identity &tgt,Level l);

	/**
	 * Verify this COT and its signature
	 *
	 * @param RR Runtime environment for looking up peers
	 * @return 0 == OK, 1 == waiting for WHOIS, -1 == BAD signature or credential
	 */
	int verify(const RuntimeEnvironment *RR) const;

	inline bool roleUpstream() const { return ((_roles & (uint64_t)ROLE_UPSTREAM) != 0); }

	inline uint64_t timestamp() const { return _timestamp; }
	inline uint64_t roles() const { return _roles; }
	inline const Address &issuer() const { return _issuer; }
	inline const Identity &target() const { return _target; }
	inline Level level() const { return _level; }

	inline operator bool() const { return (_issuer); }

	template<unsigned int C>
	inline void serialize(Buffer<C> &b) const
	{
		b.append(_timestamp);
		b.append(_roles);
		_issuer.appendTo(b);
		_target.serialize(b);
		b.append((uint16_t)_level);
		b.append((uint8_t)1); // 1 == ed25519 signature
		b.append((uint16_t)ZT_C25519_SIGNATURE_LEN);
		b.append(_signature.data,ZT_C25519_SIGNATURE_LEN);
		b.append((uint16_t)0); // length of additional fields
	}

	template<unsigned int C>
	inline unsigned int deserialize(const Buffer<C> &b,unsigned int startAt = 0)
	{
		unsigned int p = startAt;
		_timestamp = b.template at<uint64_t>(p); p += 8;
		_roles = b.template at<uint64_t>(p); p += 8;
		_issuer.setTo(b.field(p,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH); p += ZT_ADDRESS_LENGTH;
		p += _target.deserialize(b,p);
		_level = b.template at<uint16_t>(p); p += 2;
		p += b.template at<uint16_t>(p); p += 2;
		return (p - startAt);
	}

private:
	uint64_t _timestamp;
	uint64_t _roles;
	Address _issuer;
	Identity _target;
	Level _level;
	C25519::Signature _signature;
};

} // namespace ZeroTier

#endif
