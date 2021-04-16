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

#ifndef ZT_DEFAULTS_HPP
#define ZT_DEFAULTS_HPP

#include "Constants.hpp"

namespace ZeroTier {
namespace Defaults {

#define DEFAULT_CERTIFICATE_COUNT 0

extern const uint8_t *CERTIFICATE[DEFAULT_CERTIFICATE_COUNT];
extern unsigned int CERTIFICATE_SIZE[DEFAULT_CERTIFICATE_COUNT];

} // namespace Defaults
} // namespace ZeroTier

#endif
