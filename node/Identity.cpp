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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "Constants.hpp"
#include "Identity.hpp"
#include "SHA512.hpp"
#include "Salsa20.hpp"
#include "Utils.hpp"

namespace ZeroTier {

//////////////////////////////////////////////////////////////////////////////
// This is the memory-hard hash used for type 0 identities' addresses

// These can't be changed without a new identity type. They define the
// parameters of the hashcash hashing/searching algorithm.
#define ZT_IDENTITY_GEN_HASHCASH_FIRST_BYTE_LESS_THAN 17
#define ZT_IDENTITY_GEN_MEMORY 2097152

// A memory-hard composition of SHA-512 and Salsa20 for hashcash hashing
static inline void _computeMemoryHardHash(const void *publicKey,unsigned int publicKeyBytes,void *digest,void *genmem)
{
	// Digest publicKey[] to obtain initial digest
	SHA512(digest,publicKey,publicKeyBytes);

	// Initialize genmem[] using Salsa20 in a CBC-like configuration since
	// ordinary Salsa20 is randomly seek-able. This is good for a cipher
	// but is not what we want for sequential memory-hardness.
	memset(genmem,0,ZT_IDENTITY_GEN_MEMORY);
	Salsa20 s20(digest,(char *)digest + 32);
	s20.crypt20((char *)genmem,(char *)genmem,64);
	for(unsigned long i=64;i<ZT_IDENTITY_GEN_MEMORY;i+=64) {
		unsigned long k = i - 64;
		*((uint64_t *)((char *)genmem + i)) = *((uint64_t *)((char *)genmem + k));
		*((uint64_t *)((char *)genmem + i + 8)) = *((uint64_t *)((char *)genmem + k + 8));
		*((uint64_t *)((char *)genmem + i + 16)) = *((uint64_t *)((char *)genmem + k + 16));
		*((uint64_t *)((char *)genmem + i + 24)) = *((uint64_t *)((char *)genmem + k + 24));
		*((uint64_t *)((char *)genmem + i + 32)) = *((uint64_t *)((char *)genmem + k + 32));
		*((uint64_t *)((char *)genmem + i + 40)) = *((uint64_t *)((char *)genmem + k + 40));
		*((uint64_t *)((char *)genmem + i + 48)) = *((uint64_t *)((char *)genmem + k + 48));
		*((uint64_t *)((char *)genmem + i + 56)) = *((uint64_t *)((char *)genmem + k + 56));
		s20.crypt20((char *)genmem + i,(char *)genmem + i,64);
	}

	// Render final digest using genmem as a lookup table
	for(unsigned long i=0;i<(ZT_IDENTITY_GEN_MEMORY / sizeof(uint64_t));) {
		unsigned long idx1 = (unsigned long)(Utils::ntoh(((uint64_t *)genmem)[i++]) % (64 / sizeof(uint64_t)));
		unsigned long idx2 = (unsigned long)(Utils::ntoh(((uint64_t *)genmem)[i++]) % (ZT_IDENTITY_GEN_MEMORY / sizeof(uint64_t)));
		uint64_t tmp = ((uint64_t *)genmem)[idx2];
		((uint64_t *)genmem)[idx2] = ((uint64_t *)digest)[idx1];
		((uint64_t *)digest)[idx1] = tmp;
		s20.crypt20(digest,digest,64);
	}
}

// Hashcash generation halting condition -- halt when first byte is less than
// threshold value.
struct _Identity_generate_cond
{
	_Identity_generate_cond() {}
	_Identity_generate_cond(unsigned char *sb,char *gm) : digest(sb),genmem(gm) {}
	inline bool operator()(const C25519::Pair &kp) const
	{
		_computeMemoryHardHash(kp.pub.data,ZT_C25519_PUBLIC_KEY_LEN,digest,genmem);
		return (digest[0] < ZT_IDENTITY_GEN_HASHCASH_FIRST_BYTE_LESS_THAN);
	}
	unsigned char *digest;
	char *genmem;
};

//////////////////////////////////////////////////////////////////////////////

void Identity::generate(const Type t)
{
	uint8_t digest[64];
	switch(t) {
		case C25519: {
			char *genmem = new char[ZT_IDENTITY_GEN_MEMORY];

			C25519::Pair kp;
			do {
				kp = C25519::generateSatisfying(_Identity_generate_cond(digest,genmem));
				_address.setTo(digest + 59,ZT_ADDRESS_LENGTH); // last 5 bytes are address
			} while (_address.isReserved());

			memcpy(_k.t0.pub.data,kp.pub.data,ZT_C25519_PUBLIC_KEY_LEN);
			memcpy(_k.t0.priv.data,kp.priv.data,ZT_C25519_PRIVATE_KEY_LEN);
			_type = C25519;
			_hasPrivate = true;
		
			delete [] genmem;
		}	break;

		case P384: {
			do {
				ECC384GenerateKey(_k.t1.pub,_k.t1.priv);
				// TODO
				SHA512(digest,_k.t1.pub,ZT_ECC384_PUBLIC_KEY_SIZE);
				_address.setTo(digest + 59,ZT_ADDRESS_LENGTH);
			} while (_address.isReserved());
			_type = P384;
			_hasPrivate = true;
		}	break;
	}
}

bool Identity::locallyValidate() const
{
	if (_address.isReserved())
		return false;

	switch(_type) {
		case C25519: {
			unsigned char digest[64];
			char *genmem = new char[ZT_IDENTITY_GEN_MEMORY];
			_computeMemoryHardHash(_k.t0.pub.data,ZT_C25519_PUBLIC_KEY_LEN,digest,genmem);
			delete [] genmem;
			unsigned char addrb[5];
			_address.copyTo(addrb,5);
			return (
				(digest[0] < ZT_IDENTITY_GEN_HASHCASH_FIRST_BYTE_LESS_THAN)&&
				(digest[59] == addrb[0])&&
				(digest[60] == addrb[1])&&
				(digest[61] == addrb[2])&&
				(digest[62] == addrb[3])&&
				(digest[63] == addrb[4]));
		}	break;

		case P384: {
			return true;
		}	break;
	}

	return false;
}

char *Identity::toString(bool includePrivate,char buf[ZT_IDENTITY_STRING_BUFFER_LENGTH]) const
{
	switch(_type) {
		case C25519: {
			char *p = buf;
			Utils::hex10(_address.toInt(),p);
			p += 10;
			*(p++) = ':';
			*(p++) = '0';
			*(p++) = ':';
			Utils::hex(_k.t0.pub.data,ZT_C25519_PUBLIC_KEY_LEN,p);
			p += ZT_C25519_PUBLIC_KEY_LEN * 2;
			if ((_hasPrivate)&&(includePrivate)) {
				*(p++) = ':';
				Utils::hex(_k.t0.priv.data,ZT_C25519_PRIVATE_KEY_LEN,p);
				p += ZT_C25519_PRIVATE_KEY_LEN * 2;
			}
			*p = (char)0;
			return buf;
		}

		case P384: {
			char *p = buf;
			Utils::hex10(_address.toInt(),p);
			p += 10;
			*(p++) = ':';
			*(p++) = '1';
			*(p++) = ':';
			Utils::hex(_k.t1.pub,ZT_ECC384_PUBLIC_KEY_SIZE,p);
			p += ZT_ECC384_PUBLIC_KEY_SIZE * 2;
			if ((_hasPrivate)&&(includePrivate)) {
				*(p++) = ':';
				Utils::hex(_k.t1.priv,ZT_ECC384_PRIVATE_KEY_SIZE,p);
				p += ZT_ECC384_PRIVATE_KEY_SIZE * 2;
			}
			*p = (char)0;
			return buf;
		}	break;
	}
}

bool Identity::fromString(const char *str)
{
	if (!str) {
		_address.zero();
		return false;
	}
	char tmp[ZT_IDENTITY_STRING_BUFFER_LENGTH];
	if (!Utils::scopy(tmp,sizeof(tmp),str)) {
		_address.zero();
		return false;
	}

	_hasPrivate = false;

	int fno = 0;
	char *saveptr = (char *)0;
	for(char *f=Utils::stok(tmp,":",&saveptr);(f);f=Utils::stok((char *)0,":",&saveptr)) {
		switch(fno++) {
			case 0:
				_address = Address(Utils::hexStrToU64(f));
				if (_address.isReserved()) {
					_address.zero();
					return false;
				}
				break;
			case 1:
				if ((f[0] == '0')&&(!f[1])) {
					_type = C25519;
				} else if ((f[0] == '1')&&(!f[1])) {
					_type = P384;
				} else {
					_address.zero();
					return false;
				}
				break;
			case 2:
				switch(_type) {
					case C25519:
						if (Utils::unhex(f,_k.t0.pub.data,ZT_C25519_PUBLIC_KEY_LEN) != ZT_C25519_PUBLIC_KEY_LEN) {
							_address.zero();
							return false;
						}
						break;
					case P384:
						if (Utils::unhex(f,_k.t1.pub,ZT_ECC384_PUBLIC_KEY_SIZE) != ZT_ECC384_PUBLIC_KEY_SIZE) {
							_address.zero();
							return false;
						}
						break;
				}
				break;
			case 3:
				switch(_type) {
					case C25519:
						if (Utils::unhex(f,_k.t0.priv.data,ZT_C25519_PRIVATE_KEY_LEN) != ZT_C25519_PRIVATE_KEY_LEN) {
							_address.zero();
							return false;
						} else {
							_hasPrivate = true;
						}
						break;
					case P384:
						if (Utils::unhex(f,_k.t1.priv,ZT_ECC384_PRIVATE_KEY_SIZE) != ZT_ECC384_PRIVATE_KEY_SIZE) {
							_address.zero();
							return false;
						} else {
							_hasPrivate = true;
						}
						break;
				}
				break;
			default:
				_address.zero();
				return false;
		}
	}
	if (fno < 3) {
		_address.zero();
		return false;
	}

	return true;
}

} // namespace ZeroTier
