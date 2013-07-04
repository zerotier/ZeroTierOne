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

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <openssl/sha.h>

#include "Identity.hpp"
#include "Salsa20.hpp"
#include "HMAC.hpp"
#include "Utils.hpp"

namespace ZeroTier {

void Identity::generate()
{
	delete [] _keyPair;

	// Generate key pair and derive address
	do {
		_keyPair = new EllipticCurveKeyPair();
		_keyPair->generate();
		_address = deriveAddress(_keyPair->pub().data(),_keyPair->pub().size());
	} while (_address.isReserved());
	_publicKey = _keyPair->pub();

	// Sign address, key type, and public key with private key (with a zero
	// byte between each field). Including this extra data means simply editing
	// the address of an identity will be detected as its signature will be
	// invalid. Of course, deep verification of address/key relationship is
	// required to cover the more elaborate address claim jump attempt case.
	SHA256_CTX sha;
	unsigned char dig[32];
	unsigned char idtype = IDENTITY_TYPE_NIST_P_521,zero = 0;
	SHA256_Init(&sha);
	SHA256_Update(&sha,_address.data(),ZT_ADDRESS_LENGTH);
	SHA256_Update(&sha,&zero,1);
	SHA256_Update(&sha,&idtype,1);
	SHA256_Update(&sha,&zero,1);
	SHA256_Update(&sha,_publicKey.data(),_publicKey.size());
	SHA256_Update(&sha,&zero,1);
	SHA256_Final(dig,&sha);
	_signature = _keyPair->sign(dig);
}

bool Identity::locallyValidate(bool doAddressDerivationCheck) const
{
	SHA256_CTX sha;
	unsigned char dig[32];
	unsigned char idtype = IDENTITY_TYPE_NIST_P_521,zero = 0;
	SHA256_Init(&sha);
	SHA256_Update(&sha,_address.data(),ZT_ADDRESS_LENGTH);
	SHA256_Update(&sha,&zero,1);
	SHA256_Update(&sha,&idtype,1);
	SHA256_Update(&sha,&zero,1);
	SHA256_Update(&sha,_publicKey.data(),_publicKey.size());
	SHA256_Update(&sha,&zero,1);
	SHA256_Final(dig,&sha);

	return ((EllipticCurveKeyPair::verify(dig,_publicKey,_signature.data(),_signature.length()))&&((!doAddressDerivationCheck)||(deriveAddress(_publicKey.data(),_publicKey.size()) == _address)));
}

std::string Identity::toString(bool includePrivate) const
{
	std::string r;
	r.append(_address.toString());
	r.append(":1:"); // 1 == IDENTITY_TYPE_NIST_P_521
	r.append(Utils::base64Encode(_publicKey.data(),_publicKey.size()));
	r.push_back(':');
	r.append(Utils::base64Encode(_signature.data(),_signature.length()));
	if ((includePrivate)&&(_keyPair)) {
		r.push_back(':');
		r.append(Utils::base64Encode(_keyPair->priv().data(),_keyPair->priv().size()));
	}
	return r;
}

bool Identity::fromString(const char *str)
{
	delete _keyPair;
	_keyPair = (EllipticCurveKeyPair *)0;

	std::vector<std::string> fields(Utils::split(Utils::trim(std::string(str)).c_str(),":","",""));

	if (fields.size() < 4)
		return false;

	if (fields[1] != "1")
		return false; // version mismatch

	std::string b(Utils::unhex(fields[0]));
	if (b.length() != ZT_ADDRESS_LENGTH)
		return false;
	_address = b.data();

	b = Utils::base64Decode(fields[2]);
	if ((!b.length())||(b.length() > ZT_EC_MAX_BYTES))
		return false;
	_publicKey.set(b.data(),b.length());

	_signature = Utils::base64Decode(fields[3]);
	if (!_signature.length())
		return false;

	if (fields.size() >= 5) {
		b = Utils::base64Decode(fields[4]);
		if ((!b.length())||(b.length() > ZT_EC_MAX_BYTES))
			return false;
		_keyPair = new EllipticCurveKeyPair(_publicKey,EllipticCurveKey(b.data(),b.length()));
	}

	return true;
}

// These are core protocol parameters and can't be changed without a new
// identity type.
#define ZT_IDENTITY_DERIVEADDRESS_ROUNDS 4
#define ZT_IDENTITY_DERIVEADDRESS_MEMORY 33554432

Address Identity::deriveAddress(const void *keyBytes,unsigned int keyLen)
{
	unsigned char dig[32];
	Salsa20 s20a,s20b;
	SHA256_CTX sha;

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
	 * This is designed not to be parallelizable and to be resistant to
	 * implementation on things like GPUs with tiny-memory nodes and poor
	 * branching capability. Toward that end it throws branching and a large
	 * memory buffer into the mix. It can only be efficiently computed by a
	 * single core with at least ~32MB RAM.
	 *
	 * Search for "sequential memory hard algorithm" for academic references
	 * to similar concepts.
	 *
	 * Right now this takes ~1700ms on a 2.4ghz Intel Core i5. If this could
	 * be reduced to 1ms per derivation, it would take about 34 years to search
	 * the entire 40-bit address space for an average of ~17 years to generate
	 * a key colliding with a known existing address.
	 */

	// Initial starting digest
	SHA256_Init(&sha);
	SHA256_Update(&sha,(const unsigned char *)keyBytes,keyLen); // key
	SHA256_Final(dig,&sha);

	s20a.init(dig,256,"ZeroTier");

	unsigned char *ram = new unsigned char[ZT_IDENTITY_DERIVEADDRESS_MEMORY];

	// Encrypt and digest a large memory buffer for several rounds
	for(unsigned long i=0;i<ZT_IDENTITY_DERIVEADDRESS_MEMORY;++i)
		ram[i] = (unsigned char)(i & 0xff) ^ dig[i & 31];
	for(unsigned long r=0;r<ZT_IDENTITY_DERIVEADDRESS_ROUNDS;++r) {
		SHA256_Init(&sha);

		SHA256_Update(&sha,(const unsigned char *)keyBytes,keyLen);
		SHA256_Update(&sha,dig,32);

		for(unsigned long i=0;i<ZT_IDENTITY_DERIVEADDRESS_MEMORY;++i) {
			if (ram[i] == 17) // Forces a branch to be required
				ram[i] ^= dig[i & 31];
		}
		s20b.init(dig,256,"ZeroTier");
		s20a.encrypt(ram,ram,ZT_IDENTITY_DERIVEADDRESS_MEMORY);
		s20b.encrypt(ram,ram,ZT_IDENTITY_DERIVEADDRESS_MEMORY);
		SHA256_Update(&sha,ram,ZT_IDENTITY_DERIVEADDRESS_MEMORY);

		SHA256_Final(dig,&sha);
	}

	// Final digest, executed for twice our number of rounds
	SHA256_Init(&sha);
	for(unsigned long r=0;r<(ZT_IDENTITY_DERIVEADDRESS_ROUNDS * 2);++r) {
		SHA256_Update(&sha,(const unsigned char *)keyBytes,keyLen);
		SHA256_Update(&sha,ram,ZT_IDENTITY_DERIVEADDRESS_ROUNDS);
		SHA256_Update(&sha,dig,32);
		SHA256_Update(&sha,(const unsigned char *)keyBytes,keyLen);
	}
	SHA256_Final(dig,&sha);

	delete [] ram;

	return Address(dig); // first 5 bytes of dig[]
}

std::string Identity::encrypt(const Identity &to,const void *data,unsigned int len) const
{
	unsigned char key[64];
	unsigned char mac[32];
	unsigned char iv[8];

	if (!agree(to,key,sizeof(key)))
		return std::string();
	Utils::getSecureRandom(iv,8);
	for(int i=0;i<8;++i)
		key[i + 32] ^= iv[i]; // perturb HMAC key with IV so IV is effectively included in HMAC
	Salsa20 s20(key,256,iv);

	std::string compressed;
	compressed.reserve(len);
	Utils::compress((const char *)data,(const char *)data + len,Utils::StringAppendOutput(compressed));
	if (!compressed.length())
		return std::string();

	char *encrypted = new char[compressed.length() + 16];
	try {
		s20.encrypt(compressed.data(),encrypted + 16,(unsigned int)compressed.length());
		HMAC::sha256(key + 32,32,encrypted + 16,(unsigned int)compressed.length(),mac);
		for(int i=0;i<8;++i)
			encrypted[i] = iv[i];
		for(int i=0;i<8;++i)
			encrypted[i + 8] = mac[i];

		std::string s(encrypted,compressed.length() + 16);
		delete [] encrypted;
		return s;
	} catch ( ... ) {
		delete [] encrypted;
		return std::string();
	}
}

std::string Identity::decrypt(const Identity &from,const void *cdata,unsigned int len) const
{
	unsigned char key[64];
	unsigned char mac[32];

	if (len < 16)
		return std::string();

	if (!agree(from,key,sizeof(key)))
		return std::string();

	for(int i=0;i<8;++i)
		key[i + 32] ^= ((const unsigned char *)cdata)[i]; // apply IV to HMAC key
	HMAC::sha256(key + 32,32,((const char *)cdata) + 16,(unsigned int)(len - 16),mac);
	for(int i=0;i<8;++i) {
		if (((const unsigned char *)cdata)[i + 8] != mac[i])
			return std::string();
	}

	char *decbuf = new char[len - 16];
	try {
		Salsa20 s20(key,256,cdata); // first 8 bytes are IV
		len -= 16;
		s20.decrypt((const char *)cdata + 16,decbuf,len);

		std::string decompressed;
		if (Utils::decompress((const char *)decbuf,(const char *)decbuf + len,Utils::StringAppendOutput(decompressed))) {
			delete [] decbuf;
			return decompressed;
		} else {
			delete [] decbuf;
			return std::string();
		}
	} catch ( ... ) {
		delete [] decbuf;
		return std::string();
	}
}

} // namespace ZeroTier

