/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_SHA512_HPP
#define ZT_SHA512_HPP

#define ZT_SHA512_DIGEST_LEN 64

namespace ZeroTier {

/**
 * SHA-512 digest algorithm
 */
class SHA512
{
public:
	static void hash(void *digest,const void *data,unsigned int len);
};

} // namespace ZeroTier

#endif
