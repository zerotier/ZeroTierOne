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

#ifndef ZT_CERTIFICATEOFMEMBERSHIP_HPP
#define ZT_CERTIFICATEOFMEMBERSHIP_HPP

#include <stdint.h>
#include <string.h>

#include <string>
#include <stdexcept>
#include <algorithm>

#include "Constants.hpp"
#include "Credential.hpp"
#include "Buffer.hpp"
#include "Address.hpp"
#include "C25519.hpp"
#include "Identity.hpp"
#include "Utils.hpp"

/**
 * Maximum number of qualifiers allowed in a COM (absolute max: 65535)
 */
#define ZT_NETWORK_COM_MAX_QUALIFIERS 8

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * Certificate of network membership
 *
 * The COM contains a sorted set of three-element tuples called qualifiers.
 * These contain an id, a value, and a maximum delta.
 *
 * The ID is arbitrary and should be assigned using a scheme that makes
 * every ID globally unique. IDs beneath 65536 are reserved for global
 * assignment by ZeroTier Networks.
 *
 * The value's meaning is ID-specific and isn't important here. What's
 * important is the value and the third member of the tuple: the maximum
 * delta. The maximum delta is the maximum difference permitted between
 * values for a given ID between certificates for the two certificates to
 * themselves agree.
 *
 * Network membership is checked by checking whether a peer's certificate
 * agrees with your own. The timestamp provides the fundamental criterion--
 * each member of a private network must constantly obtain new certificates
 * often enough to stay within the max delta for this qualifier. But other
 * criteria could be added in the future for very special behaviors, things
 * like latitude and longitude for instance.
 *
 * This is a memcpy()'able structure and is safe (in a crash sense) to modify
 * without locks.
 */
class CertificateOfMembership : public Credential
{
public:
	static inline Credential::Type credentialType() { return Credential::CREDENTIAL_TYPE_COM; }

	/**
	 * Reserved qualifier IDs
	 *
	 * IDs below 1024 are reserved for use as standard IDs. Others are available
	 * for user-defined use.
	 *
	 * Addition of new required fields requires that code in hasRequiredFields
	 * be updated as well.
	 */
	enum ReservedId
	{
		/**
		 * Timestamp of certificate
		 */
		COM_RESERVED_ID_TIMESTAMP = 0,

		/**
		 * Network ID for which certificate was issued
		 */
		COM_RESERVED_ID_NETWORK_ID = 1,

		/**
		 * ZeroTier address to whom certificate was issued
		 */
		COM_RESERVED_ID_ISSUED_TO = 2

		// IDs 3-6 reserved for full hash of identity to which this COM was issued.
	};

	/**
	 * Create an empty certificate of membership
	 */
	CertificateOfMembership() :
		_qualifierCount(0) {}

	/**
	 * Create from required fields common to all networks
	 *
	 * @param timestamp Timestamp of certificate
	 * @param timestampMaxDelta Maximum variation between timestamps on this net
	 * @param nwid Network ID
	 * @param issuedTo Certificate recipient
	 */
	CertificateOfMembership(uint64_t timestamp,uint64_t timestampMaxDelta,uint64_t nwid,const Identity &issuedTo);

	/**
	 * Create from binary-serialized COM in buffer
	 *
	 * @param b Buffer to deserialize from
	 * @param startAt Position to start in buffer
	 */
	template<unsigned int C>
	CertificateOfMembership(const Buffer<C> &b,unsigned int startAt = 0)
	{
		deserialize(b,startAt);
	}

	/**
	 * @return True if there's something here
	 */
	inline operator bool() const { return (_qualifierCount != 0); }

	/**
	 * @return Credential ID, always 0 for COMs
	 */
	inline uint32_t id() const { return 0; }

	/**
	 * @return Timestamp for this cert and maximum delta for timestamp
	 */
	inline int64_t timestamp() const
	{
		for(unsigned int i=0;i<_qualifierCount;++i) {
			if (_qualifiers[i].id == COM_RESERVED_ID_TIMESTAMP) {
				return _qualifiers[i].value;
			}
		}
		return 0;
	}

	/**
	 * @return Address to which this cert was issued
	 */
	inline Address issuedTo() const
	{
		for(unsigned int i=0;i<_qualifierCount;++i) {
			if (_qualifiers[i].id == COM_RESERVED_ID_ISSUED_TO) {
				return Address(_qualifiers[i].value);
			}
		}
		return Address();
	}

	/**
	 * @return Network ID for which this cert was issued
	 */
	inline uint64_t networkId() const
	{
		for(unsigned int i=0;i<_qualifierCount;++i) {
			if (_qualifiers[i].id == COM_RESERVED_ID_NETWORK_ID) {
				return _qualifiers[i].value;
			}
		}
		return 0ULL;
	}

	/**
	 * Compare two certificates for parameter agreement
	 *
	 * This compares this certificate with the other and returns true if all
	 * parameters in this cert are present in the other and if they agree to
	 * within this cert's max delta value for each given parameter.
	 *
	 * Tuples present in other but not in this cert are ignored, but any
	 * tuples present in this cert but not in other result in 'false'.
	 *
	 * @param other Cert to compare with
	 * @param otherIdentity Identity of other node
	 * @return True if certs agree and 'other' may be communicated with
	 */
	bool agreesWith(const CertificateOfMembership &other, const Identity &otherIdentity) const;

	/**
	 * Sign this certificate
	 *
	 * @param with Identity to sign with, must include private key
	 * @return True if signature was successful
	 */
	bool sign(const Identity &with);

	/**
	 * Verify this COM and its signature
	 *
	 * @param RR Runtime environment for looking up peers
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @return 0 == OK, 1 == waiting for WHOIS, -1 == BAD signature or credential
	 */
	int verify(const RuntimeEnvironment *RR,void *tPtr) const;

	/**
	 * @return True if signed
	 */
	inline bool isSigned() const { return (_signedBy); }

	/**
	 * @return Address that signed this certificate or null address if none
	 */
	inline const Address &signedBy() const { return _signedBy; }

	template<unsigned int C>
	inline void serialize(Buffer<C> &b) const
	{
		b.append((uint8_t)1);
		b.append((uint16_t)_qualifierCount);
		for(unsigned int i=0;i<_qualifierCount;++i) {
			b.append(_qualifiers[i].id);
			b.append(_qualifiers[i].value);
			b.append(_qualifiers[i].maxDelta);
		}
		_signedBy.appendTo(b);
		if (_signedBy) {
			b.append(_signature.data,ZT_ECC_SIGNATURE_LEN);
		}
	}

	template<unsigned int C>
	inline unsigned int deserialize(const Buffer<C> &b,unsigned int startAt = 0)
	{
		unsigned int p = startAt;

		_qualifierCount = 0;
		_signedBy.zero();

		if (b[p++] != 1) {
			throw ZT_EXCEPTION_INVALID_SERIALIZED_DATA_INVALID_TYPE;
		}

		unsigned int numq = b.template at<uint16_t>(p);
		p += sizeof(uint16_t);
		uint64_t lastId = 0;
		for(unsigned int i=0;i<numq;++i) {
			const uint64_t qid = b.template at<uint64_t>(p);
			if (qid < lastId) {
				throw ZT_EXCEPTION_INVALID_SERIALIZED_DATA_BAD_ENCODING;
			} else {
				lastId = qid;
			}
			if (_qualifierCount < ZT_NETWORK_COM_MAX_QUALIFIERS) {
				_qualifiers[_qualifierCount].id = qid;
				_qualifiers[_qualifierCount].value = b.template at<uint64_t>(p + 8);
				_qualifiers[_qualifierCount].maxDelta = b.template at<uint64_t>(p + 16);
				p += 24;
				++_qualifierCount;
			} else {
				throw ZT_EXCEPTION_INVALID_SERIALIZED_DATA_OVERFLOW;
			}
		}

		_signedBy.setTo(b.field(p,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH);
		p += ZT_ADDRESS_LENGTH;

		if (_signedBy) {
			memcpy(_signature.data,b.field(p,ZT_ECC_SIGNATURE_LEN),ZT_ECC_SIGNATURE_LEN);
			p += ZT_ECC_SIGNATURE_LEN;
		}

		return (p - startAt);
	}

	inline bool operator==(const CertificateOfMembership &c) const
	{
		if (_signedBy != c._signedBy) {
			return false;
		}
		if (_qualifierCount != c._qualifierCount) {
			return false;
		}
		for(unsigned int i=0;i<_qualifierCount;++i) {
			const _Qualifier &a = _qualifiers[i];
			const _Qualifier &b = c._qualifiers[i];
			if ((a.id != b.id)||(a.value != b.value)||(a.maxDelta != b.maxDelta)) {
				return false;
			}
		}
		return (memcmp(_signature.data,c._signature.data,ZT_ECC_SIGNATURE_LEN) == 0);
	}
	inline bool operator!=(const CertificateOfMembership &c) const { return (!(*this == c)); }

private:
	struct _Qualifier
	{
		_Qualifier() : id(0),value(0),maxDelta(0) {}
		uint64_t id;
		uint64_t value;
		uint64_t maxDelta;
		inline bool operator<(const _Qualifier &q) const { return (id < q.id); } // sort order
	};

	Address _signedBy;
	_Qualifier _qualifiers[ZT_NETWORK_COM_MAX_QUALIFIERS];
	unsigned int _qualifierCount;
	C25519::Signature _signature;
};

} // namespace ZeroTier

#endif
