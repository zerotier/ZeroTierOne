/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2024-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_REVOCATION_HPP
#define ZT_REVOCATION_HPP

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

#include "Constants.hpp"
#include "Credential.hpp"
#include "Address.hpp"
#include "C25519.hpp"
#include "Utils.hpp"
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
	friend class Credential;

public:
	static ZT_ALWAYS_INLINE ZT_CredentialType credentialType() { return ZT_CREDENTIAL_TYPE_REVOCATION; }

	ZT_ALWAYS_INLINE Revocation() :
		_id(0),
		_credentialId(0),
		_networkId(0),
		_threshold(0),
		_flags(0),
		_target(),
		_signedBy(),
		_type(ZT_CREDENTIAL_TYPE_NULL),
		_signatureLength(0)
	{
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
	ZT_ALWAYS_INLINE Revocation(const uint32_t i,const uint64_t nwid,const uint32_t cid,const uint64_t thr,const uint64_t fl,const Address &tgt,const ZT_CredentialType ct) :
		_id(i),
		_credentialId(cid),
		_networkId(nwid),
		_threshold(thr),
		_flags(fl),
		_target(tgt),
		_signedBy(),
		_type(ct),
		_signatureLength(0)
	{
	}

	ZT_ALWAYS_INLINE uint32_t id() const { return _id; }
	ZT_ALWAYS_INLINE uint32_t credentialId() const { return _credentialId; }
	ZT_ALWAYS_INLINE uint64_t networkId() const { return _networkId; }
	ZT_ALWAYS_INLINE int64_t threshold() const { return _threshold; }
	ZT_ALWAYS_INLINE const Address &target() const { return _target; }
	ZT_ALWAYS_INLINE const Address &signer() const { return _signedBy; }
	ZT_ALWAYS_INLINE ZT_CredentialType typeBeingRevoked() const { return _type; }
	ZT_ALWAYS_INLINE const uint8_t *signature() const { return _signature; }
	ZT_ALWAYS_INLINE unsigned int signatureLength() const { return _signatureLength; }
	ZT_ALWAYS_INLINE bool fastPropagate() const { return ((_flags & ZT_REVOCATION_FLAG_FAST_PROPAGATE) != 0); }

	/**
	 * @param signer Signing identity, must have private key
	 * @return True if signature was successful
	 */
	ZT_ALWAYS_INLINE bool sign(const Identity &signer)
	{
		if (signer.hasPrivate()) {
			Buffer<sizeof(Revocation) + 64> tmp;
			_signedBy = signer.address();
			this->serialize(tmp,true);
			_signatureLength = signer.sign(tmp.data(),tmp.size(),_signature,sizeof(_signature));
			return true;
		}
		return false;
	}

	/**
	 * Verify this revocation's signature
	 *
	 * @param RR Runtime environment to provide for peer lookup, etc.
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 */
	ZT_ALWAYS_INLINE Credential::VerifyResult verify(const RuntimeEnvironment *RR,void *tPtr) const { return _verify(RR,tPtr,*this); }

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
			b.append((uint8_t)1);
			b.append((uint16_t)_signatureLength);
			b.append(_signature,_signatureLength);
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
		_type = (ZT_CredentialType)b[p++];

		if (b[p++] == 1) {
			_signatureLength = b.template at<uint16_t>(p);
			if (_signatureLength > sizeof(_signature))
				throw ZT_EXCEPTION_INVALID_SERIALIZED_DATA_INVALID_CRYPTOGRAPHIC_TOKEN;
			memcpy(_signature,b.field(p,_signatureLength),_signatureLength);
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
	ZT_CredentialType _type;
	unsigned int _signatureLength;
	uint8_t _signature[ZT_SIGNATURE_BUFFER_SIZE];
};

} // namespace ZeroTier

#endif
