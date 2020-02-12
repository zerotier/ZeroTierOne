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

#include "CertificateOfMembership.hpp"

namespace ZeroTier {

CertificateOfMembership::CertificateOfMembership(uint64_t timestamp,uint64_t timestampMaxDelta,uint64_t nwid,const Address &issuedTo)
{
	_qualifiers[0].id = COM_RESERVED_ID_TIMESTAMP;
	_qualifiers[0].value = timestamp;
	_qualifiers[0].maxDelta = timestampMaxDelta;
	_qualifiers[1].id = COM_RESERVED_ID_NETWORK_ID;
	_qualifiers[1].value = nwid;
	_qualifiers[1].maxDelta = 0;
	_qualifiers[2].id = COM_RESERVED_ID_ISSUED_TO;
	_qualifiers[2].value = issuedTo.toInt();
	_qualifiers[2].maxDelta = 0xffffffffffffffffULL;
	_qualifierCount = 3;
	_signatureLength = 0;
}

void CertificateOfMembership::setQualifier(uint64_t id,uint64_t value,uint64_t maxDelta)
{
	_signedBy.zero();
	for(unsigned int i=0;i<_qualifierCount;++i) {
		if (_qualifiers[i].id == id) {
			_qualifiers[i].value = value;
			_qualifiers[i].maxDelta = maxDelta;
			return;
		}
	}
	if (_qualifierCount < ZT_NETWORK_COM_MAX_QUALIFIERS) {
		_qualifiers[_qualifierCount].id = id;
		_qualifiers[_qualifierCount].value = value;
		_qualifiers[_qualifierCount].maxDelta = maxDelta;
		++_qualifierCount;
		std::sort(&(_qualifiers[0]),&(_qualifiers[_qualifierCount]));
	}
}

bool CertificateOfMembership::agreesWith(const CertificateOfMembership &other) const
{
	unsigned int myidx = 0;
	unsigned int otheridx = 0;

	if ((_qualifierCount == 0)||(other._qualifierCount == 0))
		return false;

	while (myidx < _qualifierCount) {
		// Fail if we're at the end of other, since this means the field is
		// missing.
		if (otheridx >= other._qualifierCount)
			return false;

		// Seek to corresponding tuple in other, ignoring tuples that
		// we may not have. If we run off the end of other, the tuple is
		// missing. This works because tuples are sorted by ID.
		while (other._qualifiers[otheridx].id != _qualifiers[myidx].id) {
			++otheridx;
			if (otheridx >= other._qualifierCount)
				return false;
		}

		// Compare to determine if the absolute value of the difference
		// between these two parameters is within our maxDelta.
		const uint64_t a = _qualifiers[myidx].value;
		const uint64_t b = other._qualifiers[myidx].value;
		if (((a >= b) ? (a - b) : (b - a)) > _qualifiers[myidx].maxDelta)
			return false;

		++myidx;
	}

	return true;
}

bool CertificateOfMembership::sign(const Identity &with)
{
	uint64_t buf[ZT_NETWORK_COM_MAX_QUALIFIERS * 3];
	unsigned int ptr = 0;
	for(unsigned int i=0;i<_qualifierCount;++i) {
		buf[ptr++] = Utils::hton(_qualifiers[i].id);
		buf[ptr++] = Utils::hton(_qualifiers[i].value);
		buf[ptr++] = Utils::hton(_qualifiers[i].maxDelta);
	}

	try {
		_signatureLength = with.sign(buf,ptr * sizeof(uint64_t),_signature,sizeof(_signature));
		_signedBy = with.address();
		return true;
	} catch ( ... ) {
		_signedBy.zero();
		return false;
	}
}

int CertificateOfMembership::marshal(uint8_t data[ZT_CERTIFICATEOFMEMBERSHIP_MARSHAL_SIZE_MAX]) const noexcept
{
	data[0] = 1;
	Utils::storeBigEndian<uint16_t>(data + 1,(uint16_t)_qualifierCount);
	int p = 3;
	for(unsigned int i=0;i<_qualifierCount;++i) {
		Utils::storeBigEndian<uint64_t>(data + p,_qualifiers[i].id); p += 8;
		Utils::storeBigEndian<uint64_t>(data + p,_qualifiers[i].value); p += 8;
		Utils::storeBigEndian<uint64_t>(data + p,_qualifiers[i].maxDelta); p += 8;
	}
	_signedBy.copyTo(data + p); p += ZT_ADDRESS_LENGTH;
	if ((_signedBy)&&(_signatureLength == 96)) {
		// UGLY: Ed25519 signatures in ZT are 96 bytes (64 + 32 bytes of hash).
		// P-384 signatures are also 96 bytes, praise the horned one. That means
		// we don't need to include a length. If we ever do we will need a new
		// serialized object version, but only for those with length != 96.
		memcpy(data + p,_signature,96); p += 96;
	}
	return p;
}

int CertificateOfMembership::unmarshal(const uint8_t *data,int len) noexcept
{
	if ((len < 3)||(data[0] != 1))
		return -1;
	unsigned int numq = Utils::loadBigEndian<uint16_t>(data + 1);
	if (numq > ZT_NETWORK_COM_MAX_QUALIFIERS)
		return -1;
	_qualifierCount = numq;
	int p = 3;
	for(unsigned int i=0;i<numq;++i) {
		if ((p + 24) > len)
			return -1;
		_qualifiers[i].id = Utils::loadBigEndian<uint64_t>(data + p); p += 8;
		_qualifiers[i].value = Utils::loadBigEndian<uint64_t>(data + p); p += 8;
		_qualifiers[i].maxDelta = Utils::loadBigEndian<uint64_t>(data + p); p += 8;
	}
	if ((p + ZT_ADDRESS_LENGTH) > len)
		return -1;
	_signedBy.setTo(data + p); p += ZT_ADDRESS_LENGTH;
	if (_signedBy) {
		if ((p + 96) > len)
			return -1;
		_signatureLength = 96;
		memcpy(_signature,data + p,96);
		p += 96;
	}
	return p;
}

bool CertificateOfMembership::operator==(const CertificateOfMembership &c) const
{
	if (_signedBy != c._signedBy)
		return false;
	if (_qualifierCount != c._qualifierCount)
		return false;
	if (_signatureLength != c._signatureLength)
		return false;
	for(unsigned int i=0;i<_qualifierCount;++i) {
		const _Qualifier &a = _qualifiers[i];
		const _Qualifier &b = c._qualifiers[i];
		if ((a.id != b.id)||(a.value != b.value)||(a.maxDelta != b.maxDelta))
			return false;
	}
	return (memcmp(_signature,c._signature,_signatureLength) == 0);
}

} // namespace ZeroTier
