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

#include "Identity.hpp"

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

	return r;
}

bool Identity::fromString(const char *str)
{
}

#define ZT_IDENTITY_DERIVEADDRESS_DIGESTS 540672
#define ZT_IDENTITY_DERIVEADDRESS_ROUNDS 4

Address Identity::deriveAddress(const void *keyBytes,unsigned int keyLen)
{
	/*
	 * Sequential memory-hard algorithm wedding address to public key
	 *
	 * Conventional hashcash with long computations and quick verifications
	 * unfortunately cannot be used here. If that were used, it would be
	 * equivalently costly to simply increment/vary the public key and find
	 * a collision as it would be to find the address. We need something
	 * that creates a costly 1:~1 mapping from key to address, hence this odd
	 * algorithm.
	 *
	 * Search for "sequential memory hard algorithm" for academic references
	 * to similar concepts.
	 */

	unsigned char finalDigest[ZT_SHA512_DIGEST_LEN];
	unsigned char *digests = new unsigned char[ZT_SHA512_DIGEST_LEN * ZT_IDENTITY_DERIVEADDRESS_DIGESTS];

	SHA512::hash(finalDigest,keyBytes,keyLen);
	for(unsigned int i=0;i<(unsigned int)sizeof(digests);++i)
		digests[i] = ((const unsigned char *)keyBytes)[i % keyLen];

	for(unsigned int r=0;r<ZT_IDENTITY_DERIVEADDRESS_ROUNDS;++r) {
		for(unsigned int i=0;i<(ZT_SHA512_DIGEST_LEN * ZT_IDENTITY_DERIVEADDRESS_DIGESTS);++i)
			digests[i] ^= finalDigest[i % ZT_SHA512_DIGEST_LEN];
		for(unsigned int d=0;d<ZT_IDENTITY_DERIVEADDRESS_DIGESTS;++d)
			SHA512::hash(digests + (ZT_SHA512_DIGEST_LEN * d),digests,ZT_SHA512_DIGEST_LEN * ZT_IDENTITY_DERIVEADDRESS_DIGESTS);
		SHA512::hash(finalDigest,digests,ZT_SHA512_DIGEST_LEN * ZT_IDENTITY_DERIVEADDRESS_DIGESTS);
	}

	delete [] digests;

	return Address(finalDigest,ZT_ADDRESS_LENGTH); // first 5 bytes of dig[]
}

} // namespace ZeroTier

