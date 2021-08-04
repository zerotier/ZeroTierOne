/*
 *  LZ4 - Fast LZ compression algorithm
 *  Header File
 *  Copyright (C) 2011-2016, Yann Collet.

   BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   You can contact the author at :
    - LZ4 homepage : http://www.lz4.org
    - LZ4 source repository : https://github.com/lz4/lz4
*/

// Some modifications were made for ZeroTier but this code remains under the
// original LZ4 license.

#include "LZ4.hpp"

#include "Utils.hpp"

#include <cstddef>
#include <cstdlib>
#include <cstring>

#ifdef _MSC_VER
#define FORCE_INLINE __forceinline
#include <intrin.h>
#pragma warning(disable : 4127) /* disable: C4127: conditional expression is constant */
#pragma warning(disable : 4293) /* disable: C4293: too large shift (32-bits) */
#else
#define FORCE_INLINE ZT_INLINE
#endif

namespace ZeroTier {

namespace {

// #define LZ4_VERSION_MAJOR	1	/* for breaking interface changes  */
// #define LZ4_VERSION_MINOR	7	/* for new (non-breaking) interface capabilities */
// #define LZ4_VERSION_RELEASE  5	/* for tweaks, bug-fixes, or development */
#define LZ4_MEMORY_USAGE 14

typedef union LZ4_stream_u LZ4_stream_t; /* incomplete type (defined later) */

FORCE_INLINE void LZ4_resetStream(LZ4_stream_t *streamPtr);

#define LZ4_HASHLOG       (LZ4_MEMORY_USAGE - 2)
#define LZ4_HASH_SIZE_U32 (1 << LZ4_HASHLOG) /* required as macro for static allocation */

typedef struct {
    uint32_t hashTable[LZ4_HASH_SIZE_U32];
    uint32_t currentOffset;
    uint32_t initCheck;
    const uint8_t *dictionary;
    uint8_t *bufferStart; /* obsolete, used for slideInputBuffer */
    uint32_t dictSize;
} LZ4_stream_t_internal;

typedef struct {
    const uint8_t *externalDict;
    size_t extDictSize;
    const uint8_t *prefixEnd;
    size_t prefixSize;
} LZ4_streamDecode_t_internal;

#define LZ4_STREAMSIZE_U64 ((1 << (LZ4_MEMORY_USAGE - 3)) + 4)
union LZ4_stream_u {
    unsigned long long table[LZ4_STREAMSIZE_U64];
    LZ4_stream_t_internal internal_donotuse;
}; /* previously typedef'd to LZ4_stream_t */

#define LZ4_STREAMDECODESIZE_U64 4
union LZ4_streamDecode_u {
    unsigned long long table[LZ4_STREAMDECODESIZE_U64];
    LZ4_streamDecode_t_internal internal_donotuse;
}; /* previously typedef'd to LZ4_streamDecode_t */

#ifndef HEAPMODE
#define HEAPMODE 0
#endif

#ifdef ZT_NO_UNALIGNED_ACCESS
#define LZ4_FORCE_MEMORY_ACCESS 0
#else
#define LZ4_FORCE_MEMORY_ACCESS 2
#endif

#if defined(_MSC_VER) && defined(_WIN32_WCE)
#define LZ4_FORCE_SW_BITCOUNT
#endif

#define ALLOCATOR(n, s) calloc(n, s)
#define FREEMEM         free

typedef uint8_t BYTE;
typedef uint16_t U16;
typedef uint32_t U32;
typedef int32_t S32;
typedef uint64_t U64;
typedef uintptr_t uptrval;
typedef uintptr_t reg_t;

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define LZ4_isLittleEndian() (1)
#else
#define LZ4_isLittleEndian() (0)
#endif

#if defined(LZ4_FORCE_MEMORY_ACCESS) && (LZ4_FORCE_MEMORY_ACCESS == 2)
FORCE_INLINE U16 LZ4_read16(const void *memPtr) { return *(const U16 *)memPtr; }

FORCE_INLINE U32 LZ4_read32(const void *memPtr) { return *(const U32 *)memPtr; }

FORCE_INLINE reg_t LZ4_read_ARCH(const void *memPtr) { return *(const reg_t *)memPtr; }

FORCE_INLINE void LZ4_write16(void *memPtr, U16 value) { *(U16 *)memPtr = value; }

FORCE_INLINE void LZ4_write32(void *memPtr, U32 value) { *(U32 *)memPtr = value; }

#elif defined(LZ4_FORCE_MEMORY_ACCESS) && (LZ4_FORCE_MEMORY_ACCESS == 1)
typedef union {
    U16 u16;
    U32 u32;
    reg_t uArch;
} __attribute__((packed)) unalign;
FORCE_INLINE U16 LZ4_read16(const void *ptr) { return ((const unalign *)ptr)->u16; }
FORCE_INLINE U32 LZ4_read32(const void *ptr) { return ((const unalign *)ptr)->u32; }
FORCE_INLINE reg_t LZ4_read_ARCH(const void *ptr) { return ((const unalign *)ptr)->uArch; }
FORCE_INLINE void LZ4_write16(void *memPtr, U16 value) { ((unalign *)memPtr)->u16 = value; }
FORCE_INLINE void LZ4_write32(void *memPtr, U32 value) { ((unalign *)memPtr)->u32 = value; }
#else  /* safe and portable */
FORCE_INLINE U16 LZ4_read16(const void *memPtr)
{
    U16 val;
    Utils::copy(&val, memPtr, sizeof(val));
    return val;
}
FORCE_INLINE U32 LZ4_read32(const void *memPtr)
{
    U32 val;
    Utils::copy(&val, memPtr, sizeof(val));
    return val;
}
FORCE_INLINE reg_t LZ4_read_ARCH(const void *memPtr)
{
    reg_t val;
    Utils::copy(&val, memPtr, sizeof(val));
    return val;
}
FORCE_INLINE void LZ4_write16(void *memPtr, U16 value) { Utils::copy(memPtr, &value, sizeof(value)); }
FORCE_INLINE void LZ4_write32(void *memPtr, U32 value) { Utils::copy(memPtr, &value, sizeof(value)); }
#endif /* LZ4_FORCE_MEMORY_ACCESS */

FORCE_INLINE U16 LZ4_readLE16(const void *memPtr)
{
    if (LZ4_isLittleEndian()) {
        return LZ4_read16(memPtr);
    }
    else {
        const BYTE *p = (const BYTE *)memPtr;
        return (U16)((U16)p[0] + (p[1] << 8));
    }
}

FORCE_INLINE void LZ4_writeLE16(void *memPtr, U16 value)
{
    if (LZ4_isLittleEndian()) {
        LZ4_write16(memPtr, value);
    }
    else {
        BYTE *p = (BYTE *)memPtr;
        p[0]    = (BYTE)value;
        p[1]    = (BYTE)(value >> 8);
    }
}

FORCE_INLINE void LZ4_copy8(void *dst, const void *src) { Utils::copy<8>(dst, src); }

FORCE_INLINE void LZ4_wildCopy(void *dstPtr, const void *srcPtr, void *dstEnd)
{
    BYTE *d       = (BYTE *)dstPtr;
    const BYTE *s = (const BYTE *)srcPtr;
    BYTE *const e = (BYTE *)dstEnd;
    do {
        LZ4_copy8(d, s);
        d += 8;
        s += 8;
    } while (d < e);
}

#define MINMATCH 4

#define WILDCOPYLENGTH 8
#define LASTLITERALS   5
#define MFLIMIT        (WILDCOPYLENGTH + MINMATCH)
static const int LZ4_minLength = (MFLIMIT + 1);

#define KB *(1 << 10)
// #define MB *(1 <<20)
// #define GB *(1U<<30)

#define MAXD_LOG     16
#define MAX_DISTANCE ((1 << MAXD_LOG) - 1)

#define ML_BITS  4
#define ML_MASK  ((1U << ML_BITS) - 1)
#define RUN_BITS (8 - ML_BITS)
#define RUN_MASK ((1U << RUN_BITS) - 1)

// #define LZ4_STATIC_ASSERT(c)	{ enum { LZ4_static_assert = 1/(int)(!!(c)) }; }   /* use only *after* variable
// declarations */

FORCE_INLINE unsigned LZ4_NbCommonBytes(reg_t val)
{
    if (LZ4_isLittleEndian()) {
        if (sizeof(val) == 8) {
#if defined(_MSC_VER) && defined(_WIN64) && !defined(LZ4_FORCE_SW_BITCOUNT)
            unsigned long r = 0;
            _BitScanForward64(&r, (U64)val);
            return (int)(r >> 3);
#elif (defined(__clang__) || (defined(__GNUC__) && (__GNUC__ >= 3))) && !defined(LZ4_FORCE_SW_BITCOUNT)
            return (__builtin_ctzll((U64)val) >> 3);
#else
            static const int DeBruijnBytePos[64] = { 0, 0, 0, 0, 0, 1, 1, 2, 0, 3, 1, 3, 1, 4, 2, 7, 0, 2, 3, 6, 1, 5, 3, 5, 1, 3, 4, 4, 2, 5, 6, 7, 7, 0, 1, 2, 3, 3, 4, 6, 2, 6, 5, 5, 3, 4, 5, 6, 7, 1, 2, 4, 6, 4, 4, 5, 7, 2, 6, 5, 7, 6, 7, 7 };
            return DeBruijnBytePos[((U64)((val & -(long long)val) * 0x0218A392CDABBD3FULL)) >> 58];
#endif
        }
        else /* 32 bits */ {
#if defined(_MSC_VER) && !defined(LZ4_FORCE_SW_BITCOUNT)
            unsigned long r;
            _BitScanForward(&r, (U32)val);
            return (int)(r >> 3);
#elif (defined(__clang__) || (defined(__GNUC__) && (__GNUC__ >= 3))) && !defined(LZ4_FORCE_SW_BITCOUNT)
            return (__builtin_ctz((U32)val) >> 3);
#else
            static const int DeBruijnBytePos[32] = { 0, 0, 3, 0, 3, 1, 3, 0, 3, 2, 2, 1, 3, 2, 0, 1, 3, 3, 1, 2, 2, 2, 2, 0, 3, 1, 2, 0, 1, 0, 1, 1 };
            return DeBruijnBytePos[((U32)((val & -(S32)val) * 0x077CB531U)) >> 27];
#endif
        }
    }
    else /* Big Endian CPU */ {
        if (sizeof(val) == 8) {
#if defined(_MSC_VER) && defined(_WIN64) && !defined(LZ4_FORCE_SW_BITCOUNT)
            unsigned long r = 0;
            _BitScanReverse64(&r, val);
            return (unsigned)(r >> 3);
#elif (defined(__clang__) || (defined(__GNUC__) && (__GNUC__ >= 3))) && !defined(LZ4_FORCE_SW_BITCOUNT)
            return (__builtin_clzll((U64)val) >> 3);
#else
            unsigned r;
            if (!(val >> 32)) {
                r = 4;
            }
            else {
                r = 0;
                val >>= 32;
            }
            if (!(val >> 16)) {
                r += 2;
                val >>= 8;
            }
            else {
                val >>= 24;
            }
            r += (!val);
            return r;
#endif
        }
        else /* 32 bits */ {
#if defined(_MSC_VER) && !defined(LZ4_FORCE_SW_BITCOUNT)
            unsigned long r = 0;
            _BitScanReverse(&r, (unsigned long)val);
            return (unsigned)(r >> 3);
#elif (defined(__clang__) || (defined(__GNUC__) && (__GNUC__ >= 3))) && !defined(LZ4_FORCE_SW_BITCOUNT)
            return (__builtin_clz((U32)val) >> 3);
#else
            unsigned r;
            if (!(val >> 16)) {
                r = 2;
                val >>= 8;
            }
            else {
                r = 0;
                val >>= 24;
            }
            r += (!val);
            return r;
#endif
        }
    }
}

#define STEPSIZE sizeof(reg_t)
FORCE_INLINE unsigned LZ4_count(const BYTE *pIn, const BYTE *pMatch, const BYTE *pInLimit)
{
    const BYTE *const pStart = pIn;

    while (likely(pIn < pInLimit - (STEPSIZE - 1))) {
        reg_t const diff = LZ4_read_ARCH(pMatch) ^ LZ4_read_ARCH(pIn);
        if (!diff) {
            pIn += STEPSIZE;
            pMatch += STEPSIZE;
            continue;
        }
        pIn += LZ4_NbCommonBytes(diff);
        return (unsigned)(pIn - pStart);
    }

    if ((STEPSIZE == 8) && (pIn < (pInLimit - 3)) && (LZ4_read32(pMatch) == LZ4_read32(pIn))) {
        pIn += 4;
        pMatch += 4;
    }
    if ((pIn < (pInLimit - 1)) && (LZ4_read16(pMatch) == LZ4_read16(pIn))) {
        pIn += 2;
        pMatch += 2;
    }
    if ((pIn < pInLimit) && (*pMatch == *pIn))
        pIn++;
    return (unsigned)(pIn - pStart);
}

static const int LZ4_64Klimit    = ((64 KB) + (MFLIMIT - 1));
static const U32 LZ4_skipTrigger = 6; /* Increase this value ==> compression run slower on incompressible data */

typedef enum { notLimited = 0, limitedOutput = 1 } limitedOutput_directive;
typedef enum { byPtr, byU32, byU16 } tableType_t;

typedef enum { noDict = 0, withPrefix64k, usingExtDict } dict_directive;
typedef enum { noDictIssue = 0, dictSmall } dictIssue_directive;

typedef enum { endOnOutputSize = 0, endOnInputSize = 1 } endCondition_directive;
typedef enum { full = 0, partial = 1 } earlyEnd_directive;

FORCE_INLINE int LZ4_compressBound(int isize) { return LZ4_COMPRESSBOUND(isize); }

FORCE_INLINE U32 LZ4_hash4(U32 sequence, tableType_t const tableType)
{
    if (tableType == byU16)
        return ((sequence * 2654435761U) >> ((MINMATCH * 8) - (LZ4_HASHLOG + 1)));
    else
        return ((sequence * 2654435761U) >> ((MINMATCH * 8) - LZ4_HASHLOG));
}

FORCE_INLINE U32 LZ4_hash5(U64 sequence, tableType_t const tableType)
{
    static const U64 prime5bytes = 889523592379ULL;
    static const U64 prime8bytes = 11400714785074694791ULL;
    const U32 hashLog            = (tableType == byU16) ? LZ4_HASHLOG + 1 : LZ4_HASHLOG;
    if (LZ4_isLittleEndian())
        return (U32)(((sequence << 24) * prime5bytes) >> (64 - hashLog));
    else
        return (U32)(((sequence >> 24) * prime8bytes) >> (64 - hashLog));
}

FORCE_INLINE U32 LZ4_hashPosition(const void *const p, tableType_t const tableType)
{
    if ((sizeof(reg_t) == 8) && (tableType != byU16))
        return LZ4_hash5(LZ4_read_ARCH(p), tableType);
    return LZ4_hash4(LZ4_read32(p), tableType);
}

FORCE_INLINE void LZ4_putPositionOnHash(const BYTE *p, U32 h, void *tableBase, tableType_t const tableType, const BYTE *srcBase)
{
    switch (tableType) {
        case byPtr: {
            const BYTE **hashTable = (const BYTE **)tableBase;
            hashTable[h]           = p;
            return;
        }
        case byU32: {
            U32 *hashTable = (U32 *)tableBase;
            hashTable[h]   = (U32)(p - srcBase);
            return;
        }
        case byU16: {
            U16 *hashTable = (U16 *)tableBase;
            hashTable[h]   = (U16)(p - srcBase);
            return;
        }
    }
}

FORCE_INLINE void LZ4_putPosition(const BYTE *p, void *tableBase, tableType_t tableType, const BYTE *srcBase)
{
    U32 const h = LZ4_hashPosition(p, tableType);
    LZ4_putPositionOnHash(p, h, tableBase, tableType, srcBase);
}

FORCE_INLINE const BYTE *LZ4_getPositionOnHash(U32 h, void *tableBase, tableType_t tableType, const BYTE *srcBase)
{
    if (tableType == byPtr) {
        const BYTE **hashTable = (const BYTE **)tableBase;
        return hashTable[h];
    }
    if (tableType == byU32) {
        const U32 *const hashTable = (U32 *)tableBase;
        return hashTable[h] + srcBase;
    }
    {
        const U16 *const hashTable = (U16 *)tableBase;
        return hashTable[h] + srcBase;
    } /* default, to ensure a return */
}

FORCE_INLINE const BYTE *LZ4_getPosition(const BYTE *p, void *tableBase, tableType_t tableType, const BYTE *srcBase)
{
    U32 const h = LZ4_hashPosition(p, tableType);
    return LZ4_getPositionOnHash(h, tableBase, tableType, srcBase);
}

FORCE_INLINE int LZ4_compress_generic(LZ4_stream_t_internal *const cctx, const char *const source, char *const dest, const int inputSize, const int maxOutputSize, const limitedOutput_directive outputLimited, const tableType_t tableType, const dict_directive dict, const dictIssue_directive dictIssue, const U32 acceleration)
{
    const BYTE *ip = (const BYTE *)source;
    const BYTE *base;
    const BYTE *lowLimit;
    const BYTE *const lowRefLimit = ip - cctx->dictSize;
    const BYTE *const dictionary  = cctx->dictionary;
    const BYTE *const dictEnd     = dictionary + cctx->dictSize;
    const ptrdiff_t dictDelta     = dictEnd - (const BYTE *)source;
    const BYTE *anchor            = (const BYTE *)source;
    const BYTE *const iend        = ip + inputSize;
    const BYTE *const mflimit     = iend - MFLIMIT;
    const BYTE *const matchlimit  = iend - LASTLITERALS;

    BYTE *op           = (BYTE *)dest;
    BYTE *const olimit = op + maxOutputSize;

    U32 forwardH;

    /* Init conditions */
    if ((U32)inputSize > (U32)LZ4_MAX_INPUT_SIZE)
        return 0; /* Unsupported inputSize, too large (or negative) */
    switch (dict) {
        case noDict:
        default:
            base     = (const BYTE *)source;
            lowLimit = (const BYTE *)source;
            break;
        case withPrefix64k:
            base     = (const BYTE *)source - cctx->currentOffset;
            lowLimit = (const BYTE *)source - cctx->dictSize;
            break;
        case usingExtDict:
            base     = (const BYTE *)source - cctx->currentOffset;
            lowLimit = (const BYTE *)source;
            break;
    }
    if ((tableType == byU16) && (inputSize >= LZ4_64Klimit))
        return 0; /* Size too large (not within 64K limit) */
    if (inputSize < LZ4_minLength)
        goto _last_literals; /* Input too small, no compression (all literals) */

    /* First Byte */
    LZ4_putPosition(ip, cctx->hashTable, tableType, base);
    ip++;
    forwardH = LZ4_hashPosition(ip, tableType);

    /* Main Loop */
    for (;;) {
        ptrdiff_t refDelta = 0;
        const BYTE *match;
        BYTE *token;

        /* Find a match */
        {
            const BYTE *forwardIp  = ip;
            unsigned step          = 1;
            unsigned searchMatchNb = acceleration << LZ4_skipTrigger;
            do {
                U32 const h = forwardH;
                ip          = forwardIp;
                forwardIp += step;
                step = (searchMatchNb++ >> LZ4_skipTrigger);

                if (unlikely(forwardIp > mflimit))
                    goto _last_literals;

                match = LZ4_getPositionOnHash(h, cctx->hashTable, tableType, base);
                if (dict == usingExtDict) {
                    if (match < (const BYTE *)source) {
                        refDelta = dictDelta;
                        lowLimit = dictionary;
                    }
                    else {
                        refDelta = 0;
                        lowLimit = (const BYTE *)source;
                    }
                }
                forwardH = LZ4_hashPosition(forwardIp, tableType);
                LZ4_putPositionOnHash(ip, h, cctx->hashTable, tableType, base);

            } while (((dictIssue == dictSmall) ? (match < lowRefLimit) : 0) || ((tableType == byU16) ? 0 : (match + MAX_DISTANCE < ip)) || (LZ4_read32(match + refDelta) != LZ4_read32(ip)));
        }

        /* Catch up */
        while (((ip > anchor) & (match + refDelta > lowLimit)) && (unlikely(ip[-1] == match[refDelta - 1]))) {
            ip--;
            match--;
        }

        /* Encode Literals */
        {
            unsigned const litLength = (unsigned)(ip - anchor);
            token                    = op++;
            if ((outputLimited) && /* Check output buffer overflow */
                (unlikely(op + litLength + (2 + 1 + LASTLITERALS) + (litLength / 255) > olimit)))
                return 0;
            if (litLength >= RUN_MASK) {
                int len = (int)litLength - RUN_MASK;
                *token  = (RUN_MASK << ML_BITS);
                for (; len >= 255; len -= 255)
                    *op++ = 255;
                *op++ = (BYTE)len;
            }
            else
                *token = (BYTE)(litLength << ML_BITS);

            /* Copy Literals */
            LZ4_wildCopy(op, anchor, op + litLength);
            op += litLength;
        }

    _next_match:
        /* Encode Offset */
        LZ4_writeLE16(op, (U16)(ip - match));
        op += 2;

        /* Encode MatchLength */
        {
            unsigned matchCode;

            if ((dict == usingExtDict) && (lowLimit == dictionary)) {
                const BYTE *limit;
                match += refDelta;
                limit = ip + (dictEnd - match);
                if (limit > matchlimit)
                    limit = matchlimit;
                matchCode = LZ4_count(ip + MINMATCH, match + MINMATCH, limit);
                ip += MINMATCH + matchCode;
                if (ip == limit) {
                    unsigned const more = LZ4_count(ip, (const BYTE *)source, matchlimit);
                    matchCode += more;
                    ip += more;
                }
            }
            else {
                matchCode = LZ4_count(ip + MINMATCH, match + MINMATCH, matchlimit);
                ip += MINMATCH + matchCode;
            }

            if (outputLimited && /* Check output buffer overflow */
                (unlikely(op + (1 + LASTLITERALS) + (matchCode >> 8) > olimit)))
                return 0;
            if (matchCode >= ML_MASK) {
                *token += ML_MASK;
                matchCode -= ML_MASK;
                LZ4_write32(op, 0xFFFFFFFF);
                while (matchCode >= 4 * 255)
                    op += 4, LZ4_write32(op, 0xFFFFFFFF), matchCode -= 4 * 255;
                op += matchCode / 255;
                *op++ = (BYTE)(matchCode % 255);
            }
            else
                *token += (BYTE)(matchCode);
        }

        anchor = ip;

        /* Test end of chunk */
        if (ip > mflimit)
            break;

        /* Fill table */
        LZ4_putPosition(ip - 2, cctx->hashTable, tableType, base);

        /* Test next position */
        match = LZ4_getPosition(ip, cctx->hashTable, tableType, base);
        if (dict == usingExtDict) {
            if (match < (const BYTE *)source) {
                refDelta = dictDelta;
                lowLimit = dictionary;
            }
            else {
                refDelta = 0;
                lowLimit = (const BYTE *)source;
            }
        }
        LZ4_putPosition(ip, cctx->hashTable, tableType, base);
        if (((dictIssue == dictSmall) ? (match >= lowRefLimit) : 1) && (match + MAX_DISTANCE >= ip) && (LZ4_read32(match + refDelta) == LZ4_read32(ip))) {
            token  = op++;
            *token = 0;
            goto _next_match;
        }

        /* Prepare next loop */
        forwardH = LZ4_hashPosition(++ip, tableType);
    }

_last_literals:
    /* Encode Last Literals */
    {
        size_t const lastRun = (size_t)(iend - anchor);
        if ((outputLimited) && /* Check output buffer overflow */
            ((op - (BYTE *)dest) + lastRun + 1 + ((lastRun + 255 - RUN_MASK) / 255) > (U32)maxOutputSize))
            return 0;
        if (lastRun >= RUN_MASK) {
            size_t accumulator = lastRun - RUN_MASK;
            *op++              = RUN_MASK << ML_BITS;
            for (; accumulator >= 255; accumulator -= 255)
                *op++ = 255;
            *op++ = (BYTE)accumulator;
        }
        else {
            *op++ = (BYTE)(lastRun << ML_BITS);
        }
        Utils::copy(op, anchor, lastRun);
        op += lastRun;
    }

    /* End */
    return (int)(((char *)op) - dest);
}

ZT_INLINE int LZ4_compress_fast_extState(void *state, const char *source, char *dest, int inputSize, int maxOutputSize, int acceleration)
{
    LZ4_stream_t_internal *ctx = &((LZ4_stream_t *)state)->internal_donotuse;
    LZ4_resetStream((LZ4_stream_t *)state);
    // if (acceleration < 1) acceleration = ACCELERATION_DEFAULT;

    if (maxOutputSize >= LZ4_compressBound(inputSize)) {
        if (inputSize < LZ4_64Klimit)
            return LZ4_compress_generic(ctx, source, dest, inputSize, 0, notLimited, byU16, noDict, noDictIssue, acceleration);
        else
            return LZ4_compress_generic(ctx, source, dest, inputSize, 0, notLimited, (sizeof(void *) == 8) ? byU32 : byPtr, noDict, noDictIssue, acceleration);
    }
    else {
        if (inputSize < LZ4_64Klimit)
            return LZ4_compress_generic(ctx, source, dest, inputSize, maxOutputSize, limitedOutput, byU16, noDict, noDictIssue, acceleration);
        else
            return LZ4_compress_generic(ctx, source, dest, inputSize, maxOutputSize, limitedOutput, (sizeof(void *) == 8) ? byU32 : byPtr, noDict, noDictIssue, acceleration);
    }
}

FORCE_INLINE void LZ4_resetStream(LZ4_stream_t *LZ4_stream) { Utils::zero<sizeof(LZ4_stream_t)>(LZ4_stream); }

FORCE_INLINE int LZ4_decompress_generic(
    const char *const source, char *const dest, int inputSize, int outputSize, /* If endOnInput==endOnInputSize, this value is the max size of Output Buffer. */

    int endOnInput,              /* endOnOutputSize, endOnInputSize */
    int partialDecoding,         /* full, partial */
    int targetOutputSize,        /* only used if partialDecoding==partial */
    int dict,                    /* noDict, withPrefix64k, usingExtDict */
    const BYTE *const lowPrefix, /* == dest when no prefix */
    const BYTE *const dictStart, /* only if dict==usingExtDict */
    const size_t dictSize        /* note : = 0 if noDict */
)
{
    /* Local Variables */
    const BYTE *ip         = (const BYTE *)source;
    const BYTE *const iend = ip + inputSize;

    BYTE *op         = (BYTE *)dest;
    BYTE *const oend = op + outputSize;
    BYTE *cpy;
    BYTE *oexit                = op + targetOutputSize;
    const BYTE *const lowLimit = lowPrefix - dictSize;

    const BYTE *const dictEnd   = (const BYTE *)dictStart + dictSize;
    const unsigned dec32table[] = { 0, 1, 2, 1, 4, 4, 4, 4 };
    const int dec64table[]      = { 0, 0, 0, -1, 0, 1, 2, 3 };

    const int safeDecode  = (endOnInput == endOnInputSize);
    const int checkOffset = ((safeDecode) && (dictSize < (int)(64 KB)));

    /* Special cases */
    if ((partialDecoding) && (oexit > oend - MFLIMIT))
        oexit = oend - MFLIMIT; /* targetOutputSize too high => decode everything */
    if ((endOnInput) && (unlikely(outputSize == 0)))
        return ((inputSize == 1) && (*ip == 0)) ? 0 : -1; /* Empty output buffer */
    if ((!endOnInput) && (unlikely(outputSize == 0)))
        return (*ip == 0 ? 1 : -1);

    /* Main Loop : decode sequences */
    while (1) {
        size_t length;
        const BYTE *match;
        size_t offset;

        /* get literal length */
        unsigned const token = *ip++;
        if ((length = (token >> ML_BITS)) == RUN_MASK) {
            unsigned s;
            do {
                s = *ip++;
                length += s;
            } while (likely(endOnInput ? ip < iend - RUN_MASK : 1) & (s == 255));
            if ((safeDecode) && unlikely((uptrval)(op) + length < (uptrval)(op)))
                goto _output_error; /* overflow detection */
            if ((safeDecode) && unlikely((uptrval)(ip) + length < (uptrval)(ip)))
                goto _output_error; /* overflow detection */
        }

        /* copy literals */
        cpy = op + length;
        if (((endOnInput) && ((cpy > (partialDecoding ? oexit : oend - MFLIMIT)) || (ip + length > iend - (2 + 1 + LASTLITERALS)))) || ((!endOnInput) && (cpy > oend - WILDCOPYLENGTH))) {
            if (partialDecoding) {
                if (cpy > oend)
                    goto _output_error; /* Error : write attempt beyond end of output buffer */
                if ((endOnInput) && (ip + length > iend))
                    goto _output_error; /* Error : read attempt beyond end of input buffer */
            }
            else {
                if ((!endOnInput) && (cpy != oend))
                    goto _output_error; /* Error : block decoding must stop exactly there */
                if ((endOnInput) && ((ip + length != iend) || (cpy > oend)))
                    goto _output_error; /* Error : input must be consumed */
            }
            Utils::copy(op, ip, length);
            ip += length;
            op += length;
            break; /* Necessarily EOF, due to parsing restrictions */
        }
        LZ4_wildCopy(op, ip, cpy);
        ip += length;
        op = cpy;

        /* get offset */
        offset = LZ4_readLE16(ip);
        ip += 2;
        match = op - offset;
        if ((checkOffset) && (unlikely(match < lowLimit)))
            goto _output_error;       /* Error : offset outside buffers */
        LZ4_write32(op, (U32)offset); /* costs ~1%; silence an msan warning when offset==0 */

        /* get matchlength */
        length = token & ML_MASK;
        if (length == ML_MASK) {
            unsigned s;
            do {
                s = *ip++;
                if ((endOnInput) && (ip > iend - LASTLITERALS))
                    goto _output_error;
                length += s;
            } while (s == 255);
            if ((safeDecode) && unlikely((uptrval)(op) + length < (uptrval)op))
                goto _output_error; /* overflow detection */
        }
        length += MINMATCH;

        /* check external dictionary */
        if ((dict == usingExtDict) && (match < lowPrefix)) {
            if (unlikely(op + length > oend - LASTLITERALS))
                goto _output_error; /* doesn't respect parsing restriction */

            if (length <= (size_t)(lowPrefix - match)) {
                /* match can be copied as a single segment from external dictionary */
                memmove(op, dictEnd - (lowPrefix - match), length);
                op += length;
            }
            else {
                /* match encompass external dictionary and current block */
                size_t const copySize = (size_t)(lowPrefix - match);
                size_t const restSize = length - copySize;
                Utils::copy(op, dictEnd - copySize, copySize);
                op += copySize;
                if (restSize > (size_t)(op - lowPrefix)) { /* overlap copy */
                    BYTE *const endOfMatch = op + restSize;
                    const BYTE *copyFrom   = lowPrefix;
                    while (op < endOfMatch)
                        *op++ = *copyFrom++;
                }
                else {
                    Utils::copy(op, lowPrefix, restSize);
                    op += restSize;
                }
            }
            continue;
        }

        /* copy match within block */
        cpy = op + length;
        if (unlikely(offset < 8)) {
            const int dec64 = dec64table[offset];
            op[0]           = match[0];
            op[1]           = match[1];
            op[2]           = match[2];
            op[3]           = match[3];
            match += dec32table[offset];
            Utils::copy<4>(op + 4, match);
            match -= dec64;
        }
        else {
            LZ4_copy8(op, match);
            match += 8;
        }
        op += 8;

        if (unlikely(cpy > oend - 12)) {
            BYTE *const oCopyLimit = oend - (WILDCOPYLENGTH - 1);
            if (cpy > oend - LASTLITERALS)
                goto _output_error; /* Error : last LASTLITERALS bytes must be literals (uncompressed) */
            if (op < oCopyLimit) {
                LZ4_wildCopy(op, match, oCopyLimit);
                match += oCopyLimit - op;
                op = oCopyLimit;
            }
            while (op < cpy)
                *op++ = *match++;
        }
        else {
            LZ4_copy8(op, match);
            if (length > 16)
                LZ4_wildCopy(op + 8, match + 8, cpy);
        }
        op = cpy; /* correction */
    }

    /* end of decoding */
    if (endOnInput)
        return (int)(((char *)op) - dest); /* Nb of output bytes decoded */
    else
        return (int)(((const char *)ip) - source); /* Nb of input bytes read */

/* Overflow error detected */
_output_error:
    return (int)(-(((const char *)ip) - source)) - 1;
}

}   // anonymous namespace

int LZ4_compress_fast(const char *source, char *dest, int inputSize, int maxOutputSize, int acceleration) noexcept
{
#if (HEAPMODE)
    void *ctxPtr = ALLOCATOR(1, sizeof(LZ4_stream_t)); /* malloc-calloc always properly aligned */
#else
    LZ4_stream_t ctx;
    void *const ctxPtr = &ctx;
#endif

    int const result = LZ4_compress_fast_extState(ctxPtr, source, dest, inputSize, maxOutputSize, acceleration);

#if (HEAPMODE)
    FREEMEM(ctxPtr);
#endif
    return result;
}

int LZ4_decompress_safe(const char *source, char *dest, int compressedSize, int maxDecompressedSize) noexcept { return LZ4_decompress_generic(source, dest, compressedSize, maxDecompressedSize, endOnInputSize, full, 0, noDict, (BYTE *)dest, NULL, 0); }

}   // namespace ZeroTier
