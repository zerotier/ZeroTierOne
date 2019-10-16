/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2023-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

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

namespace {

// These can't be changed without a new identity type. They define the
// parameters of the hashcash hashing/searching algorithm for type 0
// identities.
#define ZT_IDENTITY_GEN_HASHCASH_FIRST_BYTE_LESS_THAN 17
#define ZT_IDENTITY_GEN_MEMORY 2097152

// A memory-hard composition of SHA-512 and Salsa20 for hashcash hashing
static void _computeMemoryHardHash(const void *publicKey,unsigned int publicKeyBytes,void *digest,void *genmem)
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
	inline _Identity_generate_cond() {}
	inline _Identity_generate_cond(unsigned char *sb,char *gm) : digest(sb),genmem(gm) {}
	inline bool operator()(const uint8_t pub[ZT_C25519_PUBLIC_KEY_LEN]) const
	{
		_computeMemoryHardHash(pub,ZT_C25519_PUBLIC_KEY_LEN,digest,genmem);
		return (digest[0] < ZT_IDENTITY_GEN_HASHCASH_FIRST_BYTE_LESS_THAN);
	}
	unsigned char *digest;
	char *genmem;
};

} // anonymous namespace

void Identity::generate(const Type t)
{
	uint8_t digest[64];

	_type = t;
	_hasPrivate = true;

	char *const genmem = new char[ZT_IDENTITY_GEN_MEMORY];
	do {
		C25519::generateSatisfying(_Identity_generate_cond(digest,genmem),_pub.c25519,_priv.c25519);
		_address.setTo(digest + 59,ZT_ADDRESS_LENGTH); // last 5 bytes are address
	} while (_address.isReserved());
	delete [] genmem;

	if (t == P384) {
		ECC384GenerateKey(_pub.p384,_priv.p384);
		C25519::sign(_priv.c25519,_pub.c25519,&_pub,ZT_C25519_PUBLIC_KEY_LEN + ZT_ECC384_PUBLIC_KEY_SIZE,_pub.p384s);
	}
}

bool Identity::locallyValidate() const
{
	uint8_t digest[64];

	if (_address.isReserved())
		return false;

	switch(_type) {
		case C25519:
			break;
		case P384:
			if (!C25519::verify(_pub.c25519,&_pub,ZT_C25519_PUBLIC_KEY_LEN + ZT_ECC384_PUBLIC_KEY_SIZE,_pub.p384s,ZT_C25519_SIGNATURE_LEN))
				return false;
		default:
			return false;
	}

	char *genmem = nullptr;
	try {
		genmem = new char[ZT_IDENTITY_GEN_MEMORY];
		_computeMemoryHardHash(_pub.c25519,ZT_C25519_PUBLIC_KEY_LEN,digest,genmem);
		delete [] genmem;
		return ((_address == Address(digest + 59,ZT_ADDRESS_LENGTH))&&(!_address.isReserved())&&(digest[0] < ZT_IDENTITY_GEN_HASHCASH_FIRST_BYTE_LESS_THAN));
	} catch ( ... ) {
		if (genmem) delete [] genmem;
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
			Utils::hex(_pub.c25519,ZT_C25519_PUBLIC_KEY_LEN,p);
			p += ZT_C25519_PUBLIC_KEY_LEN * 2;
			if ((_hasPrivate)&&(includePrivate)) {
				*(p++) = ':';
				Utils::hex(_priv.c25519,ZT_C25519_PRIVATE_KEY_LEN,p);
				p += ZT_C25519_PRIVATE_KEY_LEN * 2;
			}
			*p = (char)0;
			return buf;
		} break;

		case P384: {
			char *p = buf;
			Utils::hex10(_address.toInt(),p);
			p += 10;
			*(p++) = ':';
			*(p++) = '1';
			*(p++) = ':';
			int el = Utils::b32e((const uint8_t *)(&_pub),sizeof(_pub),p,(unsigned int)(ZT_IDENTITY_STRING_BUFFER_LENGTH - (uintptr_t)(p - buf)));
			if (el <= 0) return nullptr;
			p += el;
			if ((_hasPrivate)&&(includePrivate)) {
				*(p++) = ':';
				el = Utils::b32e((const uint8_t *)(&_priv),sizeof(_priv),p,(unsigned int)(ZT_IDENTITY_STRING_BUFFER_LENGTH - (uintptr_t)(p - buf)));
				if (el <= 0) return nullptr;
				p += el;
			}
			*p = (char)0;
			return buf;
		}	break;

	}
	return nullptr;
}

bool Identity::fromString(const char *str)
{
	_hasPrivate = false;

	if (!str) {
		_address.zero();
		return false;
	}

	char tmp[ZT_IDENTITY_STRING_BUFFER_LENGTH];
	if (!Utils::scopy(tmp,sizeof(tmp),str)) {
		_address.zero();
		return false;
	}

	int fno = 0;
	char *saveptr = (char *)0;
	for(char *f=Utils::stok(tmp,":",&saveptr);((f)&&(fno < 4));f=Utils::stok((char *)0,":",&saveptr)) {
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
						if (Utils::unhex(f,_pub.c25519,ZT_C25519_PUBLIC_KEY_LEN) != ZT_C25519_PUBLIC_KEY_LEN) {
							_address.zero();
							return false;
						}
						break;

					case P384:
						if (Utils::b32d(f,(uint8_t *)(&_pub),sizeof(_pub)) != sizeof(_pub)) {
							_address.zero();
							return false;
						}
						break;

				}
				break;

			case 3:
				if (strlen(f) > 1) {
					switch(_type) {

						case C25519:
							if (Utils::unhex(f,_priv.c25519,ZT_C25519_PRIVATE_KEY_LEN) != ZT_C25519_PRIVATE_KEY_LEN) {
								_address.zero();
								return false;
							} else {
								_hasPrivate = true;
							}
							break;

						case P384:
							if (Utils::b32d(f,(uint8_t *)(&_priv),sizeof(_priv)) != sizeof(_priv)) {
								_address.zero();
								return false;
							} else {
								_hasPrivate = true;
							}
							break;

					}
					break;
				}

		}
	}

	if (fno < 3) {
		_address.zero();
		return false;
	}

	return true;
}

} // namespace ZeroTier
