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

#include <openssl/bn.h>
#include <openssl/obj_mac.h>
#include <openssl/rand.h>
#include <openssl/ec.h>
#include <openssl/ecdh.h>
#include <openssl/ecdsa.h>
#include <openssl/sha.h>

#include "EllipticCurveKey.hpp"
#include "EllipticCurveKeyPair.hpp"

namespace ZeroTier {

class _EC_Group
{
public:
	_EC_Group()
	{
		g = EC_GROUP_new_by_curve_name(ZT_EC_OPENSSL_CURVE);
	}
	~_EC_Group() {}
	EC_GROUP *g;
};
static _EC_Group ZT_EC_GROUP;

/**
 * Key derivation function
 *
 * TODO:
 * If/when we document the protocol, this will have to be documented as
 * well. It's a fairly standard KDF that uses SHA-256 to transform the
 * raw EC key. It's generally considered good crypto practice to do this
 * to eliminate the possibility of leaking information from EC exchange to
 * downstream algorithms.
 *
 * In our code it is used to produce a two 32-bit keys. One key is used
 * for Salsa20 and the other for HMAC-SHA-256. They are generated together
 * as a single 64-bit key.
 */
static void *_zt_EC_KDF(const void *in,size_t inlen,void *out,size_t *outlen)
{
	SHA256_CTX sha;
	unsigned char dig[SHA256_DIGEST_LENGTH];

	SHA256_Init(&sha);
	SHA256_Update(&sha,(const unsigned char *)in,inlen);
	SHA256_Final(dig,&sha);
	for(unsigned long i=0,k=0;i<(unsigned long)*outlen;) {
		if (k == SHA256_DIGEST_LENGTH) {
			k = 0;
			SHA256_Init(&sha);
			SHA256_Update(&sha,(const unsigned char *)in,inlen);
			SHA256_Update(&sha,dig,SHA256_DIGEST_LENGTH);
			SHA256_Final(dig,&sha);
		}
		((unsigned char *)out)[i++] = dig[k++];
	}

	return out;
}

EllipticCurveKeyPair::EllipticCurveKeyPair() :
	_pub(),
	_priv(),
	_internal_key((void *)0)
{
}

EllipticCurveKeyPair::EllipticCurveKeyPair(const EllipticCurveKeyPair &pair) :
	_pub(pair._pub),
	_priv(pair._priv),
	_internal_key((void *)0)
{
}

EllipticCurveKeyPair::EllipticCurveKeyPair(const EllipticCurveKey &pubk,const EllipticCurveKey &privk) :
	_pub(pubk),
	_priv(privk),
	_internal_key((void *)0)
{
}

EllipticCurveKeyPair::~EllipticCurveKeyPair()
{
	if (_internal_key)
		EC_KEY_free((EC_KEY *)_internal_key);
}

const EllipticCurveKeyPair &EllipticCurveKeyPair::operator=(const EllipticCurveKeyPair &pair)
{
	if (_internal_key)
		EC_KEY_free((EC_KEY *)_internal_key);
	_pub = pair._pub;
	_priv = pair._priv;
	_internal_key = (void *)0;
	return *this;
}

bool EllipticCurveKeyPair::generate()
{
	unsigned char tmp[16384];
	EC_KEY *key;
	int len;

	// Make sure OpenSSL libcrypto has sufficient randomness (on most
	// platforms it auto-seeds, so this is a sanity check).
	if (!RAND_status()) {
#if defined(__APPLE__) || defined(__linux__) || defined(linux) || defined(__LINUX__) || defined(__linux)
		FILE *rf = fopen("/dev/urandom","r");
		if (rf) {
			fread(tmp,sizeof(tmp),1,rf);
			fclose(rf);
		} else {
			fprintf(stderr,"FATAL: could not open /dev/urandom\n");
			exit(-1);
		}
		RAND_seed(tmp,sizeof(tmp));
#else
#ifdef _WIN32
		error need win32;
#else
		error;
#endif
#endif
	}

	key = EC_KEY_new();
	if (!key) return false;

	if (!EC_KEY_set_group(key,ZT_EC_GROUP.g)) {
		EC_KEY_free(key);
		return false;
	}

	if (!EC_KEY_generate_key(key)) {
		EC_KEY_free(key);
		return false;
	}

	memset(_priv._key,0,sizeof(_priv._key));
	len = BN_num_bytes(EC_KEY_get0_private_key(key));
	if ((len > ZT_EC_PRIME_BYTES)||(len < 0)) {
		EC_KEY_free(key);
		return false;
	}
	BN_bn2bin(EC_KEY_get0_private_key(key),&(_priv._key[ZT_EC_PRIME_BYTES - len]));
	_priv._bytes = ZT_EC_PRIME_BYTES;

	memset(_pub._key,0,sizeof(_pub._key));
	len = EC_POINT_point2oct(ZT_EC_GROUP.g,EC_KEY_get0_public_key(key),POINT_CONVERSION_COMPRESSED,_pub._key,sizeof(_pub._key),0);
	if (len != ZT_EC_PUBLIC_KEY_BYTES) {
		EC_KEY_free(key);
		return false;
	}
	_pub._bytes = ZT_EC_PUBLIC_KEY_BYTES;

	if (_internal_key)
		EC_KEY_free((EC_KEY *)_internal_key);
	_internal_key = key;

	return true;
}

bool EllipticCurveKeyPair::agree(const EllipticCurveKey &theirPublicKey,unsigned char *agreedUponKey,unsigned int agreedUponKeyLength) const
{
	if (theirPublicKey._bytes != ZT_EC_PUBLIC_KEY_BYTES)
		return false;

	if (!_internal_key) {
		if (!(const_cast <EllipticCurveKeyPair *> (this))->initInternalKey())
			return false;
	}

	EC_POINT *pub = EC_POINT_new(ZT_EC_GROUP.g);
	if (!pub)
		return false;
	EC_POINT_oct2point(ZT_EC_GROUP.g,pub,theirPublicKey._key,ZT_EC_PUBLIC_KEY_BYTES,0);

	int i = ECDH_compute_key(agreedUponKey,agreedUponKeyLength,pub,(EC_KEY *)_internal_key,&_zt_EC_KDF);
	EC_POINT_free(pub);

	return (i == (int)agreedUponKeyLength);
}

std::string EllipticCurveKeyPair::sign(const void *sha256) const
{
	unsigned char buf[256];
	std::string sigbin;

	if (!_internal_key) {
		if (!(const_cast <EllipticCurveKeyPair *> (this))->initInternalKey())
			return std::string();
	}

	ECDSA_SIG *sig = ECDSA_do_sign((const unsigned char *)sha256,SHA256_DIGEST_LENGTH,(EC_KEY *)_internal_key);
	if (!sig)
		return std::string();

	int rlen = BN_num_bytes(sig->r);
	if ((rlen > 255)||(rlen <= 0)) {
		ECDSA_SIG_free(sig);
		return std::string();
	}
	sigbin.push_back((char)rlen);
	BN_bn2bin(sig->r,buf);
	sigbin.append((const char *)buf,rlen);

	int slen = BN_num_bytes(sig->s);
	if ((slen > 255)||(slen <= 0)) {
		ECDSA_SIG_free(sig);
		return std::string();
	}
	sigbin.push_back((char)slen);
	BN_bn2bin(sig->s,buf);
	sigbin.append((const char *)buf,slen);

	ECDSA_SIG_free(sig);

	return sigbin;
}

std::string EllipticCurveKeyPair::sign(const void *data,unsigned int len) const
{
	SHA256_CTX sha;
	unsigned char dig[SHA256_DIGEST_LENGTH];

	SHA256_Init(&sha);
	SHA256_Update(&sha,(const unsigned char *)data,len);
	SHA256_Final(dig,&sha);

	return sign(dig);
}

bool EllipticCurveKeyPair::verify(const void *sha256,const EllipticCurveKey &pk,const void *sigbytes,unsigned int siglen)
{
	bool result = false;
	ECDSA_SIG *sig = (ECDSA_SIG *)0;
	EC_POINT *pub = (EC_POINT *)0;
	EC_KEY *key = (EC_KEY *)0;
	int rlen,slen;

	if (!siglen)
		goto verify_sig_return;
	rlen = ((const unsigned char *)sigbytes)[0];
	if (!rlen)
		goto verify_sig_return;
	if (siglen < (unsigned int)(rlen + 2))
		goto verify_sig_return;
	slen = ((const unsigned char *)sigbytes)[rlen + 1];
	if (!slen)
		goto verify_sig_return;
	if (siglen < (unsigned int)(rlen + slen + 2))
		goto verify_sig_return;

	sig = ECDSA_SIG_new();
	if (!sig)
		goto verify_sig_return;

	BN_bin2bn((const unsigned char *)sigbytes + 1,rlen,sig->r);
	BN_bin2bn((const unsigned char *)sigbytes + (1 + rlen + 1),slen,sig->s);

	pub = EC_POINT_new(ZT_EC_GROUP.g);
	if (!pub)
		goto verify_sig_return;
	EC_POINT_oct2point(ZT_EC_GROUP.g,pub,pk._key,ZT_EC_PUBLIC_KEY_BYTES,0);

	key = EC_KEY_new();
	if (!key)
		goto verify_sig_return;
	if (!EC_KEY_set_group(key,ZT_EC_GROUP.g))
		goto verify_sig_return;
	EC_KEY_set_public_key(key,pub);

	result = (ECDSA_do_verify((const unsigned char *)sha256,SHA256_DIGEST_LENGTH,sig,key) == 1);

verify_sig_return:
	if (key)
		EC_KEY_free(key);
	if (pub)
		EC_POINT_free(pub);
	if (sig)
		ECDSA_SIG_free(sig);

	return result;
}

bool EllipticCurveKeyPair::verify(const void *data,unsigned int len,const EllipticCurveKey &pk,const void *sigbytes,unsigned int siglen)
{
	SHA256_CTX sha;
	unsigned char dig[SHA256_DIGEST_LENGTH];

	SHA256_Init(&sha);
	SHA256_Update(&sha,(const unsigned char *)data,len);
	SHA256_Final(dig,&sha);

	return verify(dig,pk,sigbytes,siglen);
}

bool EllipticCurveKeyPair::initInternalKey()
{
	EC_KEY *key;
	EC_POINT *kxy;
	BIGNUM *pn;

	if (_priv._bytes != ZT_EC_PRIME_BYTES) return false;
	if (_pub._bytes != ZT_EC_PUBLIC_KEY_BYTES) return false;

	key = EC_KEY_new();
	if (!key) return false;

	if (!EC_KEY_set_group(key,ZT_EC_GROUP.g)) {
		EC_KEY_free(key);
		return false;
	}

	pn = BN_new();
	if (!pn) {
		EC_KEY_free(key);
		return false;
	}
	if (!BN_bin2bn(_priv._key,ZT_EC_PRIME_BYTES,pn)) {
		BN_free(pn);
		EC_KEY_free(key);
		return false;
	}
	if (!EC_KEY_set_private_key(key,pn)) {
		BN_free(pn);
		EC_KEY_free(key);
		return false;
	}
	BN_free(pn);

	kxy = EC_POINT_new(ZT_EC_GROUP.g);
	if (!kxy) {
		EC_KEY_free(key);
		return false;
	}
	EC_POINT_oct2point(ZT_EC_GROUP.g,kxy,_pub._key,ZT_EC_PUBLIC_KEY_BYTES,0);
	if (!EC_KEY_set_public_key(key,kxy)) {
		EC_POINT_free(kxy);
		EC_KEY_free(key);
		return false;
	}
	EC_POINT_free(kxy);

	if (_internal_key)
		EC_KEY_free((EC_KEY *)_internal_key);
	_internal_key = key;

	return true;
}

} // namespace ZeroTier

