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

/*
 * This header and its implementation in Tests.cpp contain assertion tests,
 * self-tests, cryptographic tests, and fuzzing for the ZeroTier core.
 *
 * To build these ensure that ZT_ENABLE_TESTS is defined during build time.
 * Otherwise they are omitted.
 *
 * These symbols are defined extern "C" so tests can be called from regular
 * C code, which is important for use via CGo or in plain C projects.
 *
 * The ZT_T_PRINTF macro defaults to printf() but if it's defined at compile
 * time (also must be set while building Tests.cpp) it can specify another
 * function to use for output. Defining it to a no-op can be used to disable
 * output.
 *
 * Each test function returns NULL if the tests succeeds or an error message
 * on test failure.
 *
 * Only the fuzzing test functions are thread-safe. Other test functions
 * should not be called concurrently. It's okay to call different tests from
 * different threads, but not the same test.
 */

#ifndef ZT_TESTS_HPP
#define ZT_TESTS_HPP

#ifdef ZT_ENABLE_TESTS

#ifdef __cplusplus
#include <cstdint>
#include <cstdio>
extern "C" {
#else
#include <stdint.h>
#include <stdio.h>
#endif

#ifndef ZT_T_PRINTF
#define ZT_T_PRINTF(fmt,...) printf((fmt),##__VA_ARGS__),fflush(stdout)
#endif

// Basic self-tests ---------------------------------------------------------------------------------------------------

const char *ZTT_general();
const char *ZTT_crypto();
const char *ZTT_defragmenter();

// Benchmarks ---------------------------------------------------------------------------------------------------------

const char *ZTT_benchmarkCrypto();

// Fuzzing ------------------------------------------------------------------------------------------------------------


// --------------------------------------------------------------------------------------------------------------------

#ifdef __cplusplus
}
#endif

#endif // ZT_ENABLE_TESTS

#endif
