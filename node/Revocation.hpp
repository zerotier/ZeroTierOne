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

#ifndef ZT_REVOCATION_HPP
#define ZT_REVOCATION_HPP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "Constants.hpp"
#include "../include/ZeroTierOne.h"
#include "Address.hpp"
#include "C25519.hpp"
#include "Utils.hpp"
#include "Buffer.hpp"
#include "Identity.hpp"

/**
 * Flag: fast propagation via rumor mill algorithm
 */
#define ZT_REVOCATION_FLAG_FAST_PROPAGATE 0x1ULL

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * Revocation certificate to instantaneously revoke a COM, capability, or tag
 */
class Revocation
{
public:
	/**
	 * Credential type being revoked
	 */
	enum CredentialType
	{
		CREDENTIAL_TYPE_NULL = 0,
		CREDENTIAL_TYPE_COM = 1, // CertificateOfMembership
		CREDENTIAL_TYPE_CAPABILITY = 2,
		CREDENTIAL_TYPE_TAG = 3,
		CREDENTIAL_TYPE_COO = 4 // CertificateOfOwnership
	};

	Revocation()
	{
		memset(this,0,sizeof(Revocation));
	}

	/**
	 * @param i ID (arbitrary for revocations, currently random)
	 * @param nwid Network ID
	 * @param cid Credential ID being revoked (0 for all or for COMs, which lack IDs)
	 * @param thr Revocation time threshold before which credentials will be revoked
	 * @param fl Flags
	 * @param tgt Target node whose credential(s) are being revoked
	 * @param ct Credential type being revoked
	 */
	Revocation(const uint64_t i,const uint64_t nwid,const uint64_t cid,const uint64_t thr,const uint64_t fl,const Address &tgt,const CredentialType ct) :
		_id(i),
		_networkId(nwid),
		_credentialId(cid),
		_threshold(thr),
		_flags(fl),
		_target(tgt),
		_signedBy(),
		_type(ct) {}

	inline uint64_t id() const { return _id; }
	inline uint64_t networkId() const { return _networkId; }
	inline uint64_t credentialId() const { return _credentialId; }
	inline uint64_t threshold() const { return _threshold; }
	inline const Address &target() const { return _target; }
	inline const Address &signer() const { return _signedBy; }
	inline CredentialType type() const { return _type; }

	inline bool fastPropagate() const { return ((_flags & ZT_REVOCATION_FLAG_FAST_PROPAGATE) != 0); }

	/**
	 * @param signer Signing identity, must have private key
	 * @return True if signature was successful
	 */
	inline bool sign(const Identity &signer)
	{
		if (signer.hasPrivate()) {
			Buffer<sizeof(Revocation) + 64> tmp;
			_signedBy = signer.address();
			this->serialize(tmp,true);
			_signature = signer.sign(tmp.data(),tmp.size());
			return true;
		}
		return false;
	}

	/**
	 * Verify this revocation's signature
	 *
	 * @param RR Runtime environment to provide for peer lookup, etc.
	 * @return 0 == OK, 1 == waiting for WHOIS, -1 == BAD signature or chain
	 */
	int verify(const RuntimeEnvironment *RR) const;

	template<unsigned int C>
	inline void serialize(Buffer<C> &b,const bool forSign = false) const
	{
		if (forSign) b.append((uint64_t)0x7f7f7f7f7f7f7f7fULL);

		b.append(_id);
		b.append(_networkId);
		b.append(_credentialId);
		b.append(_threshold);
		b.append(_flags);
		_target.appendTo(b);
		_signedBy.appendTo(b);
		b.append((uint8_t)_type);

		if (!forSign) {
			b.append((uint8_t)1); // 1 == Ed25519 signature
			b.append((uint16_t)ZT_C25519_SIGNATURE_LEN);
			b.append(_signature.data,ZT_C25519_SIGNATURE_LEN);
		}

		// This is the size of any additional fields, currently 0.
		b.append((uint16_t)0);

		if (forSign) b.append((uint64_t)0x7f7f7f7f7f7f7f7fULL);
	}

	template<unsigned int C>
	inline unsigned int deserialize(const Buffer<C> &b,unsigned int startAt = 0)
	{
		memset(this,0,sizeof(Revocation));

		unsigned int p = startAt;

		_id = b.template at<uint64_t>(p); p += 8;
		_networkId = b.template at<uint64_t>(p); p += 8;
		_credentialId = b.template at<uint64_t>(p); p += 8;
		_threshold = b.template at<uint64_t>(p); p += 8;
		_flags = b.template at<uint64_t>(p); p += 8;
		_target.setTo(b.field(p,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH); p += ZT_ADDRESS_LENGTH;
		_signedBy.setTo(b.field(p,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH); p += ZT_ADDRESS_LENGTH;
		_type = (CredentialType)b[p++];

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
	uint64_t _id;
	uint64_t _networkId;
	uint64_t _credentialId;
	uint64_t _threshold;
	uint64_t _flags;
	Address _target;
	Address _signedBy;
	CredentialType _type;
	C25519::Signature _signature;
};

} // namespace ZeroTier

#endif
