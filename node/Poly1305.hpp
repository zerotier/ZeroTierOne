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

#ifndef ZT_POLY1305_HPP
#define ZT_POLY1305_HPP

namespace ZeroTier {

#define ZT_POLY1305_KEY_LEN 32
#define ZT_POLY1305_MAC_LEN 16

/**
 * Compute a one-time authentication code
 *
 * @param auth Buffer to receive code -- MUST be 16 bytes in length
 * @param data Data to authenticate
 * @param len Length of data to authenticate in bytes
 * @param key 32-byte one-time use key to authenticate data (must not be reused)
 */
void poly1305(void *auth,const void *data,unsigned int len,const void *key) noexcept;

} // namespace ZeroTier

#endif
