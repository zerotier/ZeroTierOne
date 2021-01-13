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

#ifndef ZT_MIMC52_HPP
#define ZT_MIMC52_HPP

#include "Constants.hpp"

namespace ZeroTier {
namespace MIMC52 {

/**
 * Compute proof of execution for the delay function
 *
 * @param challenge 256-bit challenge input to randomize algorithm, making it a unique function
 * @param rounds Number of rounds
 * @return Proof of execution of delay function (only least significant 52 bits are meaningful)
 */
uint64_t delay(const uint8_t challenge[32], unsigned long rounds);

/**
 * Verify a proof of execution
 *
 * @param challenge 256-bit challenge
 * @param rounds Number of rounds
 * @param proof Proof from delay()
 * @return True if proof is valid
 */
bool verify(const uint8_t challenge[32], unsigned long rounds, uint64_t proof);

} // namespace MIMC52
} // namespcae ZeroTier

#endif
