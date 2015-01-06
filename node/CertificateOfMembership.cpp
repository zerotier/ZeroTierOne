/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2011-2015  ZeroTier Networks
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

#include <algorithm>

#include "CertificateOfMembership.hpp"

namespace ZeroTier {

void CertificateOfMembership::setQualifier(uint64_t id,uint64_t value,uint64_t maxDelta)
{
	_signedBy.zero();

	for(std::vector<_Qualifier>::iterator q(_qualifiers.begin());q!=_qualifiers.end();++q) {
		if (q->id == id) {
			q->value = value;
			q->maxDelta = maxDelta;
			return;
		}
	}

	_qualifiers.push_back(_Qualifier(id,value,maxDelta));
	std::sort(_qualifiers.begin(),_qualifiers.end());
}

std::string CertificateOfMembership::toString() const
{
	std::string s;

	s.append("1:"); // COM_UINT64_ED25519

	uint64_t *buf = new uint64_t[_qualifiers.size() * 3];
	try {
		unsigned int ptr = 0;
		for(std::vector<_Qualifier>::const_iterator q(_qualifiers.begin());q!=_qualifiers.end();++q) {
			buf[ptr++] = Utils::hton(q->id);
			buf[ptr++] = Utils::hton(q->value);
			buf[ptr++] = Utils::hton(q->maxDelta);
		}
		s.append(Utils::hex(buf,ptr * sizeof(uint64_t)));
		delete [] buf;
	} catch ( ... ) {
		delete [] buf;
		throw;
	}

	s.push_back(':');

	s.append(_signedBy.toString());

	if (_signedBy) {
		s.push_back(':');
		s.append(Utils::hex(_signature.data,(unsigned int)_signature.size()));
	}

	return s;
}

void CertificateOfMembership::fromString(const char *s)
{
	_qualifiers.clear();
	_signedBy.zero();
	memset(_signature.data,0,_signature.size());

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
		unsigned int buflen = colonAt / 2;
		char *buf = new char[buflen];
		unsigned int bufactual = Utils::unhex(s,colonAt,buf,buflen);
		char *bufptr = buf;
		try {
			while (bufactual >= 24) {
				_qualifiers.push_back(_Qualifier());
				_qualifiers.back().id = Utils::ntoh(*((uint64_t *)bufptr)); bufptr += 8;
				_qualifiers.back().value = Utils::ntoh(*((uint64_t *)bufptr)); bufptr += 8;
				_qualifiers.back().maxDelta = Utils::ntoh(*((uint64_t *)bufptr)); bufptr += 8;
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
					if (Utils::unhex(s,colonAt,_signature.data,(unsigned int)_signature.size()) != _signature.size())
						_signedBy.zero();
				} else _signedBy.zero();
			} else _signedBy.zero();
		}
	}

	std::sort(_qualifiers.begin(),_qualifiers.end());
	std::unique(_qualifiers.begin(),_qualifiers.end());
}

bool CertificateOfMembership::agreesWith(const CertificateOfMembership &other) const
	throw()
{
	unsigned long myidx = 0;
	unsigned long otheridx = 0;

	while (myidx < _qualifiers.size()) {
		// Fail if we're at the end of other, since this means the field is
		// missing.
		if (otheridx >= other._qualifiers.size())
			return false;

		// Seek to corresponding tuple in other, ignoring tuples that
		// we may not have. If we run off the end of other, the tuple is
		// missing. This works because tuples are sorted by ID.
		while (other._qualifiers[otheridx].id != _qualifiers[myidx].id) {
			++otheridx;
			if (otheridx >= other._qualifiers.size())
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
	uint64_t *buf = new uint64_t[_qualifiers.size() * 3];
	unsigned int ptr = 0;
	for(std::vector<_Qualifier>::const_iterator q(_qualifiers.begin());q!=_qualifiers.end();++q) {
		buf[ptr++] = Utils::hton(q->id);
		buf[ptr++] = Utils::hton(q->value);
		buf[ptr++] = Utils::hton(q->maxDelta);
	}

	try {
		_signature = with.sign(buf,ptr * sizeof(uint64_t));
		_signedBy = with.address();
		delete [] buf;
		return true;
	} catch ( ... ) {
		_signedBy.zero();
		delete [] buf;
		return false;
	}
}

bool CertificateOfMembership::verify(const Identity &id) const
{
	if (!_signedBy)
		return false;
	if (id.address() != _signedBy)
		return false;

	uint64_t *buf = new uint64_t[_qualifiers.size() * 3];
	unsigned int ptr = 0;
	for(std::vector<_Qualifier>::const_iterator q(_qualifiers.begin());q!=_qualifiers.end();++q) {
		buf[ptr++] = Utils::hton(q->id);
		buf[ptr++] = Utils::hton(q->value);
		buf[ptr++] = Utils::hton(q->maxDelta);
	}

	bool valid = false;
	try {
		valid = id.verify(buf,ptr * sizeof(uint64_t),_signature);
		delete [] buf;
	} catch ( ... ) {
		delete [] buf;
	}
	return valid;
}

} // namespace ZeroTier
