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

#ifndef ZT_SHA512_HPP
#define ZT_SHA512_HPP

#include "Constants.hpp"

#ifdef __APPLE__
#include <CommonCrypto/CommonDigest.h>
#endif

#define ZT_SHA512_DIGEST_LEN 64
#define ZT_SHA384_DIGEST_LEN 48

#define ZT_SHA512_BLOCK_SIZE 128
#define ZT_SHA384_BLOCK_SIZE 128

#define ZT_HMACSHA384_LEN 48

namespace ZeroTier {

// SHA384 and SHA512 are actually in the standard libraries on MacOS and iOS
#ifdef __APPLE__
#define ZT_HAVE_NATIVE_SHA512 1
static ZT_ALWAYS_INLINE void SHA512(void *digest,const void *data,unsigned int len)
{
	CC_SHA512_CTX ctx;
	CC_SHA512_Init(&ctx);
	CC_SHA512_Update(&ctx,data,len);
	CC_SHA512_Final(reinterpret_cast<unsigned char *>(digest),&ctx);
}
static ZT_ALWAYS_INLINE void SHA384(void *digest,const void *data,unsigned int len)
{
	CC_SHA512_CTX ctx;
	CC_SHA384_Init(&ctx);
	CC_SHA384_Update(&ctx,data,len);
	CC_SHA384_Final(reinterpret_cast<unsigned char *>(digest),&ctx);
}
static ZT_ALWAYS_INLINE void SHA384(void *digest,const void *data0,unsigned int len0,const void *data1,unsigned int len1)
{
	CC_SHA512_CTX ctx;
	CC_SHA384_Init(&ctx);
	CC_SHA384_Update(&ctx,data0,len0);
	CC_SHA384_Update(&ctx,data1,len1);
	CC_SHA384_Final(reinterpret_cast<unsigned char *>(digest),&ctx);
}
#endif

#ifndef ZT_HAVE_NATIVE_SHA512
void SHA512(void *digest,const void *data,unsigned int len);
void SHA384(void *digest,const void *data,unsigned int len);
void SHA384(void *digest,const void *data0,unsigned int len0,const void *data1,unsigned int len1);
#endif

/**
 * Compute HMAC SHA-384 using a 256-bit key
 *
 * @param key Secret key
 * @param msg Message to HMAC
 * @param msglen Length of message
 * @param mac Buffer to fill with result
 */
void HMACSHA384(const uint8_t key[32],const void *msg,unsigned int msglen,uint8_t mac[48]);

/**
 * Compute KBKDF (key-based key derivation function) using HMAC-SHA-384 as a PRF
 *
 * @param key Source master key
 * @param label A label indicating the key's purpose in the ZeroTier system
 * @param context An arbitrary "context" or zero if not applicable
 * @param iter Key iteration for generation of multiple keys for the same label/context
 * @param out Output to receive derived key
 */
void KBKDFHMACSHA384(const uint8_t key[32],char label,char context,uint32_t iter,uint8_t out[32]);

} // namespace ZeroTier

#endif
