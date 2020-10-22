/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_CERTIFICATEOFOWNERSHIP_HPP
#define ZT_CERTIFICATEOFOWNERSHIP_HPP

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
class OwnershipCredential : public Credential
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

	ZT_INLINE OwnershipCredential() noexcept { memoryZero(this); }

	ZT_INLINE OwnershipCredential(const uint64_t nwid, const int64_t ts, const Address &issuedTo, const uint32_t id) noexcept
	{
		memoryZero(this);
		m_networkId = nwid;
		m_ts = ts;
		m_id = id;
		m_issuedTo = issuedTo;
	}

	ZT_INLINE uint64_t networkId() const noexcept { return m_networkId; }
	ZT_INLINE int64_t timestamp() const noexcept { return m_ts; }
	ZT_INLINE uint32_t id() const noexcept { return m_id; }
	ZT_INLINE const Address &issuedTo() const noexcept { return m_issuedTo; }
	ZT_INLINE const Address &signer() const noexcept { return m_signedBy; }
	ZT_INLINE const uint8_t *signature() const noexcept { return m_signature; }
	ZT_INLINE unsigned int signatureLength() const noexcept { return m_signatureLength; }

	ZT_INLINE unsigned int thingCount() const noexcept { return (unsigned int)m_thingCount; }
	ZT_INLINE Thing thingType(const unsigned int i) const noexcept { return (Thing)m_thingTypes[i]; }
	ZT_INLINE const uint8_t *thingValue(const unsigned int i) const noexcept { return m_thingValues[i]; }

	ZT_INLINE bool owns(const InetAddress &ip) const noexcept
	{
		if (ip.family() == AF_INET)
			return this->_owns(THING_IPV4_ADDRESS,&(reinterpret_cast<const struct sockaddr_in *>(&ip)->sin_addr.s_addr),4);
		if (ip.family() == AF_INET6)
			return this->_owns(THING_IPV6_ADDRESS,reinterpret_cast<const struct sockaddr_in6 *>(&ip)->sin6_addr.s6_addr,16);
		return false;
	}

	ZT_INLINE bool owns(const MAC &mac) const noexcept
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
	ZT_INLINE Credential::VerifyResult verify(const RuntimeEnvironment *RR,void *tPtr) const { return _verify(RR,tPtr,*this); }

	static constexpr int marshalSizeMax() noexcept { return ZT_CERTIFICATEOFOWNERSHIP_MARSHAL_SIZE_MAX; }
	int marshal(uint8_t data[ZT_CERTIFICATEOFOWNERSHIP_MARSHAL_SIZE_MAX],bool forSign = false) const noexcept;
	int unmarshal(const uint8_t *data,int len) noexcept;

	// Provides natural sort order by ID
	ZT_INLINE bool operator<(const OwnershipCredential &coo) const noexcept { return (m_id < coo.m_id); }

	ZT_INLINE bool operator==(const OwnershipCredential &coo) const noexcept { return (memcmp(this, &coo, sizeof(OwnershipCredential)) == 0); }
	ZT_INLINE bool operator!=(const OwnershipCredential &coo) const noexcept { return (memcmp(this, &coo, sizeof(OwnershipCredential)) != 0); }

private:
	ZT_INLINE bool _owns(const Thing &t,const void *v,unsigned int l) const noexcept
	{
		for(unsigned int i=0,j=m_thingCount;i < j;++i) {
			if (m_thingTypes[i] == (uint8_t)t) {
				unsigned int k = 0;
				while (k < l) {
					if (reinterpret_cast<const uint8_t *>(v)[k] != m_thingValues[i][k])
						break;
					++k;
				}
				if (k == l)
					return true;
			}
		}
		return false;
	}

	uint64_t m_networkId;
	int64_t m_ts;
	uint64_t m_flags;
	uint32_t m_id;
	uint16_t m_thingCount;
	uint8_t m_thingTypes[ZT_CERTIFICATEOFOWNERSHIP_MAX_THINGS];
	uint8_t m_thingValues[ZT_CERTIFICATEOFOWNERSHIP_MAX_THINGS][ZT_CERTIFICATEOFOWNERSHIP_MAX_THING_VALUE_SIZE];
	Address m_issuedTo;
	Address m_signedBy;
	unsigned int m_signatureLength;
	uint8_t m_signature[ZT_SIGNATURE_BUFFER_SIZE];
};

} // namespace ZeroTier

#endif
