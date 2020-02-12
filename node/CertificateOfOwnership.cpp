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

#include "CertificateOfOwnership.hpp"

namespace ZeroTier {

void CertificateOfOwnership::addThing(const InetAddress &ip)
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

void CertificateOfOwnership::addThing(const MAC &mac)
{
	if (_thingCount >= ZT_CERTIFICATEOFOWNERSHIP_MAX_THINGS) return;
	_thingTypes[_thingCount] = THING_MAC_ADDRESS;
	mac.copyTo(_thingValues[_thingCount]);
	++_thingCount;
}

bool CertificateOfOwnership::sign(const Identity &signer)
{
	uint8_t buf[ZT_CERTIFICATEOFOWNERSHIP_MARSHAL_SIZE_MAX + 16];
	if (signer.hasPrivate()) {
		_signedBy = signer.address();
		_signatureLength = signer.sign(buf,(unsigned int)marshal(buf,true),_signature,sizeof(_signature));
		return true;
	}
	return false;
}

int CertificateOfOwnership::marshal(uint8_t data[ZT_CERTIFICATEOFOWNERSHIP_MARSHAL_SIZE_MAX],bool forSign) const noexcept
{
	int p = 0;
	if (forSign) {
		for(int k=0;k<16;++k)
			data[p++] = 0x7f;
	}
	Utils::storeBigEndian<uint64_t>(data + p,_networkId);
	Utils::storeBigEndian<uint64_t>(data + p + 8,(uint64_t)_ts);
	Utils::storeBigEndian<uint64_t>(data + p + 16,_flags);
	Utils::storeBigEndian<uint32_t>(data + p + 24,_id);
	Utils::storeBigEndian<uint16_t>(data + p + 28,(uint16_t)_thingCount);
	p += 30;
	for(unsigned int i=0,j=_thingCount;i<j;++i) {
		data[p++] = _thingTypes[i];
		memcpy(data + p,_thingValues[i],ZT_CERTIFICATEOFOWNERSHIP_MAX_THING_VALUE_SIZE);
		p += ZT_CERTIFICATEOFOWNERSHIP_MAX_THING_VALUE_SIZE;
	}
	_issuedTo.copyTo(data + p); p += ZT_ADDRESS_LENGTH;
	_signedBy.copyTo(data + p); p += ZT_ADDRESS_LENGTH;
	if (!forSign) {
		data[p++] = 1;
		Utils::storeBigEndian<uint16_t>(data + p,(uint16_t)_signatureLength); p += 2;
		memcpy(data + p,_signature,_signatureLength); p += (int)_signatureLength;
	}
	data[p++] = 0;
	data[p++] = 0;
	if (forSign) {
		for(int k=0;k<16;++k)
			data[p++] = 0x7f;
	}
	return p;
}

int CertificateOfOwnership::unmarshal(const uint8_t *data,int len) noexcept
{
	if (len < 30)
		return -1;

	_networkId = Utils::loadBigEndian<uint64_t>(data);
	_ts = (int64_t)Utils::loadBigEndian<uint64_t>(data + 8);
	_flags = Utils::loadBigEndian<uint64_t>(data + 16);
	_id = Utils::loadBigEndian<uint32_t>(data + 24);
	_thingCount = Utils::loadBigEndian<uint16_t>(data + 28);
	if (_thingCount > ZT_CERTIFICATEOFOWNERSHIP_MAX_THINGS)
		return -1;
	int p = 30;

	for(unsigned int i=0,j=_thingCount;i<j;++i) {
		if ((p + 1 + ZT_CERTIFICATEOFOWNERSHIP_MAX_THING_VALUE_SIZE) > len)
			return -1;
		_thingTypes[i] = data[p++];
		memcpy(_thingValues[i],data + p,ZT_CERTIFICATEOFOWNERSHIP_MAX_THING_VALUE_SIZE);
		p += ZT_CERTIFICATEOFOWNERSHIP_MAX_THING_VALUE_SIZE;
	}

	if ((p + ZT_ADDRESS_LENGTH + ZT_ADDRESS_LENGTH + 1 + 2) > len)
		return -1;
	_issuedTo.setTo(data + p); p += ZT_ADDRESS_LENGTH;
	_signedBy.setTo(data + p); p += ZT_ADDRESS_LENGTH + 1;

	p += 2 + Utils::loadBigEndian<uint16_t>(data + p);
	if (p > len)
		return -1;
	return p;
}

} // namespace ZeroTier
