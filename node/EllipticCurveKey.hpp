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

#ifndef _ZT_ELLIPTICCURVEKEY_H
#define _ZT_ELLIPTICCURVEKEY_H

#include <string>
#include <algorithm>
#include <string.h>
#include "Utils.hpp"

/**
 * Key type ID for identifying our use of NIST-P-521
 *
 * If in the future other types of keys are supported (post-quantum crypto?)
 * then we'll need a key type 2, etc. When keys are stored in the database
 * they are prefixed by this key type ID byte.
 */
#define ZT_KEY_TYPE 1

#define ZT_EC_OPENSSL_CURVE NID_secp521r1
#define ZT_EC_CURVE_NAME "NIST-P-521"
#define ZT_EC_PRIME_BYTES 66
#define ZT_EC_PUBLIC_KEY_BYTES (ZT_EC_PRIME_BYTES + 1)
#define ZT_EC_PRIVATE_KEY_BYTES ZT_EC_PRIME_BYTES
#define ZT_EC_MAX_BYTES ZT_EC_PUBLIC_KEY_BYTES

namespace ZeroTier {

class EllipticCurveKeyPair;

/**
 * An elliptic curve public or private key
 */
class EllipticCurveKey
{
	friend class EllipticCurveKeyPair;

public:
	EllipticCurveKey()
		throw() :
		_bytes(0)
	{
		memset(_key,0,sizeof(_key));
	}

	EllipticCurveKey(const void *data,unsigned int len)
		throw()
	{
		set(data,len);
	}

	EllipticCurveKey(const std::string &data)
		throw()
	{
		set(data.data(),data.length());
	}

	EllipticCurveKey(const EllipticCurveKey &k)
		throw()
	{
		_bytes = k._bytes;
		memcpy(_key,k._key,_bytes);
	}

	inline EllipticCurveKey &operator=(const EllipticCurveKey &k)
		throw()
	{
		_bytes = k._bytes;
		memcpy(_key,k._key,_bytes);
		return *this;
	}

	inline void set(const void *data,unsigned int len)
		throw()
	{
		if (len <= ZT_EC_MAX_BYTES) {
			_bytes = len;
			memcpy(_key,data,len);
		} else _bytes = 0;
	}

	inline const unsigned char *data() const throw() { return _key; }
	inline unsigned int size() const throw() { return _bytes; }
	inline std::string toHex() const throw() { return Utils::hex(_key,_bytes); }

	inline unsigned char operator[](const unsigned int i) const throw() { return _key[i]; }

	inline bool operator==(const EllipticCurveKey &k) const throw() { return ((_bytes == k._bytes)&&(!memcmp(_key,k._key,_bytes))); }
	inline bool operator<(const EllipticCurveKey &k) const throw() { return std::lexicographical_compare(_key,&_key[_bytes],k._key,&k._key[k._bytes]); }
	inline bool operator!=(const EllipticCurveKey &k) const throw() { return !(*this == k); }
	inline bool operator>(const EllipticCurveKey &k) const throw() { return (k < *this); }
	inline bool operator<=(const EllipticCurveKey &k) const throw() { return !(k < *this); }
	inline bool operator>=(const EllipticCurveKey &k) const throw() { return !(*this < k); }

private:
	unsigned int _bytes;
	unsigned char _key[ZT_EC_MAX_BYTES];
};

} // namespace ZeroTier

#endif

