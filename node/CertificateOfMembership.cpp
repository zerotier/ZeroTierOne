/*
 * Copyright (c)2019 ZeroTier, Inc.
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

#include "CertificateOfMembership.hpp"
#include "RuntimeEnvironment.hpp"
#include "Topology.hpp"
#include "Switch.hpp"
#include "Network.hpp"
#include "Node.hpp"

namespace ZeroTier {

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

#ifdef ZT_SUPPORT_OLD_STYLE_NETCONF

std::string CertificateOfMembership::toString() const
{
	char tmp[ZT_NETWORK_COM_MAX_QUALIFIERS * 32];
	std::string s;

	s.append("1:"); // COM_UINT64_ED25519

	uint64_t *const buf = new uint64_t[_qualifierCount * 3];
	try {
		unsigned int ptr = 0;
		for(unsigned int i=0;i<_qualifierCount;++i) {
			buf[ptr++] = Utils::hton(_qualifiers[i].id);
			buf[ptr++] = Utils::hton(_qualifiers[i].value);
			buf[ptr++] = Utils::hton(_qualifiers[i].maxDelta);
		}
		s.append(Utils::hex(buf,ptr * sizeof(uint64_t),tmp));
		delete [] buf;
	} catch ( ... ) {
		delete [] buf;
		throw;
	}

	s.push_back(':');

	s.append(_signedBy.toString(tmp));

	if (_signedBy) {
		s.push_back(':');
		s.append(Utils::hex(_signature.data,ZT_C25519_SIGNATURE_LEN,tmp));
	}

	return s;
}

void CertificateOfMembership::fromString(const char *s)
{
	_qualifierCount = 0;
	_signedBy.zero();
	memset(_signature.data,0,ZT_C25519_SIGNATURE_LEN);

	if (!*s)
		return;

	unsigned int colonAt = 0;
	while ((s[colonAt])&&(s[colonAt] != ':')) ++colonAt;

	if (!((colonAt == 1)&&(s[0] == '1'))) // COM_UINT64_ED25519?
		return;

	s += colonAt + 1;
	colonAt = 0;
	while ((s[colonAt])&&(s[colonAt] != ':')) ++colonAt;

	if (colonAt) {
		const unsigned int buflen = colonAt / 2;
		char *const buf = new char[buflen];
		unsigned int bufactual = Utils::unhex(s,colonAt,buf,buflen);
		char *bufptr = buf;
		try {
			while (bufactual >= 24) {
				if (_qualifierCount < ZT_NETWORK_COM_MAX_QUALIFIERS) {
					_qualifiers[_qualifierCount].id = Utils::ntoh(*((uint64_t *)bufptr)); bufptr += 8;
					_qualifiers[_qualifierCount].value = Utils::ntoh(*((uint64_t *)bufptr)); bufptr += 8;
					_qualifiers[_qualifierCount].maxDelta = Utils::ntoh(*((uint64_t *)bufptr)); bufptr += 8;
					++_qualifierCount;
				} else {
					bufptr += 24;
				}
				bufactual -= 24;
			}
		} catch ( ... ) {}
		delete [] buf;
	}

	if (s[colonAt]) {
		s += colonAt + 1;
		colonAt = 0;
		while ((s[colonAt])&&(s[colonAt] != ':')) ++colonAt;

		if (colonAt) {
			char addrbuf[ZT_ADDRESS_LENGTH];
			if (Utils::unhex(s,colonAt,addrbuf,sizeof(addrbuf)) == ZT_ADDRESS_LENGTH)
				_signedBy.setTo(addrbuf,ZT_ADDRESS_LENGTH);

			if ((_signedBy)&&(s[colonAt])) {
				s += colonAt + 1;
				colonAt = 0;
				while ((s[colonAt])&&(s[colonAt] != ':')) ++colonAt;
				if (colonAt) {
					if (Utils::unhex(s,colonAt,_signature.data,ZT_C25519_SIGNATURE_LEN) != ZT_C25519_SIGNATURE_LEN)
						_signedBy.zero();
				} else {
					_signedBy.zero();
				}
			} else {
				_signedBy.zero();
			}
		}
	}

	std::sort(&(_qualifiers[0]),&(_qualifiers[_qualifierCount]));
}

#endif // ZT_SUPPORT_OLD_STYLE_NETCONF

bool CertificateOfMembership::agreesWith(const CertificateOfMembership &other, const Identity &otherIdentity) const
{
	if ((_qualifierCount == 0)||(other._qualifierCount == 0))
		return false;

	std::map< uint64_t, uint64_t > otherFields;
	for(unsigned int i=0;i<other._qualifierCount;++i)
		otherFields[other._qualifiers[i].id] = other._qualifiers[i].value;

	bool fullIdentityVerification = false;
	for(unsigned int i=0;i<_qualifierCount;++i) {
		const uint64_t qid = _qualifiers[i].id;
		if ((qid >= 3)&&(qid <= 6)) {
			fullIdentityVerification = true;
		} else {
			std::map< uint64_t, uint64_t >::iterator otherQ(otherFields.find(qid));
			if (otherQ == otherFields.end())
				return false;
			const uint64_t a = _qualifiers[i].value;
			const uint64_t b = otherQ->second;
			if (((a >= b) ? (a - b) : (b - a)) > _qualifiers[i].maxDelta)
				return false;
		}
	}

	// If this COM has a full hash of its identity, assume the other must have this as well.
	// Otherwise we are on a controller that does not incorporate these.
	if (fullIdentityVerification) {
		uint64_t idHash[6];
		otherIdentity.publicKeyHash(idHash);
		for(unsigned long i=0;i<4;++i) {
			std::map< uint64_t, uint64_t >::iterator otherQ(otherFields.find((uint64_t)(i + 3)));
			if (otherQ == otherFields.end())
				return false;
			if (otherQ->second != Utils::ntoh(idHash[i]))
				return false;
		}
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
		_signature = with.sign(buf,ptr * sizeof(uint64_t));
		_signedBy = with.address();
		return true;
	} catch ( ... ) {
		_signedBy.zero();
		return false;
	}
}

int CertificateOfMembership::verify(const RuntimeEnvironment *RR,void *tPtr) const
{
	if ((!_signedBy)||(_signedBy != Network::controllerFor(networkId()))||(_qualifierCount > ZT_NETWORK_COM_MAX_QUALIFIERS))
		return -1;

	const Identity id(RR->topology->getIdentity(tPtr,_signedBy));
	if (!id) {
		RR->sw->requestWhois(tPtr,RR->node->now(),_signedBy);
		return 1;
	}

	uint64_t buf[ZT_NETWORK_COM_MAX_QUALIFIERS * 3];
	unsigned int ptr = 0;
	for(unsigned int i=0;i<_qualifierCount;++i) {
		buf[ptr++] = Utils::hton(_qualifiers[i].id);
		buf[ptr++] = Utils::hton(_qualifiers[i].value);
		buf[ptr++] = Utils::hton(_qualifiers[i].maxDelta);
	}
	return (id.verify(buf,ptr * sizeof(uint64_t),_signature) ? 0 : -1);
}

} // namespace ZeroTier
