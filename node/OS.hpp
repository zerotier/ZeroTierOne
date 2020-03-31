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

// This include file uses various macros and other tricks to auto-detect, define, and
// canonicalize a bunch of macros and types used throughout the ZeroTier core.

#ifndef ZT_OS_HPP
#define ZT_OS_HPP

#include <stdint.h> // NOLINT(modernize-deprecated-headers,hicpp-deprecated-headers)
#include <stdlib.h> // NOLINT(modernize-deprecated-headers,hicpp-deprecated-headers)
#include <string.h> // NOLINT(modernize-deprecated-headers,hicpp-deprecated-headers)
#include <stdio.h> // NOLINT(modernize-deprecated-headers,hicpp-deprecated-headers)

#if defined(_WIN32) || defined(_WIN64)
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
#undef __UNIX_LIKE__
#undef __BSD__
#include <WinSock2.h>
#include <Windows.h>
#include <sys/param.h>
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

#if !defined(__GNUC__) && (defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_1) || defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_2) || defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4) || defined(__INTEL_COMPILER) || defined(__clang__))
#define __GNUC__ 3
#endif

#ifdef __cplusplus
#if __cplusplus > 199711L
#include <atomic>
#ifndef __CPP11__
#define __CPP11__
#endif
#endif
#ifndef __CPP11__
// Beyond that defining nullptr, constexpr, and noexcept should allow us to still build on these. So far we've
// avoided deeper C++11 features like lambdas in the core until we're 100% sure all the ancient targets are gone.
#error TODO: to build on pre-c++11 compilers we will need to make a subset of std::atomic for integers
#define nullptr (0)
#define constexpr ZT_INLINE
#define noexcept throw()
#endif
#endif

#ifdef __GNUC__
#ifndef ZT_DEBUG
#define ZT_INLINE __attribute__((always_inline)) inline
#endif
#ifndef restrict
#define restrict __restrict__
#endif
#ifndef likely
#define likely(x) __builtin_expect((x),1)
#endif
#ifndef unlikely
#define unlikely(x) __builtin_expect((x),0)
#endif
#else /* not GCC-like */
#ifndef restrict
#define restrict
#endif
#ifndef likely
#define likely(x) (x)
#endif
#ifndef unlikely
#define unlikely(x) (x)
#endif
#endif

#if (defined(__amd64) || defined(__amd64__) || defined(__x86_64) || defined(__x86_64__) || defined(__AMD64) || defined(__AMD64__) || defined(_M_X64))
#define ZT_ARCH_X64 1
#endif
#if defined(ZT_ARCH_X64) || defined(i386) || defined(__i386) || defined(__i386__) || defined(__i486__) || defined(__i586__) || defined(__i686__) || defined(_M_IX86) || defined(__X86__) || defined(_X86_) || defined(__I86__) || defined(__INTEL__) || defined(__386)
#define ZT_ARCH_X86 1
#endif

#if !defined(ZT_ARCH_X86)
#ifndef ZT_NO_UNALIGNED_ACCESS
#define ZT_NO_UNALIGNED_ACCESS 1
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

#if defined(__linux__) || defined(linux) || defined(__LINUX__) || defined(__linux)
#ifndef __LINUX__
#define __LINUX__ 1
#endif
#ifndef __UNIX_LIKE__
#define __UNIX_LIKE__ 1
#endif
#include <endian.h>
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
#define __BYTE_ORDER __DARWIN_BYTE_ORDER
#define __BIG_ENDIAN __DARWIN_BIG_ENDIAN
#define __LITTLE_ENDIAN __DARWIN_LITTLE_ENDIAN
#endif
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
#define ZT_PATH_SEPARATOR '\\'
#define ZT_PATH_SEPARATOR_S "\\"
#define ZT_EOL_S "\r\n"
#else
#define ZT_PATH_SEPARATOR '/'
#define ZT_PATH_SEPARATOR_S "/"
#define ZT_EOL_S "\n"
#endif

#if !defined(__BYTE_ORDER) && defined(__BYTE_ORDER__)
#define __BYTE_ORDER __BYTE_ORDER__
#define __LITTLE_ENDIAN __ORDER_LITTLE_ENDIAN__
#define __BIG_ENDIAN __ORDER_BIG_ENDIAN__
#endif
#if !defined(__BYTE_ORDER) && defined(BYTE_ORDER)
#define __BYTE_ORDER BYTE_ORDER
#define __LITTLE_ENDIAN LITTLE_ENDIAN
#define __BIG_ENDIAN BIG_ENDIAN
#endif
#if !defined(__BYTE_ORDER) && defined(_BYTE_ORDER)
#define __BYTE_ORDER _BYTE_ORDER
#define __LITTLE_ENDIAN _LITTLE_ENDIAN
#define __BIG_ENDIAN _BIG_ENDIAN
#endif

#ifndef ZT_INLINE
#ifdef ZT_DEBUG
#define ZT_INLINE
#else
#define ZT_INLINE inline
#endif
#endif

#endif
