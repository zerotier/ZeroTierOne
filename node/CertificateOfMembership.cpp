/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2019  ZeroTier, Inc.  https://www.zerotier.com/
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

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
