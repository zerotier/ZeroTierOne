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

#ifndef ZT_OS_HPP
#define ZT_OS_HPP

/* Uncomment this to force a whole lot of debug output. */
#define ZT_DEBUG_SPEW

#if ! defined(__GNUC__) && (defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_1) || defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_2) || defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4) || defined(__INTEL_COMPILER) || defined(__clang__))
#define __GNUC__ 3
#endif

#if defined(_WIN32) || defined(_WIN64)

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x06010000
#endif

#ifdef _MSC_VER
#pragma warning(disable : 4290)
#pragma warning(disable : 4996)
#pragma warning(disable : 4101)
#endif

#ifndef __WINDOWS__
#define __WINDOWS__ 1
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifdef __UNIX_LIKE__
#undef __UNIX_LIKE__
#endif
#ifdef __BSD__
#undef __BSD__
#endif

#include <Shlobj.h>
#include <WinSock2.h>
#include <Windows.h>
#include <memoryapi.h>
#include <shlwapi.h>
#include <sys/param.h>
#include <ws2tcpip.h>

#endif /* Microsoft Windows */

#ifndef __WINDOWS__
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif /* NOT Microsoft Windows */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if (defined(__amd64) || defined(__amd64__) || defined(__x86_64) || defined(__x86_64__) || defined(__AMD64) || defined(__AMD64__) || defined(_M_X64))
#define ZT_ARCH_X64 1
#include <emmintrin.h>
#include <immintrin.h>
#include <xmmintrin.h>
#endif
#if defined(ZT_ARCH_X64) || defined(i386) || defined(__i386) || defined(__i386__) || defined(__i486__) || defined(__i586__) || defined(__i686__) || defined(_M_IX86) || defined(__X86__) || defined(_X86_) || defined(__I86__)                 \
    || defined(__INTEL__) || defined(__386)
#define ZT_ARCH_X86 1
#endif

#if ! defined(ZT_ARCH_X86)
#ifndef ZT_NO_UNALIGNED_ACCESS
#define ZT_NO_UNALIGNED_ACCESS 1
#endif
#endif

#if defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(ZT_ARCH_ARM_HAS_NEON)
#if (defined(__APPLE__) && ! defined(__LP64__)) || (defined(__ANDROID__) && defined(__arm__))
#ifdef ZT_ARCH_ARM_HAS_NEON
#undef ZT_ARCH_ARM_HAS_NEON
#endif
#else
#ifndef ZT_ARCH_ARM_HAS_NEON
#define ZT_ARCH_ARM_HAS_NEON 1
#endif
#endif
#include <arm_neon.h>
/*#include <arm_acle.h>*/
#endif

#ifdef __APPLE__
#include <TargetConditionals.h>
#include <machine/endian.h>
#ifndef __UNIX_LIKE__
#define __UNIX_LIKE__ 1
#endif
#ifndef __BSD__
#define __BSD__ 1
#endif
#ifndef __BYTE_ORDER
#define __BYTE_ORDER    __DARWIN_BYTE_ORDER
#define __BIG_ENDIAN    __DARWIN_BIG_ENDIAN
#define __LITTLE_ENDIAN __DARWIN_LITTLE_ENDIAN
#endif
#endif

#if defined(__linux__) || defined(linux) || defined(__LINUX__) || defined(__linux)
#ifndef __LINUX__
#define __LINUX__ 1
#endif
#ifndef __UNIX_LIKE__
#define __UNIX_LIKE__ 1
#endif
#include <endian.h>
#endif

#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#ifndef __UNIX_LIKE__
#define __UNIX_LIKE__ 1
#endif
#ifndef __BSD__
#define __BSD__ 1
#endif
#include <sys/endian.h>
#ifndef RTF_MULTICAST
#define RTF_MULTICAST 0x20000000
#endif
#endif

#ifdef __WINDOWS__
#define ZT_PATH_SEPARATOR   '\\'
#define ZT_PATH_SEPARATOR_S "\\"
#define ZT_EOL_S            "\r\n"
#else
#define ZT_PATH_SEPARATOR   '/'
#define ZT_PATH_SEPARATOR_S "/"
#define ZT_EOL_S            "\n"
#endif

#ifdef SOCKET
#define ZT_SOCKET SOCKET
#else
#define ZT_SOCKET int
#endif
#ifdef INVALID_SOCKET
#define ZT_INVALID_SOCKET INVALID_SOCKET
#else
#define ZT_INVALID_SOCKET (-1)
#endif

#ifdef __cplusplus
#if __cplusplus >= 199711L

#include <atomic>
#ifndef __CPP11__
#define __CPP11__ 1
#endif

#if __cplusplus >= 201703L
#define ZT_MAYBE_UNUSED [[maybe_unused]]
#ifndef __CPP17__
#define __CPP17__ 1
#endif
#else
#define ZT_MAYBE_UNUSED
#endif

#endif

#if defined(ZT_ARCH_X64) || defined(__aarch64__)
#ifndef ZT_ARCH_APPEARS_64BIT
#define ZT_ARCH_APPEARS_64BIT 1
#endif
#endif
#ifdef UINTPTR_MAX
#if UINTPTR_MAX == UINT64_MAX
#ifndef ZT_ARCH_APPEARS_64BIT
#define ZT_ARCH_APPEARS_64BIT 1
#endif
#endif
#endif

#ifndef ZT_INLINE
#ifdef ZT_DEBUG
#define ZT_INLINE
#else
#if defined(__GNUC__) || defined(__clang__)
#define ZT_INLINE __attribute__((always_inline)) inline
#else
#define ZT_INLINE inline
#endif
#endif
#endif

/* Right now we fail if no C++11. The core could be ported to old C++ compilers
 * if a shim for <atomic> were included. */
#ifndef __CPP11__
#error TODO: to build on pre-c++11 compilers we will need to make a subset of std::atomic for integers
#define nullptr   (0)
#define constexpr ZT_INLINE
#define noexcept  throw()
#define explicit
#endif
#endif

#ifndef restrict
#if defined(__GNUC__) || defined(__clang__)
#define restrict __restrict__
#else
#define restrict
#endif
#endif

#ifndef likely
#if defined(__GNUC__) || defined(__clang__)
#define likely(x) __builtin_expect((x), 1)
#else
#define likely(x) x
#endif
#endif

#ifndef unlikely
#if defined(__GNUC__) || defined(__clang__)
#define unlikely(x) __builtin_expect((x), 0)
#else
#define unlikely(x) x
#endif
#endif

#if defined(__SIZEOF_INT128__) || ((defined(ZT_ARCH_X64) || defined(__aarch64__)) && defined(__GNUC__))
#ifdef __SIZEOF_INT128__
#define ZT_HAVE_UINT128 1
typedef unsigned __int128 uint128_t;
#else
#define ZT_HAVE_UINT128 1
typedef unsigned uint128_t __attribute__((mode(TI)));
#endif
#endif

#if ! defined(__BYTE_ORDER) && defined(__BYTE_ORDER__)
#define __BYTE_ORDER    __BYTE_ORDER__
#define __LITTLE_ENDIAN __ORDER_LITTLE_ENDIAN__
#define __BIG_ENDIAN    __ORDER_BIG_ENDIAN__
#endif
#if ! defined(__BYTE_ORDER) && defined(BYTE_ORDER)
#define __BYTE_ORDER    BYTE_ORDER
#define __LITTLE_ENDIAN LITTLE_ENDIAN
#define __BIG_ENDIAN    BIG_ENDIAN
#endif
#if ! defined(__BYTE_ORDER) && defined(_BYTE_ORDER)
#define __BYTE_ORDER    _BYTE_ORDER
#define __LITTLE_ENDIAN _LITTLE_ENDIAN
#define __BIG_ENDIAN    _BIG_ENDIAN
#endif

#define ZT_VA_ARGS(...) , ##__VA_ARGS__

#ifdef ZT_DEBUG_SPEW
#define ZT_SPEW(f, ...) fprintf(stderr, "%s:%d(%s): " f ZT_EOL_S, __FILE__, __LINE__, __FUNCTION__ ZT_VA_ARGS(__VA_ARGS__))
#else
#define ZT_SPEW(f, ...)
#endif

#endif
