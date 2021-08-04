/*
 * Copyright (c)2013-2021 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_LZ4_HPP
#define ZT_LZ4_HPP

#include "Constants.hpp"

namespace ZeroTier {

#define LZ4_MAX_INPUT_SIZE       0x7E000000
#define LZ4_COMPRESSBOUND(isize) ((unsigned)(isize) > (unsigned)LZ4_MAX_INPUT_SIZE ? 0 : (isize) + ((isize) / 255) + 16)

int LZ4_compress_fast(const char *source, char *dest, int inputSize, int maxOutputSize, int acceleration = 1) noexcept;
int LZ4_decompress_safe(const char *source, char *dest, int compressedSize, int maxDecompressedSize) noexcept;

}   // namespace ZeroTier

#endif
