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

CertificateOfMembership::CertificateOfMembership(const int64_t timestamp,const int64_t timestampMaxDelta,const uint64_t nwid,const Identity &issuedTo) noexcept :
	_timestamp(timestamp),
	_timestampMaxDelta(timestampMaxDelta),
	_networkId(nwid),
	_issuedTo(issuedTo.fingerprint()),
	_signatureLength(0) {}

bool CertificateOfMembership::agreesWith(const CertificateOfMembership &other) const noexcept
{
	// NOTE: we always do explicit absolute value with an if() since llabs() can have overflow
	// conditions that could introduce a vulnerability.

	if (other._timestamp > _timestamp) {
		if ((other._timestamp - _timestamp) > _timestampMaxDelta)
			return false;
	} else {
		if ((_timestamp - other._timestamp) > _timestampMaxDelta)
			return false;
	}

	// us <> them
	for(FCV<_Qualifier,ZT_CERTIFICATEOFMEMBERSHIP_MAX_ADDITIONAL_QUALIFIERS>::const_iterator i(_additionalQualifiers.begin());i != _additionalQualifiers.end();++i) {
		if (i->delta != 0xffffffffffffffffULL) {
			const uint64_t *v2 = nullptr;
			for(FCV<_Qualifier,ZT_CERTIFICATEOFMEMBERSHIP_MAX_ADDITIONAL_QUALIFIERS>::const_iterator j(other._additionalQualifiers.begin());j != other._additionalQualifiers.end();++i) {
				if (j->id == i->id) {
					v2 = &(j->value);
					break;
				}
			}
			if (!v2)
				return false;
			if (*v2 > i->value) {
				if ((*v2 - i->value) > i->delta)
					return false;
			} else {
				if ((i->value - *v2) > i->delta)
					return false;
			}
		}
	}

	// them <> us
	for(FCV<_Qualifier,ZT_CERTIFICATEOFMEMBERSHIP_MAX_ADDITIONAL_QUALIFIERS>::const_iterator i(other._additionalQualifiers.begin());i != other._additionalQualifiers.end();++i) {
		if (i->delta != 0xffffffffffffffffULL) {
			const uint64_t *v2 = nullptr;
			for(FCV<_Qualifier,ZT_CERTIFICATEOFMEMBERSHIP_MAX_ADDITIONAL_QUALIFIERS>::const_iterator j(_additionalQualifiers.begin());j != _additionalQualifiers.end();++i) {
				if (j->id == i->id) {
					v2 = &(j->value);
					break;
				}
			}
			if (!v2)
				return false;
			if (*v2 > i->value) {
				if ((*v2 - i->value) > i->delta)
					return false;
			} else {
				if ((i->value - *v2) > i->delta)
					return false;
			}
		}
	}

	// SECURITY: check for issued-to inequality is a sanity check. This should be impossible since elsewhere
	// in the code COMs are checked to ensure that they do in fact belong to their issued-to identities.
	return (other._networkId != _networkId) && (other._issuedTo.address() != _issuedTo.address());
}

bool CertificateOfMembership::sign(const Identity &with) noexcept
{
	_signedBy = with.address();
	uint64_t buf[ZT_CERTIFICATEOFMEMBERSHIP_MARSHAL_SIZE_MAX / 8];
	const unsigned int bufSize = _fillSigningBuf(buf);
	_signatureLength = with.sign(buf,bufSize,_signature,sizeof(_signature));
	return _signatureLength > 0;
}

int CertificateOfMembership::marshal(uint8_t data[ZT_CERTIFICATEOFMEMBERSHIP_MARSHAL_SIZE_MAX],const bool v2) const noexcept
{
	data[0] = v2 ? 2 : 1;

	// All formats start with the standard three qualifiers: timestamp with delta, network ID as a strict
	// equality compare, and the address of the issued-to node as an informational tuple.
	int p = 3;
	Utils::storeBigEndian<uint64_t>(data + p,0); p += 8;
	Utils::storeBigEndian<uint64_t>(data + p,(uint64_t)_timestamp); p += 8;
	Utils::storeBigEndian<uint64_t>(data + p,(uint64_t)_timestampMaxDelta); p += 8;
	Utils::storeBigEndian<uint64_t>(data + p,1); p += 8;
	Utils::storeBigEndian<uint64_t>(data + p,_networkId); p += 8;
	Utils::storeBigEndian<uint64_t>(data + p,0); p += 8;
	Utils::storeBigEndian<uint64_t>(data + p,2); p += 8;
	Utils::storeBigEndian<uint64_t>(data + p,_issuedTo.address().toInt()); p += 8;
	Utils::storeAsIsEndian<uint64_t>(data + p,0xffffffffffffffffULL); p += 8;

	if (v2) {
		// V2 marshal format will have three tuples followed by the fingerprint hash.
		Utils::storeBigEndian<uint16_t>(data + 1,3);
		memcpy(data + p,_issuedTo.hash(),48);
		p += 48;
	} else {
		// V1 marshal format must shove everything into tuples, resulting in nine.
		Utils::storeBigEndian<uint16_t>(data + 1,9);
		for(int k=0;k<6;++k) {
			Utils::storeBigEndian<uint64_t>(data + p,(uint64_t)k + 3); p += 8;
			Utils::storeAsIsEndian<uint64_t>(data + p,Utils::loadAsIsEndian<uint64_t>(_issuedTo.hash() + (k * 8))); p += 8;
			Utils::storeAsIsEndian<uint64_t>(data + p,0xffffffffffffffffULL); p += 8;
		}
	}

	_signedBy.copyTo(data + p); p += 5;

	if (v2) {
		// V2 marshal format prefixes signatures with a 16-bit length to support future signature types.
		Utils::storeBigEndian<uint16_t>(data + p,(uint16_t)_signatureLength); p += 2;
		memcpy(data + p,_signature,_signatureLength);
		p += (int)_signatureLength;
	} else {
		// V1 only supports 96-byte signature fields.
		memcpy(data + p,_signature,96);
		p += 96;
	}

	return p;
}

int CertificateOfMembership::unmarshal(const uint8_t *data,int len) noexcept
{
	if (len < (1 + 2 + 72))
		return -1;

	TriviallyCopyable::memoryZero(this);

	const unsigned int numq = Utils::loadBigEndian<uint16_t>(data + 1);
	if ((numq < 3)||(numq > (ZT_CERTIFICATEOFMEMBERSHIP_MAX_ADDITIONAL_QUALIFIERS + 3)))
		return -1;
	int p = 3;
	for(unsigned int q=0;q<numq;++q) {
		if ((p + 24) > len)
			return -1;
		const uint64_t id = Utils::loadBigEndian<uint64_t>(data + p); p += 8;
		const uint64_t value = Utils::loadBigEndian<uint64_t>(data + p); p += 8;
		const uint64_t delta = Utils::loadBigEndian<uint64_t>(data + p); p += 8;
		switch(id) {
			case 0:
				_timestamp = (int64_t)value;
				_timestampMaxDelta = (int64_t)delta;
				break;
			case 1:
				_networkId = value;
				break;
			case 2:
				_issuedTo.apiFingerprint()->address = value;
				break;

				// V1 nodes will pack the hash into qualifier tuples.
			case 3:
				Utils::storeBigEndian<uint64_t>(_issuedTo.apiFingerprint()->hash,value);
				break;
			case 4:
				Utils::storeBigEndian<uint64_t>(_issuedTo.apiFingerprint()->hash + 8,value);
				break;
			case 5:
				Utils::storeBigEndian<uint64_t>(_issuedTo.apiFingerprint()->hash + 16,value);
				break;
			case 6:
				Utils::storeBigEndian<uint64_t>(_issuedTo.apiFingerprint()->hash + 24,value);
				break;
			case 7:
				Utils::storeBigEndian<uint64_t>(_issuedTo.apiFingerprint()->hash + 32,value);
				break;
			case 8:
				Utils::storeBigEndian<uint64_t>(_issuedTo.apiFingerprint()->hash + 40,value);
				break;

			default:
				if (_additionalQualifiers.size() == _additionalQualifiers.capacity())
					return -1;
				_additionalQualifiers.push_back(_Qualifier(id,value,delta));
				break;
		}
	}

	std::sort(_additionalQualifiers.begin(),_additionalQualifiers.end());

	if (data[0] == 1) {
		if ((p + 96) > len)
			return -1;
		_signatureLength = 96;
		memcpy(_signature,data + p,96);
		return p + 96;
	} else if (data[0] == 2) {
		if ((p + 48) > len)
			return -1;
		memcpy(_issuedTo.apiFingerprint()->hash,data + p,48);
		p += 48;
		if ((p + 2) > len)
			return -1;
		_signatureLength = Utils::loadBigEndian<uint16_t>(data + p);
		if ((_signatureLength > sizeof(_signature))||((p + _signatureLength) > len))
			return -1;
		memcpy(_signature,data + p,_signatureLength);
		return p + (int)_signatureLength;
	}

	return -1;
}

unsigned int CertificateOfMembership::_fillSigningBuf(uint64_t buf[ZT_CERTIFICATEOFMEMBERSHIP_MARSHAL_SIZE_MAX / 8]) const noexcept
{
	const uint64_t informational = 0xffffffffffffffffULL;

	/*
	 * Signing always embeds all data to be signed in qualifier tuple format for
	 * backward compatibility with V1 nodes, since otherwise we'd need a signature
	 * for v1 nodes to verify and another for v2 nodes to verify.
	 */

	// The standard three tuples that must begin every COM.
	buf[0] = 0;
	buf[1] = Utils::hton((uint64_t)_timestamp);
	buf[2] = Utils::hton((uint64_t)_timestampMaxDelta);
	buf[3] = ZT_CONST_TO_BE_UINT64(1);
	buf[4] = Utils::hton(_networkId);
	buf[5] = 0;
	buf[6] = ZT_CONST_TO_BE_UINT64(2);
	buf[7] = Utils::hton(_issuedTo.address().toInt());
	buf[8] = informational;

	unsigned int p = 9;

	// The full identity fingerprint of the peer to whom the COM was issued,
	// embeded as a series of informational tuples.
	if (_issuedTo.haveHash()) {
		buf[p++] = ZT_CONST_TO_BE_UINT64(3);
		buf[p++] = Utils::loadAsIsEndian<uint64_t>(_issuedTo.hash());
		buf[p++] = informational;
		buf[p++] = ZT_CONST_TO_BE_UINT64(4);
		buf[p++] = Utils::loadAsIsEndian<uint64_t>(_issuedTo.hash() + 8);
		buf[p++] = informational;
		buf[p++] = ZT_CONST_TO_BE_UINT64(5);
		buf[p++] = Utils::loadAsIsEndian<uint64_t>(_issuedTo.hash() + 16);
		buf[p++] = informational;
		buf[p++] = ZT_CONST_TO_BE_UINT64(6);
		buf[p++] = Utils::loadAsIsEndian<uint64_t>(_issuedTo.hash() + 24);
		buf[p++] = informational;
		buf[p++] = ZT_CONST_TO_BE_UINT64(7);
		buf[p++] = Utils::loadAsIsEndian<uint64_t>(_issuedTo.hash() + 32);
		buf[p++] = informational;
		buf[p++] = ZT_CONST_TO_BE_UINT64(8);
		buf[p++] = Utils::loadAsIsEndian<uint64_t>(_issuedTo.hash() + 40);
		buf[p++] = informational;
	}

	for(FCV<_Qualifier,ZT_CERTIFICATEOFMEMBERSHIP_MAX_ADDITIONAL_QUALIFIERS>::const_iterator i(_additionalQualifiers.begin());i != _additionalQualifiers.end();++i) {
		buf[p++] = Utils::hton(i->id);
		buf[p++] = Utils::hton(i->value);
		buf[p++] = Utils::hton(i->delta);
	}

	return p * 8;
}

} // namespace ZeroTier
