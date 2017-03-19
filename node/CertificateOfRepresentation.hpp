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

#ifndef ZT_CERTIFICATEOFREPRESENTATION_HPP
#define ZT_CERTIFICATEOFREPRESENTATION_HPP

#include "Constants.hpp"
#include "Address.hpp"
#include "C25519.hpp"
#include "Identity.hpp"
#include "Buffer.hpp"

/**
 * Maximum number of addresses allowed in a COR
 */
#define ZT_CERTIFICATEOFREPRESENTATION_MAX_ADDRESSES ZT_MAX_UPSTREAMS

namespace ZeroTier {

/**
 * A signed enumeration of a node's roots (planet and moons)
 *
 * This is sent as part of HELLO and attests to which roots a node trusts
 * to represent it on the network. Federated roots (moons) can send these
 * further upstream to tell global roots which nodes they represent, making
 * them reachable via federated roots if they are not reachable directly.
 *
 * As of 1.2.0 this is sent but not used. Right now nodes still always
 * announce to planetary roots no matter what. In the future this can be
 * used to implement even better fault tolerance for federation for the
 * no roots are reachable case as well as a "privacy mode" where federated
 * roots can shield nodes entirely and p2p connectivity behind them can
 * be disabled. This will be desirable for a number of use cases.
 */
class CertificateOfRepresentation
{
public:
	CertificateOfRepresentation()
	{
		memset(this,0,sizeof(CertificateOfRepresentation));
	}

	inline uint64_t timestamp() const { return _timestamp; }
	inline const Address &representative(const unsigned int i) const { return _reps[i]; }
	inline unsigned int repCount() const { return _repCount; }

	inline void clear()
	{
		memset(this,0,sizeof(CertificateOfRepresentation));
	}

	/**
	 * Add a representative if space remains
	 *
	 * @param r Representative to add
	 * @return True if representative was added
	 */
	inline bool addRepresentative(const Address &r)
	{
		if (_repCount < ZT_CERTIFICATEOFREPRESENTATION_MAX_ADDRESSES) {
			_reps[_repCount++] = r;
			return true;
		}
		return false;
	}

	/**
	 * Sign this COR with my identity
	 *
	 * @param myIdentity This node's identity
	 * @param ts COR timestamp for establishing new vs. old
	 */
	inline void sign(const Identity &myIdentity,const uint64_t ts)
	{
		_timestamp = ts;
		Buffer<sizeof(CertificateOfRepresentation) + 32> tmp;
		this->serialize(tmp,true);
		_signature = myIdentity.sign(tmp.data(),tmp.size());
	}

	/**
	 * Verify this COR's signature
	 *
	 * @param senderIdentity Identity of sender of COR
	 * @return True if COR is valid
	 */
	inline bool verify(const Identity &senderIdentity)
	{
		try {
			Buffer<sizeof(CertificateOfRepresentation) + 32> tmp;
			this->serialize(tmp,true);
			return senderIdentity.verify(tmp.data(),tmp.size(),_signature.data,ZT_C25519_SIGNATURE_LEN);
		} catch ( ... ) {
			return false;
		}
	}

	template<unsigned int C>
	inline void serialize(Buffer<C> &b,const bool forSign = false) const
	{
		if (forSign) b.append((uint64_t)0x7f7f7f7f7f7f7f7fULL);

		b.append((uint64_t)_timestamp);
		b.append((uint16_t)_repCount);
		for(unsigned int i=0;i<_repCount;++i)
			_reps[i].appendTo(b);

		if (!forSign) {
			b.append((uint8_t)1); // 1 == Ed25519 signature
			b.append((uint16_t)ZT_C25519_SIGNATURE_LEN);
			b.append(_signature.data,ZT_C25519_SIGNATURE_LEN);
		}

		b.append((uint16_t)0); // size of any additional fields, currently 0

		if (forSign) b.append((uint64_t)0x7f7f7f7f7f7f7f7fULL);
	}

	template<unsigned int C>
	inline unsigned int deserialize(const Buffer<C> &b,unsigned int startAt = 0)
	{
		clear();

		unsigned int p = startAt;

		_timestamp = b.template at<uint64_t>(p); p += 8;
		const unsigned int rc = b.template at<uint16_t>(p); p += 2;
		for(unsigned int i=0;i<rc;++i) {
			if (i < ZT_CERTIFICATEOFREPRESENTATION_MAX_ADDRESSES)
				_reps[i].setTo(b.field(p,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH);
			p += ZT_ADDRESS_LENGTH;
		}
		_repCount = (rc > ZT_CERTIFICATEOFREPRESENTATION_MAX_ADDRESSES) ? ZT_CERTIFICATEOFREPRESENTATION_MAX_ADDRESSES : rc;

		if (b[p++] == 1) {
			if (b.template at<uint16_t>(p) == ZT_C25519_SIGNATURE_LEN) {
				p += 2;
				memcpy(_signature.data,b.field(p,ZT_C25519_SIGNATURE_LEN),ZT_C25519_SIGNATURE_LEN);
				p += ZT_C25519_SIGNATURE_LEN;
			} else throw std::runtime_error("invalid signature");
		} else {
			p += 2 + b.template at<uint16_t>(p);
		}

		p += 2 + b.template at<uint16_t>(p);
		if (p > b.size())
			throw std::runtime_error("extended field overflow");

		return (p - startAt);
	}

private:
	uint64_t _timestamp;
	Address _reps[ZT_CERTIFICATEOFREPRESENTATION_MAX_ADDRESSES];
	unsigned int _repCount;
	C25519::Signature _signature;
};

} // namespace ZeroTier

#endif
