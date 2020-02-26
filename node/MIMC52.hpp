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

#ifndef ZT_MIMC52_HPP
#define ZT_MIMC52_HPP

#include "Constants.hpp"

namespace ZeroTier {

/**
 * Compute a number of rounds of the MIMC52 verifiable delay function
 *
 * @param salt Input salt
 * @param saltSize Size of salt in bytes
 * @param rounds Number of rounds to compute
 * @return Proof that rounds were computed (proof of CPU cycles spent)
 */
uint64_t mimc52Delay(const void *salt,unsigned int saltSize,unsigned long rounds);

/**
 * Verify a MIMC52 proof of work
 *
 * @param salt Original input salt
 * @param saltSize Size of salt in bytes
 * @param rounds Number of rounds originally computed
 * @param proof Proof to verify (most significant 12 bits are ignored)
 * @return True if proof is valid
 */
bool mimc52Verify(const void *salt,unsigned int saltSize,unsigned long rounds,uint64_t proof);

} // namespace ZeroTier

#endif
