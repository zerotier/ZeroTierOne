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

#ifndef ZT_CERTIFICATEOFOWNERSHIP_HPP
#define ZT_CERTIFICATEOFOWNERSHIP_HPP

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Constants.hpp"
#include "Credential.hpp"
#include "C25519.hpp"
#include "Address.hpp"
#include "Identity.hpp"
#include "Buffer.hpp"
#include "InetAddress.hpp"
#include "MAC.hpp"

// Max things per CertificateOfOwnership
#define ZT_CERTIFICATEOFOWNERSHIP_MAX_THINGS 16

// Maximum size of a thing's value field in bytes
#define ZT_CERTIFICATEOFOWNERSHIP_MAX_THING_VALUE_SIZE 16

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * Certificate indicating ownership of a network identifier
 */
class CertificateOfOwnership : public Credential
{
	friend class Credential;

public:
	static ZT_ALWAYS_INLINE ZT_CredentialType credentialType() { return ZT_CREDENTIAL_TYPE_COO; }

	enum Thing
	{
		THING_NULL = 0,
		THING_MAC_ADDRESS = 1,
		THING_IPV4_ADDRESS = 2,
		THING_IPV6_ADDRESS = 3
	};

	ZT_ALWAYS_INLINE CertificateOfOwnership()
	{
		memset(reinterpret_cast<void *>(this),0,sizeof(CertificateOfOwnership));
	}

	ZT_ALWAYS_INLINE CertificateOfOwnership(const uint64_t nwid,const int64_t ts,const Address &issuedTo,const uint32_t id)
	{
		memset(reinterpret_cast<void *>(this),0,sizeof(CertificateOfOwnership));
		_networkId = nwid;
		_ts = ts;
		_id = id;
		_issuedTo = issuedTo;
	}

	ZT_ALWAYS_INLINE uint64_t networkId() const { return _networkId; }
	ZT_ALWAYS_INLINE int64_t timestamp() const { return _ts; }
	ZT_ALWAYS_INLINE uint32_t id() const { return _id; }
	ZT_ALWAYS_INLINE const Address &issuedTo() const { return _issuedTo; }
	ZT_ALWAYS_INLINE const Address &signer() const { return _signedBy; }
	ZT_ALWAYS_INLINE const uint8_t *signature() const { return _signature; }
	ZT_ALWAYS_INLINE unsigned int signatureLength() const { return _signatureLength; }

	ZT_ALWAYS_INLINE unsigned int thingCount() const { return (unsigned int)_thingCount; }
	ZT_ALWAYS_INLINE Thing thingType(const unsigned int i) const { return (Thing)_thingTypes[i]; }
	ZT_ALWAYS_INLINE const uint8_t *thingValue(const unsigned int i) const { return _thingValues[i]; }

	ZT_ALWAYS_INLINE bool owns(const InetAddress &ip) const
	{
		if (ip.ss_family == AF_INET)
			return this->_owns(THING_IPV4_ADDRESS,&(reinterpret_cast<const struct sockaddr_in *>(&ip)->sin_addr.s_addr),4);
		if (ip.ss_family == AF_INET6)
			return this->_owns(THING_IPV6_ADDRESS,reinterpret_cast<const struct sockaddr_in6 *>(&ip)->sin6_addr.s6_addr,16);
		return false;
	}

	ZT_ALWAYS_INLINE bool owns(const MAC &mac) const
	{
		uint8_t tmp[6];
		mac.copyTo(tmp,6);
		return this->_owns(THING_MAC_ADDRESS,tmp,6);
	}

	ZT_ALWAYS_INLINE void addThing(const InetAddress &ip)
	{
		if (_thingCount >= ZT_CERTIFICATEOFOWNERSHIP_MAX_THINGS) return;
		if (ip.ss_family == AF_INET) {
			_thingTypes[_thingCount] = THING_IPV4_ADDRESS;
			memcpy(_thingValues[_thingCount],&(reinterpret_cast<const struct sockaddr_in *>(&ip)->sin_addr.s_addr),4);
			++_thingCount;
		} else if (ip.ss_family == AF_INET6) {
			_thingTypes[_thingCount] = THING_IPV6_ADDRESS;
			memcpy(_thingValues[_thingCount],reinterpret_cast<const struct sockaddr_in6 *>(&ip)->sin6_addr.s6_addr,16);
			++_thingCount;
		}
	}

	ZT_ALWAYS_INLINE void addThing(const MAC &mac)
	{
		if (_thingCount >= ZT_CERTIFICATEOFOWNERSHIP_MAX_THINGS) return;
		_thingTypes[_thingCount] = THING_MAC_ADDRESS;
		mac.copyTo(_thingValues[_thingCount],6);
		++_thingCount;
	}

	/**
	 * @param signer Signing identity, must have private key
	 * @return True if signature was successful
	 */
	ZT_ALWAYS_INLINE bool sign(const Identity &signer)
	{
		if (signer.hasPrivate()) {
			Buffer<sizeof(CertificateOfOwnership) + 64> tmp;
			_signedBy = signer.address();
			this->serialize(tmp,true);
			_signatureLength = signer.sign(tmp.data(),tmp.size(),_signature,sizeof(_signature));
			return true;
		}
		return false;
	}

	ZT_ALWAYS_INLINE Credential::VerifyResult verify(const RuntimeEnvironment *RR,void *tPtr) const { return _verify(RR,tPtr,*this); }

	template<unsigned int C>
	inline void serialize(Buffer<C> &b,const bool forSign = false) const
	{
		if (forSign) b.append((uint64_t)0x7f7f7f7f7f7f7f7fULL);

		b.append(_networkId);
		b.append(_ts);
		b.append(_flags);
		b.append(_id);
		b.append((uint16_t)_thingCount);
		for(unsigned int i=0,j=_thingCount;i<j;++i) {
			b.append((uint8_t)_thingTypes[i]);
			b.append(_thingValues[i],ZT_CERTIFICATEOFOWNERSHIP_MAX_THING_VALUE_SIZE);
		}

		_issuedTo.appendTo(b);
		_signedBy.appendTo(b);
		if (!forSign) {
			b.append((uint8_t)1);
			b.append((uint16_t)_signatureLength); // length of signature
			b.append(_signature,_signatureLength);
		}

		b.append((uint16_t)0); // length of additional fields, currently 0

		if (forSign) b.append((uint64_t)0x7f7f7f7f7f7f7f7fULL);
	}

	template<unsigned int C>
	inline unsigned int deserialize(const Buffer<C> &b,unsigned int startAt = 0)
	{
		unsigned int p = startAt;

		*this = CertificateOfOwnership();

		_networkId = b.template at<uint64_t>(p); p += 8;
		_ts = b.template at<uint64_t>(p); p += 8;
		_flags = b.template at<uint64_t>(p); p += 8;
		_id = b.template at<uint32_t>(p); p += 4;
		_thingCount = b.template at<uint16_t>(p); p += 2;
		for(unsigned int i=0,j=_thingCount;i<j;++i) {
			if (i < ZT_CERTIFICATEOFOWNERSHIP_MAX_THINGS) {
				_thingTypes[i] = (uint8_t)b[p++];
				memcpy(_thingValues[i],b.field(p,ZT_CERTIFICATEOFOWNERSHIP_MAX_THING_VALUE_SIZE),ZT_CERTIFICATEOFOWNERSHIP_MAX_THING_VALUE_SIZE);
				p += ZT_CERTIFICATEOFOWNERSHIP_MAX_THING_VALUE_SIZE;
			}
		}

		_issuedTo.setTo(b.field(p,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH); p += ZT_ADDRESS_LENGTH;
		_signedBy.setTo(b.field(p,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH); p += ZT_ADDRESS_LENGTH;
		if (b[p++] == 1) {
			_signatureLength = b.template at<uint16_t>(p);
			if (_signatureLength > sizeof(_signature))
				throw ZT_EXCEPTION_INVALID_SERIALIZED_DATA_INVALID_CRYPTOGRAPHIC_TOKEN;
			p += 2;
			memcpy(_signature,b.field(p,_signatureLength),_signatureLength); p += _signatureLength;
		} else {
			p += 2 + b.template at<uint16_t>(p);
		}

		p += 2 + b.template at<uint16_t>(p);
		if (p > b.size())
			throw ZT_EXCEPTION_INVALID_SERIALIZED_DATA_OVERFLOW;

		return (p - startAt);
	}

	// Provides natural sort order by ID
	ZT_ALWAYS_INLINE bool operator<(const CertificateOfOwnership &coo) const { return (_id < coo._id); }

	ZT_ALWAYS_INLINE bool operator==(const CertificateOfOwnership &coo) const { return (memcmp(this,&coo,sizeof(CertificateOfOwnership)) == 0); }
	ZT_ALWAYS_INLINE bool operator!=(const CertificateOfOwnership &coo) const { return (memcmp(this,&coo,sizeof(CertificateOfOwnership)) != 0); }

private:
	ZT_ALWAYS_INLINE bool _owns(const Thing &t,const void *v,unsigned int l) const
	{
		for(unsigned int i=0,j=_thingCount;i<j;++i) {
			if (_thingTypes[i] == (uint8_t)t) {
				unsigned int k = 0;
				while (k < l) {
					if (reinterpret_cast<const uint8_t *>(v)[k] != _thingValues[i][k])
						break;
					++k;
				}
				if (k == l)
					return true;
			}
		}
		return false;
	}

	uint64_t _networkId;
	int64_t _ts;
	uint64_t _flags;
	uint32_t _id;
	uint16_t _thingCount;
	uint8_t _thingTypes[ZT_CERTIFICATEOFOWNERSHIP_MAX_THINGS];
	uint8_t _thingValues[ZT_CERTIFICATEOFOWNERSHIP_MAX_THINGS][ZT_CERTIFICATEOFOWNERSHIP_MAX_THING_VALUE_SIZE];
	Address _issuedTo;
	Address _signedBy;
	unsigned int _signatureLength;
	uint8_t _signature[ZT_SIGNATURE_BUFFER_SIZE];
};

} // namespace ZeroTier

#endif
