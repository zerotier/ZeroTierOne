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

#ifndef ZT_OS_HPP
#define ZT_OS_HPP

//
// This include file also auto-detects and canonicalizes some environment
// information defines:
//
// __LINUX__
// __APPLE__
// __BSD__ (OSX also defines this)
// __UNIX_LIKE__ (Linux, BSD, etc.)
// __WINDOWS__
//
// Also makes sure __BYTE_ORDER is defined reasonably.
//

#ifndef __GCC__
#if defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_1) || defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_2) || defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4) || defined(__INTEL_COMPILER) || defined(__clang__)
#define __GCC__
#endif
#endif

#ifdef _MSC_VER
#pragma warning(disable : 4290)
#pragma warning(disable : 4996)
#pragma warning(disable : 4101)
#endif

#if defined(_WIN32) || defined(_WIN64)
#ifndef __WINDOWS__
#define __WINDOWS__
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#undef __UNIX_LIKE__
#undef __BSD__
#include <WinSock2.h>
#include <Windows.h>
#endif

#if defined(__linux__) || defined(linux) || defined(__LINUX__) || defined(__linux)
#ifndef __LINUX__
#define __LINUX__
#endif
#ifndef __UNIX_LIKE__
#define __UNIX_LIKE__
#endif
#include <endian.h>
#endif

#ifdef __APPLE__
#include <TargetConditionals.h>
#ifndef __UNIX_LIKE__
#define __UNIX_LIKE__
#endif
#ifndef __BSD__
#define __BSD__
#endif
#include <machine/endian.h>
#endif

#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#ifndef __UNIX_LIKE__
#define __UNIX_LIKE__
#endif
#ifndef __BSD__
#define __BSD__
#endif
#include <sys/endian.h>
#ifndef __BYTE_ORDER
#define __BYTE_ORDER _BYTE_ORDER
#define __LITTLE_ENDIAN _LITTLE_ENDIAN
#define __BIG_ENDIAN _BIG_ENDIAN
#endif
#endif
#ifdef __NetBSD__
#ifndef RTF_MULTICAST
#define RTF_MULTICAST 0x20000000
#endif
#endif

// Avoid unaligned type casts on all but x86/x64 architecture.
#if (!(defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(__x86_64) || defined(_M_AMD64) || defined(_M_X64) || defined(i386) || defined(__i386) || defined(__i386__) || defined(__i486__) || defined(__i586__) || defined(__i686__) || defined(_M_IX86) || defined(__X86__) || defined(_X86_) || defined(__I86__) || defined(__INTEL__) || defined(__386)))
#ifndef ZT_NO_UNALIGNED_ACCESS
#define ZT_NO_UNALIGNED_ACCESS
#endif
#endif

// Assume little endian if not defined on Mac and Windows as these don't run on any BE architectures.
#if (defined(__APPLE__) || defined(__WINDOWS__)) && (!defined(__BYTE_ORDER))
#undef __BYTE_ORDER
#undef __LITTLE_ENDIAN
#undef __BIG_ENDIAN
#define __BIG_ENDIAN 4321
#define __LITTLE_ENDIAN 1234
#define __BYTE_ORDER 1234
#endif
#ifndef __BYTE_ORDER
#include <endian.h>
#endif

#if (defined(__GNUC__) && (__GNUC__ >= 3)) || (defined(__INTEL_COMPILER) && (__INTEL_COMPILER >= 800)) || defined(__clang__)
#define ZT_ALWAYS_INLINE __attribute__((always_inline)) inline
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
#define inline inline
#define likely(x) (x)
#endif
#ifndef unlikely
#define unlikely(x) (x)
#endif
#endif

#if __cplusplus > 199711L
#ifndef __CPP11__
#define __CPP11__
#endif
#endif
#ifndef __CPP11__
#define nullptr (0)
#define constexpr ZT_ALWAYS_INLINE
#endif

#ifdef SOCKET
#define ZT_SOCKET SOCKET
#else
#define ZT_SOCKET int
#endif
#ifdef INVALID_SOCKET
#define ZT_INVALID_SOCKET INVALID_SOCKET
#else
#define ZT_INVALID_SOCKET -1
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

#ifndef ZT_ALWAYS_INLINE
#define ZT_ALWAYS_INLINE inline
#endif

#endif
