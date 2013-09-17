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

namespace ZeroTier {

void Identity::generate()
{
	C25519::Pair kp;
	do {
		kp = C25519::generate();
		_address = deriveAddress(kp.pub.data,kp.pub.size());
	} while (_address.isReserved());

	_publicKey = kp.pub;
	if (!_privateKey)
		_privateKey = new C25519::Private();
	*_privateKey = kp.priv;

	unsigned char tmp[ZT_ADDRESS_LENGTH + ZT_C25519_PUBLIC_KEY_LEN];
	_address.copyTo(tmp,ZT_ADDRESS_LENGTH);
	memcpy(tmp + ZT_ADDRESS_LENGTH,_publicKey.data,ZT_C25519_PUBLIC_KEY_LEN);
	_signature = C25519::sign(kp,tmp,sizeof(tmp));
}

bool Identity::locallyValidate(bool doAddressDerivationCheck) const
{
	unsigned char tmp[ZT_ADDRESS_LENGTH + ZT_C25519_PUBLIC_KEY_LEN];
	_address.copyTo(tmp,ZT_ADDRESS_LENGTH);
	memcpy(tmp + ZT_ADDRESS_LENGTH,_publicKey.data,ZT_C25519_PUBLIC_KEY_LEN);
	if (!C25519::verify(_publicKey,tmp,sizeof(tmp),_signature))
		return false;
	if ((doAddressDerivationCheck)&&(deriveAddress(_publicKey.data,_publicKey.size()) != _address))
		return false;
	return true;
}

std::string Identity::toString(bool includePrivate) const
{
	std::string r;

	r.append(_address.toString());
	r.append(":2:"); // 2 == IDENTITY_TYPE_C25519
	r.append(Utils::hex(_publicKey.data,_publicKey.size()));
	r.push_back(':');
	r.append(Utils::hex(_signature.data,_signature.size()));
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
				if (strcmp(f,"2"))
					return false;
				break;
			case 2:
				if (Utils::unhex(f,_publicKey.data,_publicKey.size()) != _publicKey.size())
					return false;
				break;
			case 3:
				if (Utils::unhex(f,_signature.data,_signature.size()) != _signature.size())
					return false;
				break;
			case 4:
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

// These are fixed parameters and can't be changed without a new
// identity type.
#define ZT_IDENTITY_DERIVEADDRESS_MEMORY 33554432
#define ZT_IDENTITY_DERIVEADDRESS_ROUNDS 50

Address Identity::deriveAddress(const void *keyBytes,unsigned int keyLen)
{
	/*
	 * Sequential memory-hard algorithm wedding address to public key
	 *
	 * Conventional hashcash with long computations and quick verifications
	 * unfortunately cannot be used here. If that were used, it would be
	 * equivalently costly to simply increment/vary the public key and find
	 * a collision as it would be to find the address. We need something
	 * that creates a costly 1:~1 mapping from key to address, hence this
	 * algorithm.
	 *
	 * Search for "sequential memory hard algorithm" for academic references
	 * to similar concepts.
	 */

	unsigned char *ram = new unsigned char[ZT_IDENTITY_DERIVEADDRESS_MEMORY];
	for(unsigned int i=0;i<ZT_IDENTITY_DERIVEADDRESS_MEMORY;++i)
		ram[i] = ((const unsigned char *)keyBytes)[i % keyLen];

	unsigned char salsaKey[ZT_SHA512_DIGEST_LEN];
	SHA512::hash(salsaKey,keyBytes,keyLen);

	uint64_t nonce = 0;
	for(unsigned int r=0;r<ZT_IDENTITY_DERIVEADDRESS_ROUNDS;++r) {
		nonce = Utils::crc64(nonce,ram,ZT_IDENTITY_DERIVEADDRESS_MEMORY);
#if __BYTE_ORDER == __BIG_ENDIAN
		nonce = ( // swap to little endian -- this was written for a LE system
			((nonce & 0x00000000000000FFULL) << 56) | 
			((nonce & 0x000000000000FF00ULL) << 40) | 
			((nonce & 0x0000000000FF0000ULL) << 24) | 
			((nonce & 0x00000000FF000000ULL) <<  8) | 
			((nonce & 0x000000FF00000000ULL) >>  8) | 
			((nonce & 0x0000FF0000000000ULL) >> 24) | 
			((nonce & 0x00FF000000000000ULL) >> 40) | 
			((nonce & 0xFF00000000000000ULL) >> 56)
		);
#endif
		Salsa20 s20(salsaKey,256,&nonce);
#if __BYTE_ORDER == __BIG_ENDIAN
		nonce = ( // swap back to big endian
			((nonce & 0x00000000000000FFULL) << 56) | 
			((nonce & 0x000000000000FF00ULL) << 40) | 
			((nonce & 0x0000000000FF0000ULL) << 24) | 
			((nonce & 0x00000000FF000000ULL) <<  8) | 
			((nonce & 0x000000FF00000000ULL) >>  8) | 
			((nonce & 0x0000FF0000000000ULL) >> 24) | 
			((nonce & 0x00FF000000000000ULL) >> 40) | 
			((nonce & 0xFF00000000000000ULL) >> 56)
		);
#endif
		s20.encrypt(ram,ram,ZT_IDENTITY_DERIVEADDRESS_MEMORY);
	}

	unsigned char finalDigest[ZT_SHA512_DIGEST_LEN];
	SHA512::hash(finalDigest,ram,ZT_IDENTITY_DERIVEADDRESS_MEMORY);

	delete [] ram;

	return Address(finalDigest,ZT_ADDRESS_LENGTH);
}

} // namespace ZeroTier

