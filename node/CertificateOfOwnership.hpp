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
 * Certificate indicating ownership of a "thing" such as an IP address
 *
 * These are used in conjunction with the rules engine to make IP addresses and
 * other identifiers un-spoofable.
 */
class CertificateOfOwnership : public Credential
{
	friend class Credential;

public:
	static constexpr ZT_CredentialType credentialType() noexcept { return ZT_CREDENTIAL_TYPE_COO; }

	enum Thing
	{
		THING_NULL = 0,
		THING_MAC_ADDRESS = 1,
		THING_IPV4_ADDRESS = 2,
		THING_IPV6_ADDRESS = 3
	};

	ZT_ALWAYS_INLINE CertificateOfOwnership() noexcept { memoryZero(this); }

	ZT_ALWAYS_INLINE CertificateOfOwnership(const uint64_t nwid,const int64_t ts,const Address &issuedTo,const uint32_t id) noexcept
	{
		memset(reinterpret_cast<void *>(this),0,sizeof(CertificateOfOwnership));
		_networkId = nwid;
		_ts = ts;
		_id = id;
		_issuedTo = issuedTo;
	}

	ZT_ALWAYS_INLINE uint64_t networkId() const noexcept { return _networkId; }
	ZT_ALWAYS_INLINE int64_t timestamp() const noexcept { return _ts; }
	ZT_ALWAYS_INLINE uint32_t id() const noexcept { return _id; }
	ZT_ALWAYS_INLINE const Address &issuedTo() const noexcept { return _issuedTo; }
	ZT_ALWAYS_INLINE const Address &signer() const noexcept { return _signedBy; }
	ZT_ALWAYS_INLINE const uint8_t *signature() const noexcept { return _signature; }
	ZT_ALWAYS_INLINE unsigned int signatureLength() const noexcept { return _signatureLength; }

	ZT_ALWAYS_INLINE unsigned int thingCount() const noexcept { return (unsigned int)_thingCount; }
	ZT_ALWAYS_INLINE Thing thingType(const unsigned int i) const noexcept { return (Thing)_thingTypes[i]; }
	ZT_ALWAYS_INLINE const uint8_t *thingValue(const unsigned int i) const noexcept { return _thingValues[i]; }

	ZT_ALWAYS_INLINE bool owns(const InetAddress &ip) const noexcept
	{
		if (ip.family() == AF_INET)
			return this->_owns(THING_IPV4_ADDRESS,&(reinterpret_cast<const struct sockaddr_in *>(&ip)->sin_addr.s_addr),4);
		if (ip.family() == AF_INET6)
			return this->_owns(THING_IPV6_ADDRESS,reinterpret_cast<const struct sockaddr_in6 *>(&ip)->sin6_addr.s6_addr,16);
		return false;
	}

	ZT_ALWAYS_INLINE bool owns(const MAC &mac) const noexcept
	{
		uint8_t tmp[6];
		mac.copyTo(tmp);
		return this->_owns(THING_MAC_ADDRESS,tmp,6);
	}

	/**
	 * Add an IP address to this certificate
	 *
	 * @param ip IPv4 or IPv6 address
	 */
	void addThing(const InetAddress &ip);

	/**
	 * Add an Ethernet MAC address
	 *
	 * ZeroTier MAC addresses are always un-spoofable. This could in theory be
	 * used to make bridged MAC addresses un-spoofable as well, but it's not
	 * currently implemented.
	 *
	 * @param mac 48-bit MAC address
	 */
	void addThing(const MAC &mac);

	/**
	 * Sign this certificate
	 *
	 * @param signer Signing identity, must have private key
	 * @return True if signature was successful
	 */
	bool sign(const Identity &signer);

	/**
	 * Verify certificate signature
	 *
	 * @param RR Runtime environment
	 * @param tPtr That pointer we pass around
	 * @return Credential verification result: OK, bad signature, or identity needed
	 */
	ZT_ALWAYS_INLINE Credential::VerifyResult verify(const RuntimeEnvironment *RR,void *tPtr) const { return _verify(RR,tPtr,*this); }

	static constexpr int marshalSizeMax() noexcept { return ZT_CERTIFICATEOFOWNERSHIP_MARSHAL_SIZE_MAX; }
	int marshal(uint8_t data[ZT_CERTIFICATEOFOWNERSHIP_MARSHAL_SIZE_MAX],bool forSign = false) const noexcept;
	int unmarshal(const uint8_t *data,int len) noexcept;

	// Provides natural sort order by ID
	ZT_ALWAYS_INLINE bool operator<(const CertificateOfOwnership &coo) const noexcept { return (_id < coo._id); }

	ZT_ALWAYS_INLINE bool operator==(const CertificateOfOwnership &coo) const noexcept { return (memcmp(this,&coo,sizeof(CertificateOfOwnership)) == 0); }
	ZT_ALWAYS_INLINE bool operator!=(const CertificateOfOwnership &coo) const noexcept { return (memcmp(this,&coo,sizeof(CertificateOfOwnership)) != 0); }

private:
	ZT_ALWAYS_INLINE bool _owns(const Thing &t,const void *v,unsigned int l) const noexcept
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
