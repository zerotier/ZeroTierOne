/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
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

#ifndef ZT_LZ4_HPP
#define ZT_LZ4_HPP

#include "Constants.hpp"

namespace ZeroTier {

int LZ4_compress_fast(const char *source,char *dest,int inputSize,int maxOutputSize,int acceleration = 1) noexcept;
int LZ4_decompress_safe(const char *source,char *dest,int compressedSize,int maxDecompressedSize) noexcept;

} // namespace ZeroTier

#endif
