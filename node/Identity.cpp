/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2012-2013  ZeroTier Networks LLC
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "Constants.hpp"
#include "Identity.hpp"
#include "SHA512.hpp"
#include "Salsa20.hpp"
#include "Utils.hpp"

#define ZT_IDENTITY_SHA_BYTE1_MASK 0xf8

namespace ZeroTier {

/*
 * This is the hashcash criterion
 */
struct _Identity_generate_cond
{
	_Identity_generate_cond() throw() {}
	_Identity_generate_cond(char *sb) throw() : sha512buf(sb) {}

	inline bool operator()(const C25519::Pair &kp) const
		throw()
	{
		SHA512::hash(sha512buf,kp.pub.data,kp.pub.size());

		if ((!sha512buf[0])&&(!(sha512buf[1] & ZT_IDENTITY_SHA_BYTE1_MASK)))
			return true;

		return false;
	}

	char *sha512buf;
};

void Identity::generate()
{
	char sha512buf[64];

	C25519::Pair kp;
	do {
		kp = C25519::generateSatisfying(_Identity_generate_cond(sha512buf));
		_address.setTo(sha512buf + 59,ZT_ADDRESS_LENGTH); // last 5 bytes are address
	} while (_address.isReserved());

	_publicKey = kp.pub;
	if (!_privateKey)
		_privateKey = new C25519::Private();
	*_privateKey = kp.priv;
}

bool Identity::locallyValidate() const
{
	char sha512buf[64];
	char addrb[5];
	_address.copyTo(addrb,5);
	SHA512::hash(sha512buf,_publicKey.data,_publicKey.size());
	return (
		(!sha512buf[0])&&
		(!(sha512buf[1] & ZT_IDENTITY_SHA_BYTE1_MASK))&&
		(sha512buf[59] == addrb[0])&&
		(sha512buf[60] == addrb[1])&&
		(sha512buf[61] == addrb[2])&&
		(sha512buf[62] == addrb[3])&&
		(sha512buf[63] == addrb[4]));
}

std::string Identity::toString(bool includePrivate) const
{
	std::string r;

	r.append(_address.toString());
	r.append(":0:"); // 0 == IDENTITY_TYPE_C25519
	r.append(Utils::hex(_publicKey.data,_publicKey.size()));
	if ((_privateKey)&&(includePrivate)) {
		r.push_back(':');
		r.append(Utils::hex(_privateKey->data,_privateKey->size()));
	}

	return r;
}

bool Identity::fromString(const char *str)
{
	char *saveptr = (char *)0;
	char tmp[4096];
	if (!Utils::scopy(tmp,sizeof(tmp),str))
		return false;

	delete _privateKey;
	_privateKey = (C25519::Private *)0;

	int fno = 0;
	for(char *f=Utils::stok(tmp,":",&saveptr);(f);f=Utils::stok((char *)0,":",&saveptr)) {
		switch(fno++) {
			case 0:
				_address = Address(f);
				if (_address.isReserved())
					return false;
				break;
			case 1:
				if (f[0] != '0')
					return false;
				break;
			case 2:
				if (Utils::unhex(f,_publicKey.data,_publicKey.size()) != _publicKey.size())
					return false;
				break;
			case 3:
				_privateKey = new C25519::Private();
				if (Utils::unhex(f,_privateKey->data,_privateKey->size()) != _privateKey->size())
					return false;
				break;
			default:
				return false;
		}
	}
	if (fno < 4)
		return false;

	return true;
}

} // namespace ZeroTier

