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

// These can't be changed without a new identity type. They define the
// parameters of the hashcash hashing/searching algorithm.

// Hashcash halting criteria
#define ZT_IDENTITY_GEN_HASHCASH_FIRST_BYTE_LESS_THAN 7

// Amount of memory for memory-hardness
#define ZT_IDENTITY_GEN_MEMORY 8388608

// Step distance for mixing genmem[]
#define ZT_IDENTITY_GEN_MEMORY_MIX_STEP 1024

namespace ZeroTier {

// A memory-hard composition of SHA-512 and Salsa20 for hashcash hashing
static inline void _computeMemoryHardHash(const void *publicKey,unsigned int publicKeyBytes,void *digest,void *genmem)
{
	// Hash publicKey[] to obtain Salsa20 key
	SHA512::hash(digest,publicKey,publicKeyBytes);

	// Generate genmem[] bytes of Salsa20 key stream
	memset(genmem,0,ZT_IDENTITY_GEN_MEMORY);
	Salsa20 s20(digest,256,(char *)digest + 32);
	s20.encrypt(genmem,genmem,ZT_IDENTITY_GEN_MEMORY);

	// Do something to genmem[] that iteratively makes every value
	// possibly dependent on every other value with a nontrivial
	// probability.
	for(unsigned int i=0;i<ZT_IDENTITY_GEN_MEMORY;i+=ZT_IDENTITY_GEN_MEMORY_MIX_STEP) {
		s20.encrypt((char *)genmem + i,(char *)genmem + i,8);
		uint64_t x = *((uint64_t *)((char *)genmem + i));
		if ((x / 7ULL) < 0x1249249249249249ULL) {
			s20.encrypt(&x,&x,8); // also causes PRNG state to depend on genmem[]'s state
			for(unsigned int k=0;k<8;++k,x>>=8)
				++((unsigned char *)genmem)[(uintptr_t)x % ZT_IDENTITY_GEN_MEMORY];
		} else {
			for(unsigned int k=0;k<8;++k,x>>=8)
				--((unsigned char *)genmem)[(uintptr_t)x % ZT_IDENTITY_GEN_MEMORY];
		}
	}

	// Mix in publicKey[] again, ensuring all entropy is used
	for(unsigned int i=0;i<publicKeyBytes;++i)
		((unsigned char *)genmem)[i] ^= ((const unsigned char *)publicKey)[i];

	// Compute final digest from final genmem[]
	SHA512::hash(digest,genmem,ZT_IDENTITY_GEN_MEMORY);
}

// Hashcash generation halting condition -- halt when first byte is less than
// threshold value.
struct _Identity_generate_cond
{
	_Identity_generate_cond() throw() {}
	_Identity_generate_cond(unsigned char *sb,char *gm) throw() : digest(sb),genmem(gm) {}
	inline bool operator()(const C25519::Pair &kp) const
		throw()
	{
		_computeMemoryHardHash(kp.pub.data,kp.pub.size(),digest,genmem);
		return (digest[0] < ZT_IDENTITY_GEN_HASHCASH_FIRST_BYTE_LESS_THAN);
	}
	unsigned char *digest;
	char *genmem;
};

void Identity::generate()
{
	unsigned char digest[64];
	char *genmem = new char[ZT_IDENTITY_GEN_MEMORY];

	C25519::Pair kp;
	do {
		kp = C25519::generateSatisfying(_Identity_generate_cond(digest,genmem));
		_address.setTo(digest + 59,ZT_ADDRESS_LENGTH); // last 5 bytes are address
	} while (_address.isReserved());

	_publicKey = kp.pub;
	if (!_privateKey)
		_privateKey = new C25519::Private();
	*_privateKey = kp.priv;

	delete [] genmem;
}

bool Identity::locallyValidate() const
{
	if (_address.isReserved())
		return false;

	unsigned char digest[64];
	char *genmem = new char[ZT_IDENTITY_GEN_MEMORY];
	_computeMemoryHardHash(_publicKey.data,_publicKey.size(),digest,genmem);
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
				if ((f[0] != '0')||(f[1]))
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
	if (fno < 3)
		return false;

	return true;
}

} // namespace ZeroTier

