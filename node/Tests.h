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
 * The macro ZT_STANDALONE_TESTS will also build a main() function for these
 * tests for creating a stand-alone test executable. It will return zero if
 * all tests pass and non-zero if at least one test fails.
 *
 * These symbols are defined extern "C" so tests can be called from regular
 * C code, which is important for use via CGo or in plain C projects.
 *
 * The ZT_T_PRINTF macro defaults to printf() but if it's defined at compile
 * time (it must be set while building Tests.cpp) it can specify another
 * function to use for output. Defining it to a no-op can be used to disable
 * output.
 *
 * Each test function returns NULL if the tests succeeds or an error message
 * on test failure.
 *
 * Be aware that fuzzing tests can and will crash the program if a serious
 * error is discovered. This is the point. It's also beneficial to run these
 * in "valgrind" or a similar tool to detect marginal bad behvaior.
 */

#ifndef ZT_TESTS_HPP
#define ZT_TESTS_HPP

#ifdef ZT_ENABLE_TESTS

#include <stdint.h> // NOLINT(modernize-deprecated-headers,hicpp-deprecated-headers)
#include <stdio.h> // NOLINT(modernize-deprecated-headers,hicpp-deprecated-headers)

#ifndef ZT_T_PRINTF
#define ZT_T_PRINTF(fmt,...) printf((fmt),##__VA_ARGS__),fflush(stdout)
#endif

/**
 * Test platform, compiler behavior, utility functions, and core classes
 */
const char *ZTT_general();

/**
 * Test crypto using test vectors and simple scenarios
 *
 * This is not an absolutely exhaustive test, just a sanity check to make sure
 * crypto routines are basically working.
 */
const char *ZTT_crypto();

/**
 * Run benchmarks of cryptographic routines and common constructions
 */
const char *ZTT_benchmarkCrypto();

#ifdef __cplusplus
}
#endif

#endif // ZT_ENABLE_TESTS

#endif
