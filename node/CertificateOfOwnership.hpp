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
#include "InetAddress.hpp"
#include "MAC.hpp"

// Max things per CertificateOfOwnership
#define ZT_CERTIFICATEOFOWNERSHIP_MAX_THINGS 16

// Maximum size of a thing's value field in bytes
#define ZT_CERTIFICATEOFOWNERSHIP_MAX_THING_VALUE_SIZE 16

#define ZT_CERTIFICATEOFOWNERSHIP_MARSHAL_SIZE_MAX (8 + 8 + 8 + 4 + 2 + ((1 + ZT_CERTIFICATEOFOWNERSHIP_MAX_THING_VALUE_SIZE) * ZT_CERTIFICATEOFOWNERSHIP_MAX_THINGS) + 5 + 5 + 1 + 2 + ZT_SIGNATURE_BUFFER_SIZE + 2)

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

	void addThing(const InetAddress &ip);
	void addThing(const MAC &mac);

	/**
	 * @param signer Signing identity, must have private key
	 * @return True if signature was successful
	 */
	bool sign(const Identity &signer);

	ZT_ALWAYS_INLINE Credential::VerifyResult verify(const RuntimeEnvironment *RR,void *tPtr) const { return _verify(RR,tPtr,*this); }

	static ZT_ALWAYS_INLINE int marshalSizeMax() { return ZT_CERTIFICATEOFOWNERSHIP_MARSHAL_SIZE_MAX; }
	int marshal(uint8_t data[ZT_CERTIFICATEOFOWNERSHIP_MARSHAL_SIZE_MAX],bool forSign = false) const;
	int unmarshal(const uint8_t *data,int len);

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
