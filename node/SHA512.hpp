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

#ifndef ZT_SHA512_HPP
#define ZT_SHA512_HPP

#include "Constants.hpp"

#ifdef __APPLE__
#include <CommonCrypto/CommonDigest.h>
#endif

#ifdef ZT_USE_LIBCRYPTO
#include <openssl/sha.h>
#endif

#define ZT_SHA512_DIGEST_LEN 64
#define ZT_SHA384_DIGEST_LEN 48

#define ZT_SHA512_BLOCK_SIZE 128
#define ZT_SHA384_BLOCK_SIZE 128

#define ZT_HMACSHA384_LEN 48

namespace ZeroTier {

#ifdef __APPLE__
#define ZT_HAVE_NATIVE_SHA512 1
static inline void SHA512(void *digest,const void *data,unsigned int len)
{
	CC_SHA512_CTX ctx;
	CC_SHA512_Init(&ctx);
	CC_SHA512_Update(&ctx,data,len);
	CC_SHA512_Final(reinterpret_cast<unsigned char *>(digest),&ctx);
}
static inline void SHA384(void *digest,const void *data,unsigned int len)
{
	CC_SHA512_CTX ctx;
	CC_SHA384_Init(&ctx);
	CC_SHA384_Update(&ctx,data,len);
	CC_SHA384_Final(reinterpret_cast<unsigned char *>(digest),&ctx);
}
static inline void SHA384ab(void *digest,const void *data0,unsigned int len0,const void *data1,unsigned int len1)
{
	CC_SHA512_CTX ctx;
	CC_SHA384_Init(&ctx);
	CC_SHA384_Update(&ctx,data0,len0);
	CC_SHA384_Update(&ctx,data1,len1);
	CC_SHA384_Final(reinterpret_cast<unsigned char *>(digest),&ctx);
}
#endif

#ifdef ZT_USE_LIBCRYPTO
#define ZT_HAVE_NATIVE_SHA512 1
static inline void SHA512(void *digest,const void *data,unsigned int len)
{
	SHA512_CTX ctx;
	SHA512_Init(&ctx);
	SHA512_Update(&ctx,data,len);
	SHA512_Final(reinterpret_cast<unsigned char *>(digest),&ctx);
}
static inline void SHA384(void *digest,const void *data,unsigned int len)
{
	SHA512_CTX ctx;
	SHA384_Init(&ctx);
	SHA384_Update(&ctx,data,len);
	SHA384_Final(reinterpret_cast<unsigned char *>(digest),&ctx);
}
static inline void SHA384ab(void *digest,const void *data0,unsigned int len0,const void *data1,unsigned int len1)
{
	SHA512_CTX ctx;
	SHA384_Init(&ctx);
	SHA384_Update(&ctx,data0,len0);
	SHA384_Update(&ctx,data1,len1);
	SHA384_Final(reinterpret_cast<unsigned char *>(digest),&ctx);
}
#endif

#ifndef ZT_HAVE_NATIVE_SHA512
void SHA512(void *digest,const void *data,unsigned int len);
void SHA384(void *digest,const void *data,unsigned int len);
void SHA384ab(void *digest,const void *data0,unsigned int len0,const void *data1,unsigned int len1);
#endif

static inline void HMACSHA384(const uint8_t key[32],const void *msg,const unsigned int msglen,uint8_t mac[48])
{
	uint64_t kInPadded[16];
	uint64_t outer[22]; // output padded key | H(input padded key | msg)

#ifdef ZT_NO_TYPE_PUNNING
	for(int i=0;i<32;++i) ((uint8_t *)kInPadded)[i] = key[i] ^ 0x36;
	for(int i=4;i<16;++i) kInPadded[i] = 0x3636363636363636ULL;
	for(int i=0;i<32;++i) ((uint8_t *)outer)[i] = key[i] ^ 0x5c;
	for(int i=4;i<16;++i) outer[i] = 0x5c5c5c5c5c5c5c5cULL;
#else
	{
		const uint64_t k0 = ((const uint64_t *)key)[0];
		const uint64_t k1 = ((const uint64_t *)key)[1];
		const uint64_t k2 = ((const uint64_t *)key)[2];
		const uint64_t k3 = ((const uint64_t *)key)[3];
		kInPadded[0] = k0 ^ 0x3636363636363636ULL;
		kInPadded[0] = k1 ^ 0x3636363636363636ULL;
		kInPadded[0] = k2 ^ 0x3636363636363636ULL;
		kInPadded[0] = k3 ^ 0x3636363636363636ULL;
		for(int i=4;i<16;++i) kInPadded[i] = 0x3636363636363636ULL;
		outer[0] = k0 ^ 0x5c5c5c5c5c5c5c5cULL;
		outer[1] = k1 ^ 0x5c5c5c5c5c5c5c5cULL;
		outer[2] = k2 ^ 0x5c5c5c5c5c5c5c5cULL;
		outer[3] = k3 ^ 0x5c5c5c5c5c5c5c5cULL;
		for(int i=4;i<16;++i) outer[i] = 0x5c5c5c5c5c5c5c5cULL;
	}
#endif

	SHA384ab(((uint8_t *)outer) + 128,kInPadded,128,msg,msglen); // H(input padded key | msg)
	SHA384(mac,outer,176); // H(output padded key | H(input padded key | msg))
}

} // namespace ZeroTier

#endif
