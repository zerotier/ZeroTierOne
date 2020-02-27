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

#include "Constants.hpp"
#include "Identity.hpp"
#include "SHA512.hpp"
#include "Salsa20.hpp"
#include "AES.hpp"
#include "Utils.hpp"
#include "MIMC52.hpp"

#include <cstring>
#include <cstdint>
#include <algorithm>

#define ZT_V1_IDENTITY_MIMC52_VDF_ROUNDS_BASE 250000

namespace ZeroTier {

namespace {

// This is the memory-intensive hash function used to compute v0 identities from v0 public keys.
#define ZT_V0_IDENTITY_GEN_MEMORY 2097152
static void _computeMemoryHardHash(const void *const publicKey,unsigned int publicKeyBytes,void *const digest,void *const genmem) noexcept
{
	// Digest publicKey[] to obtain initial digest
	SHA512(digest,publicKey,publicKeyBytes);

	// Initialize genmem[] using Salsa20 in a CBC-like configuration since
	// ordinary Salsa20 is randomly seek-able. This is good for a cipher
	// but is not what we want for sequential memory-hardness.
	memset(genmem,0,ZT_V0_IDENTITY_GEN_MEMORY);
	Salsa20 s20(digest,(char *)digest + 32);
	s20.crypt20((char *)genmem,(char *)genmem,64);
	for(unsigned long i=64;i<ZT_V0_IDENTITY_GEN_MEMORY;i+=64) {
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
	for(unsigned long i=0;i<(ZT_V0_IDENTITY_GEN_MEMORY / sizeof(uint64_t));) {
		unsigned long idx1 = (unsigned long)(Utils::ntoh(((uint64_t *)genmem)[i++]) % (64 / sizeof(uint64_t)));
		unsigned long idx2 = (unsigned long)(Utils::ntoh(((uint64_t *)genmem)[i++]) % (ZT_V0_IDENTITY_GEN_MEMORY / sizeof(uint64_t)));
		uint64_t tmp = ((uint64_t *)genmem)[idx2];
		((uint64_t *)genmem)[idx2] = ((uint64_t *)digest)[idx1];
		((uint64_t *)digest)[idx1] = tmp;
		s20.crypt20(digest,digest,64);
	}
}
struct _v0_identity_generate_cond
{
	ZT_ALWAYS_INLINE _v0_identity_generate_cond() noexcept {}
	ZT_ALWAYS_INLINE _v0_identity_generate_cond(unsigned char *sb,char *gm) noexcept : digest(sb),genmem(gm) {}
	ZT_ALWAYS_INLINE bool operator()(const uint8_t pub[ZT_C25519_PUBLIC_KEY_LEN]) const noexcept
	{
		_computeMemoryHardHash(pub,ZT_C25519_PUBLIC_KEY_LEN,digest,genmem);
		return (digest[0] < 17);
	}
	unsigned char *digest;
	char *genmem;
};

} // anonymous namespace

const Identity Identity::NIL;

bool Identity::generate(const Type t)
{
	_type = t;
	_hasPrivate = true;

	switch(t) {
		case C25519: {
			// Generate C25519/Ed25519 key pair whose hash satisfies a "hashcash" criterion and generate the
			// address from the last 40 bits of this hash. This is different from the fingerprint hash for V0.
			uint8_t digest[64];
			char *const genmem = new char[ZT_V0_IDENTITY_GEN_MEMORY];
			do {
				C25519::generateSatisfying(_v0_identity_generate_cond(digest,genmem),_pub.c25519,_priv.c25519);
				_address.setTo(digest + 59);
			} while (_address.isReserved());
			delete[] genmem;
			_computeHash();
		} break;

		case P384: {
			for(;;) {
				// Generate C25519, Ed25519, and NIST P-384 key pairs.
				C25519::generate(_pub.c25519,_priv.c25519);
				ECC384GenerateKey(_pub.p384,_priv.p384);

				// Execute the MIMC52 verifiable delay function, resulting a near constant time delay relative
				// to the speed of the current CPU. This result is incorporated into the final hash.
				Utils::storeBigEndian(_pub.t1mimc52,mimc52Delay(&_pub,sizeof(_pub) - sizeof(_pub.t1mimc52),ZT_V1_IDENTITY_MIMC52_VDF_ROUNDS_BASE));

				// Compute SHA384 fingerprint hash of keys and MIMC output and generate address directly from it.
				_computeHash();
				_address.setTo(_fp.data());
				if (!_address.isReserved())
					break;
			}
		} break;

		default:
			return false;
	}

	return true;
}

bool Identity::locallyValidate() const noexcept
{
	try {
		if ((!_address.isReserved()) && (_address)) {
			switch (_type) {

				case C25519: {
					uint8_t digest[64];
					char *genmem = new char[ZT_V0_IDENTITY_GEN_MEMORY];
					_computeMemoryHardHash(_pub.c25519,ZT_C25519_PUBLIC_KEY_LEN,digest,genmem);
					delete[] genmem;
					return ((_address == Address(digest + 59)) && (digest[0] < 17));
				}

				case P384:
					if (_address == Address(_fp.data())) {
						// The most significant 8 bits of the MIMC proof included with v1 identities can be used to store a multiplier
						// that can indicate that more work than the required minimum has been performed. Right now this is never done
						// but it could have some use in the future. There is no harm in doing it, and we'll accept any round count
						// that is at least ZT_V1_IDENTITY_MIMC52_VDF_ROUNDS_BASE.
						unsigned long rounds = (((unsigned long)_pub.t1mimc52[0] & 15U) + 1U); // max: 16 * ZT_V1_IDENTITY_MIMC52_VDF_ROUNDS_BASE
						rounds *= ZT_V1_IDENTITY_MIMC52_VDF_ROUNDS_BASE;
						return mimc52Verify(&_pub,sizeof(_pub) - sizeof(_pub.t1mimc52),rounds,Utils::loadBigEndian<uint64_t>(_pub.t1mimc52));
					} else {
						return false;
					}

			}
		}
	} catch ( ... ) {}
	return false;
}

void Identity::hashWithPrivate(uint8_t h[ZT_IDENTITY_HASH_SIZE]) const
{
	if (_hasPrivate) {
		switch (_type) {

			case C25519:
				SHA384(h,_pub.c25519,ZT_C25519_PUBLIC_KEY_LEN,_priv.c25519,ZT_C25519_PRIVATE_KEY_LEN);
				break;

			case P384:
				SHA384(h,&_pub,sizeof(_pub),&_priv,sizeof(_priv));
				break;

		}
		return;
	}
	memset(h,0,48);
}

unsigned int Identity::sign(const void *data,unsigned int len,void *sig,unsigned int siglen) const
{
	if (_hasPrivate) {
		switch(_type) {

			case C25519:
				if (siglen >= ZT_C25519_SIGNATURE_LEN) {
					C25519::sign(_priv.c25519,_pub.c25519,data,len,sig);
					return ZT_C25519_SIGNATURE_LEN;
				}

			case P384:
				if (siglen >= ZT_ECC384_SIGNATURE_SIZE) {
					// For P384 we sign SHA384(data | public keys) for added defense against any attack
					// that attempted to decouple the two keys in some way. Otherwise this has no impact
					// on the security of the signature (unless SHA384 had some serious flaw).
					uint8_t h[48];
					SHA384(h,data,len,&_pub,ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE);
					ECC384ECDSASign(_priv.p384,h,(uint8_t *)sig);
					return ZT_ECC384_SIGNATURE_SIZE;
				}

		}
	}
	return 0;
}

bool Identity::verify(const void *data,unsigned int len,const void *sig,unsigned int siglen) const
{
	switch(_type) {

		case C25519:
			return C25519::verify(_pub.c25519,data,len,sig,siglen);

		case P384:
			if (siglen == ZT_ECC384_SIGNATURE_SIZE) {
				uint8_t h[48];
				SHA384(h,data,len,&_pub,ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE);
				return ECC384ECDSAVerify(_pub.p384,h,(const uint8_t *)sig);
			}
			break;

	}
	return false;
}

bool Identity::agree(const Identity &id,uint8_t key[ZT_PEER_SECRET_KEY_LENGTH]) const
{
	uint8_t rawkey[128];
	uint8_t h[64];
	if (_hasPrivate) {
		if (_type == C25519) {

			if ((id._type == C25519)||(id._type == P384)) {
				// If we are a C25519 key we can agree with another C25519 key or with only the
				// C25519 portion of a type 1 P-384 key.
				C25519::agree(_priv.c25519,id._pub.c25519,rawkey);
				SHA512(h,rawkey,ZT_C25519_SHARED_KEY_LEN);
				memcpy(key,h,ZT_PEER_SECRET_KEY_LENGTH);
				return true;
			}

		} else if (_type == P384) {

			if (id._type == P384) {
				// For another P384 identity we execute DH agreement with BOTH keys and then
				// hash the results together. For those (cough FIPS cough) who only consider
				// P384 to be kosher, the C25519 secret can be considered a "salt"
				// or something. For those who don't trust P384 this means the privacy of
				// your traffic is also protected by C25519.
				C25519::agree(_priv.c25519,id._pub.c25519,rawkey);
				ECC384ECDH(id._pub.p384,_priv.p384,rawkey + ZT_C25519_SHARED_KEY_LEN);
				SHA384(h,rawkey,ZT_C25519_SHARED_KEY_LEN + ZT_ECC384_SHARED_SECRET_SIZE);
				memcpy(key,h,ZT_PEER_SECRET_KEY_LENGTH);
				return true;
			} else if (id._type == C25519) {
				// If the other identity is a C25519 identity we can agree using only that type.
				C25519::agree(_priv.c25519,id._pub.c25519,rawkey);
				SHA512(h,rawkey,ZT_C25519_SHARED_KEY_LEN);
				memcpy(key,h,ZT_PEER_SECRET_KEY_LENGTH);
				return true;
			}

		}
	}
	return false;
}

char *Identity::toString(bool includePrivate,char buf[ZT_IDENTITY_STRING_BUFFER_LENGTH]) const
{
	char *p = buf;
	_address.toString(p);
	p += 10;
	*(p++) = ':';

	switch(_type) {

		case C25519: {
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
		}

		case P384: {
			*(p++) = '1';
			*(p++) = ':';
			int el = Utils::b32e((const uint8_t *)(&_pub),sizeof(_pub),p,(int)(ZT_IDENTITY_STRING_BUFFER_LENGTH - (uintptr_t)(p - buf)));
			if (el <= 0) return nullptr;
			p += el;
			if ((_hasPrivate)&&(includePrivate)) {
				*(p++) = ':';
				el = Utils::b32e((const uint8_t *)(&_priv),sizeof(_priv),p,(int)(ZT_IDENTITY_STRING_BUFFER_LENGTH - (uintptr_t)(p - buf)));
				if (el <= 0) return nullptr;
				p += el;
			}
			*p = (char)0;
			return buf;
		}

	}

	return nullptr;
}

bool Identity::fromString(const char *str)
{
	_fp.zero();
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
						if (Utils::unhex(f,strlen(f),_pub.c25519,ZT_C25519_PUBLIC_KEY_LEN) != ZT_C25519_PUBLIC_KEY_LEN) {
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
							if (Utils::unhex(f,strlen(f),_priv.c25519,ZT_C25519_PRIVATE_KEY_LEN) != ZT_C25519_PRIVATE_KEY_LEN) {
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

	_computeHash();

	return true;
}

int Identity::marshal(uint8_t data[ZT_IDENTITY_MARSHAL_SIZE_MAX],const bool includePrivate) const noexcept
{
	_address.copyTo(data);
	switch(_type) {
		case C25519:
			data[ZT_ADDRESS_LENGTH] = (uint8_t)C25519;

			memcpy(data + ZT_ADDRESS_LENGTH + 1,_pub.c25519,ZT_C25519_PUBLIC_KEY_LEN);

			if ((includePrivate)&&(_hasPrivate)) {
				data[ZT_ADDRESS_LENGTH + 1 + ZT_C25519_PUBLIC_KEY_LEN] = ZT_C25519_PRIVATE_KEY_LEN;
				memcpy(data + ZT_ADDRESS_LENGTH + 1 + ZT_C25519_PUBLIC_KEY_LEN + 1,_priv.c25519,ZT_C25519_PRIVATE_KEY_LEN);
				return ZT_ADDRESS_LENGTH + 1 + ZT_C25519_PUBLIC_KEY_LEN + 1 + ZT_C25519_PRIVATE_KEY_LEN;
			} else {
				data[ZT_ADDRESS_LENGTH + 1 + ZT_C25519_PUBLIC_KEY_LEN] = 0;
				return ZT_ADDRESS_LENGTH + 1 + ZT_C25519_PUBLIC_KEY_LEN + 1;
			}

		case P384:
			data[ZT_ADDRESS_LENGTH] = (uint8_t)P384;

			memcpy(data + ZT_ADDRESS_LENGTH + 1,&_pub,ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE);

			if ((includePrivate)&&(_hasPrivate)) {
				data[ZT_ADDRESS_LENGTH + 1 + ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE] = ZT_IDENTITY_P384_COMPOUND_PRIVATE_KEY_SIZE;
				memcpy(data + ZT_ADDRESS_LENGTH + 1 + ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE + 1,&_priv,ZT_IDENTITY_P384_COMPOUND_PRIVATE_KEY_SIZE);
				return ZT_ADDRESS_LENGTH + 1 + ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE + 1 + ZT_IDENTITY_P384_COMPOUND_PRIVATE_KEY_SIZE;
			} else {
				data[ZT_ADDRESS_LENGTH + 1 + ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE] = 0;
				return ZT_ADDRESS_LENGTH + 1 + ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE + 1;
			}

	}
	return -1;
}

int Identity::unmarshal(const uint8_t *data,const int len) noexcept
{
	_fp.zero();
	_hasPrivate = false;

	if (len < (ZT_ADDRESS_LENGTH + 1))
		return -1;

	unsigned int privlen;
	switch((_type = (Type)data[ZT_ADDRESS_LENGTH])) {

		case C25519:
			if (len < (ZT_ADDRESS_LENGTH + 1 + ZT_C25519_PUBLIC_KEY_LEN + 1))
				return -1;

			memcpy(_pub.c25519,data + ZT_ADDRESS_LENGTH + 1,ZT_C25519_PUBLIC_KEY_LEN);

			privlen = data[ZT_ADDRESS_LENGTH + 1 + ZT_C25519_PUBLIC_KEY_LEN];
			if (privlen == ZT_C25519_PRIVATE_KEY_LEN) {
				if (len < (ZT_ADDRESS_LENGTH + 1 + ZT_C25519_PUBLIC_KEY_LEN + 1 + ZT_C25519_PRIVATE_KEY_LEN))
					return -1;

				_hasPrivate = true;
				memcpy(_priv.c25519,data + ZT_ADDRESS_LENGTH + 1 + ZT_C25519_PUBLIC_KEY_LEN + 1,ZT_C25519_PRIVATE_KEY_LEN);

				_computeHash();
				return ZT_ADDRESS_LENGTH + 1 + ZT_C25519_PUBLIC_KEY_LEN + 1 + ZT_C25519_PRIVATE_KEY_LEN;
			} else if (privlen == 0) {
				_hasPrivate = false;

				_computeHash();
				return ZT_ADDRESS_LENGTH + 1 + ZT_C25519_PUBLIC_KEY_LEN + 1;
			}
			break;

		case P384:
			if (len < (ZT_ADDRESS_LENGTH + 1 + ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE + 1))
				return -1;

			memcpy(&_pub,data + ZT_ADDRESS_LENGTH + 1,ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE);

			privlen = data[ZT_ADDRESS_LENGTH + 1 + ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE];
			if (privlen == ZT_IDENTITY_P384_COMPOUND_PRIVATE_KEY_SIZE) {
				if (len < (ZT_ADDRESS_LENGTH + 1 + ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE + 1 + ZT_IDENTITY_P384_COMPOUND_PRIVATE_KEY_SIZE))
					return -1;

				_hasPrivate = true;
				memcpy(&_priv,data + ZT_ADDRESS_LENGTH + 1 + ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE + 1,ZT_IDENTITY_P384_COMPOUND_PRIVATE_KEY_SIZE);

				_computeHash();
				return ZT_ADDRESS_LENGTH + 1 + ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE + 1 + ZT_IDENTITY_P384_COMPOUND_PRIVATE_KEY_SIZE;
			} else if (privlen == 0) {
				_hasPrivate = false;

				_computeHash();
				return ZT_ADDRESS_LENGTH + 1 + ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE + 1;
			}
			break;

	}

	return -1;
}

void Identity::_computeHash()
{
	switch(_type) {
		default:
			_fp.zero();
			break;

		case C25519:
			SHA384(_fp.data(),_pub.c25519,ZT_C25519_PUBLIC_KEY_LEN);
			break;

		case P384:
			SHA384(_fp.data(),&_pub,sizeof(_pub));
			break;
	}
}

} // namespace ZeroTier

extern "C" {

ZT_Identity *ZT_Identity_new(enum ZT_Identity_Type type)
{
	if ((type != ZT_IDENTITY_TYPE_C25519)&&(type != ZT_IDENTITY_TYPE_P384))
		return nullptr;
	try {
		ZeroTier::Identity *const id = new ZeroTier::Identity();
		id->generate((ZeroTier::Identity::Type)type);
		return reinterpret_cast<ZT_Identity *>(id);
	} catch ( ... ) {
		return nullptr;
	}
}

ZT_Identity *ZT_Identity_fromString(const char *idStr)
{
	if (!idStr)
		return nullptr;
	try {
		ZeroTier::Identity *const id = new ZeroTier::Identity();
		if (!id->fromString(idStr)) {
			delete id;
			return nullptr;
		}
		return reinterpret_cast<ZT_Identity *>(id);
	} catch ( ... ) {
		return nullptr;
	}
}

int ZT_Identity_validate(const ZT_Identity *id)
{
	if (!id)
		return 0;
	return reinterpret_cast<const ZeroTier::Identity *>(id)->locallyValidate() ? 1 : 0;
}

unsigned int ZT_Identity_sign(const ZT_Identity *id,const void *data,unsigned int len,void *signature,unsigned int signatureBufferLength)
{
	if (!id)
		return 0;
	if (signatureBufferLength < ZT_SIGNATURE_BUFFER_SIZE)
		return 0;
	return reinterpret_cast<const ZeroTier::Identity *>(id)->sign(data,len,signature,signatureBufferLength);
}

int ZT_Identity_verify(const ZT_Identity *id,const void *data,unsigned int len,const void *signature,unsigned int sigLen)
{
	if ((!id)||(!signature)||(!sigLen))
		return 0;
	return reinterpret_cast<const ZeroTier::Identity *>(id)->verify(data,len,signature,sigLen) ? 1 : 0;
}

enum ZT_Identity_Type ZT_Identity_type(const ZT_Identity *id)
{
	if (!id)
		return (ZT_Identity_Type)0;
	return (enum ZT_Identity_Type)reinterpret_cast<const ZeroTier::Identity *>(id)->type();
}

char *ZT_Identity_toString(const ZT_Identity *id,char *buf,int capacity,int includePrivate)
{
	if ((!id)||(!buf)||(capacity < ZT_IDENTITY_STRING_BUFFER_LENGTH))
		return nullptr;
	reinterpret_cast<const ZeroTier::Identity *>(id)->toString(includePrivate != 0,buf);
	return buf;
}

int ZT_Identity_hasPrivate(const ZT_Identity *id)
{
	if (!id)
		return 0;
	return reinterpret_cast<const ZeroTier::Identity *>(id)->hasPrivate() ? 1 : 0;
}

uint64_t ZT_Identity_address(const ZT_Identity *id)
{
	if (!id)
		return 0;
	return reinterpret_cast<const ZeroTier::Identity *>(id)->address().toInt();
}

void ZT_Identity_hash(const ZT_Identity *id,uint8_t h[48],int includePrivate)
{
	if (includePrivate)
		reinterpret_cast<const ZeroTier::Identity *>(id)->hashWithPrivate(h);
	else memcpy(h,reinterpret_cast<const ZeroTier::Identity *>(id)->fingerprint().data(),ZT_IDENTITY_HASH_SIZE);
}

ZT_SDK_API void ZT_Identity_delete(ZT_Identity *id)
{
	if (id)
		delete reinterpret_cast<ZeroTier::Identity *>(id);
}

}
