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

namespace ZeroTier {

#if 0

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
#endif

#endif

#ifndef ZT_HAVE_NATIVE_SHA512
void SHA512(void *digest,const void *data,unsigned int len);
void SHA384(void *digest,const void *data,unsigned int len);
#endif

} // namespace ZeroTier

#endif
