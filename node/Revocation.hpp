/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2023-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_REVOCATION_HPP
#define ZT_REVOCATION_HPP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "Constants.hpp"
#include "../include/ZeroTierOne.h"
#include "Credential.hpp"
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
class Revocation : public Credential
{
public:
	static inline Credential::Type credentialType() { return Credential::CREDENTIAL_TYPE_REVOCATION; }

	Revocation() :
		_id(0),
		_credentialId(0),
		_networkId(0),
		_threshold(0),
		_flags(0),
		_target(),
		_signedBy(),
		_type(Credential::CREDENTIAL_TYPE_NULL)
	{
		memset(_signature.data,0,sizeof(_signature.data));
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
	Revocation(const uint32_t i,const uint64_t nwid,const uint32_t cid,const uint64_t thr,const uint64_t fl,const Address &tgt,const Credential::Type ct) :
		_id(i),
		_credentialId(cid),
		_networkId(nwid),
		_threshold(thr),
		_flags(fl),
		_target(tgt),
		_signedBy(),
		_type(ct)
	{
		memset(_signature.data,0,sizeof(_signature.data));
	}

	inline uint32_t id() const { return _id; }
	inline uint32_t credentialId() const { return _credentialId; }
	inline uint64_t networkId() const { return _networkId; }
	inline int64_t threshold() const { return _threshold; }
	inline const Address &target() const { return _target; }
	inline const Address &signer() const { return _signedBy; }
	inline Credential::Type type() const { return _type; }

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
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @return 0 == OK, 1 == waiting for WHOIS, -1 == BAD signature or chain
	 */
	int verify(const RuntimeEnvironment *RR,void *tPtr) const;

	template<unsigned int C>
	inline void serialize(Buffer<C> &b,const bool forSign = false) const
	{
		if (forSign) b.append((uint64_t)0x7f7f7f7f7f7f7f7fULL);

		b.append((uint32_t)0); // 4 unused bytes, currently set to 0
		b.append(_id);
		b.append(_networkId);
		b.append((uint32_t)0); // 4 unused bytes, currently set to 0
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
		*this = Revocation();

		unsigned int p = startAt;

		p += 4; // 4 bytes, currently unused
		_id = b.template at<uint32_t>(p); p += 4;
		_networkId = b.template at<uint64_t>(p); p += 8;
		p += 4; // 4 bytes, currently unused
		_credentialId = b.template at<uint32_t>(p); p += 4;
		_threshold = b.template at<uint64_t>(p); p += 8;
		_flags = b.template at<uint64_t>(p); p += 8;
		_target.setTo(b.field(p,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH); p += ZT_ADDRESS_LENGTH;
		_signedBy.setTo(b.field(p,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH); p += ZT_ADDRESS_LENGTH;
		_type = (Credential::Type)b[p++];

		if (b[p++] == 1) {
			if (b.template at<uint16_t>(p) == ZT_C25519_SIGNATURE_LEN) {
				p += 2;
				memcpy(_signature.data,b.field(p,ZT_C25519_SIGNATURE_LEN),ZT_C25519_SIGNATURE_LEN);
				p += ZT_C25519_SIGNATURE_LEN;
			} else throw ZT_EXCEPTION_INVALID_SERIALIZED_DATA_INVALID_CRYPTOGRAPHIC_TOKEN;
		} else {
			p += 2 + b.template at<uint16_t>(p);
		}

		p += 2 + b.template at<uint16_t>(p);
		if (p > b.size())
			throw ZT_EXCEPTION_INVALID_SERIALIZED_DATA_OVERFLOW;

		return (p - startAt);
	}

private:
	uint32_t _id;
	uint32_t _credentialId;
	uint64_t _networkId;
	int64_t _threshold;
	uint64_t _flags;
	Address _target;
	Address _signedBy;
	Credential::Type _type;
	C25519::Signature _signature;
};

} // namespace ZeroTier

#endif
