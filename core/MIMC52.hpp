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

/*
 * This is a verifiable delay function (or serial proof of work) based on
 * the MIMC prime field hash construction. MIMC is very fast to execute in
 * one direction and inherently a lot slower in another, allowing expensive
 * work to be quickly verified. The 52-bit algorithm implemented here can
 * run very fast on a variety of systems by taking advantage of the
 * interesting double precision FPU modular multiplication hack described
 * here:
 *
 * https://stackoverflow.com/a/50479693
 *
 * 52 bits is not sufficient for high strength cryptographic applications
 * like block chains, but is good enough to add a deterministic delay to
 * identity generation. That's its only purpose here. This is used as the
 * delay function for "type 1" identities to replace the ad-hoc memory hard
 * hash used in "type 0." This is both simpler and faster to verify.
 */

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
