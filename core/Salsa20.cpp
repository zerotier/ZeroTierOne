/*
 * Based on public domain code available at: http://cr.yp.to/snuffle.html
 *
 * Modifications and C-native SSE macro based SSE implementation by
 * Adam Ierymenko <adam.ierymenko@zerotier.com>.
 *
 * Since the original was public domain, this is too.
 */

#include "Salsa20.hpp"

#include "Constants.hpp"

#define ROTATE(v, c) (((v) << (c)) | ((v) >> (32 - (c))))
#define XOR(v, w)    ((v) ^ (w))
#define PLUS(v, w)   ((uint32_t)((v) + (w)))

#ifndef ZT_SALSA20_SSE
#if __BYTE_ORDER == __LITTLE_ENDIAN
#ifdef ZT_NO_UNALIGNED_ACCESS
// Slower version that does not use type punning
#define U8TO32_LITTLE(p) (((uint32_t)(p)[0]) | ((uint32_t)(p)[1] << 8) | ((uint32_t)(p)[2] << 16) | ((uint32_t)(p)[3] << 24))
static ZT_INLINE void U32TO8_LITTLE(uint8_t* const c, const uint32_t v)
{
    c[0] = (uint8_t)v;
    c[1] = (uint8_t)(v >> 8);
    c[2] = (uint8_t)(v >> 16);
    c[3] = (uint8_t)(v >> 24);
}
#else
// Fast version that just does 32-bit load/store
#define U8TO32_LITTLE(p)    (*((const uint32_t*)((const void*)(p))))
#define U32TO8_LITTLE(c, v) *((uint32_t*)((void*)(c))) = (v)
#endif   // ZT_NO_UNALIGNED_ACCESS
#else    // __BYTE_ORDER == __BIG_ENDIAN (we don't support anything else... does MIDDLE_ENDIAN even still exist?)
#ifdef __GNUC__
// Use GNUC builtin bswap macros on big-endian machines if available
#define U8TO32_LITTLE(p)    __builtin_bswap32(*((const uint32_t*)((const void*)(p))))
#define U32TO8_LITTLE(c, v) *((uint32_t*)((void*)(c))) = __builtin_bswap32((v))
#else   // no __GNUC__
// Otherwise do it the slow, manual way on BE machines
#define U8TO32_LITTLE(p) (((uint32_t)(p)[0]) | ((uint32_t)(p)[1] << 8) | ((uint32_t)(p)[2] << 16) | ((uint32_t)(p)[3] << 24))
static ZT_INLINE void U32TO8_LITTLE(uint8_t* const c, const uint32_t v)
{
    c[0] = (uint8_t)v;
    c[1] = (uint8_t)(v >> 8);
    c[2] = (uint8_t)(v >> 16);
    c[3] = (uint8_t)(v >> 24);
}
#endif   // __GNUC__ or not
#endif   // __BYTE_ORDER little or big?
#endif   // !ZT_SALSA20_SSE

#ifdef ZT_SALSA20_SSE
class _s20sseconsts {
  public:
    _s20sseconsts() noexcept
    {
        maskLo32 = _mm_shuffle_epi32(_mm_cvtsi32_si128(-1), _MM_SHUFFLE(1, 0, 1, 0));
        maskHi32 = _mm_slli_epi64(maskLo32, 32);
    }
    __m128i maskLo32, maskHi32;
};
static const _s20sseconsts s_S20SSECONSTANTS;
#endif

namespace ZeroTier {

void Salsa20::init(const void* key, const void* iv) noexcept
{
#ifdef ZT_SALSA20_SSE
    const uint32_t* const k = (const uint32_t*)key;
    _state.i[0] = 0x61707865;
    _state.i[1] = 0x3320646e;
    _state.i[2] = 0x79622d32;
    _state.i[3] = 0x6b206574;
    _state.i[4] = k[3];
    _state.i[5] = 0;
    _state.i[6] = k[7];
    _state.i[7] = k[2];
    _state.i[8] = 0;
    _state.i[9] = k[6];
    _state.i[10] = k[1];
    _state.i[11] = ((const uint32_t*)iv)[1];
    _state.i[12] = k[5];
    _state.i[13] = k[0];
    _state.i[14] = ((const uint32_t*)iv)[0];
    _state.i[15] = k[4];
#else
    const char* const constants = "expand 32-byte k";
    const uint8_t* const k = (const uint8_t*)key;
    _state.i[0] = U8TO32_LITTLE(constants + 0);
    _state.i[1] = U8TO32_LITTLE(k + 0);
    _state.i[2] = U8TO32_LITTLE(k + 4);
    _state.i[3] = U8TO32_LITTLE(k + 8);
    _state.i[4] = U8TO32_LITTLE(k + 12);
    _state.i[5] = U8TO32_LITTLE(constants + 4);
    _state.i[6] = U8TO32_LITTLE(((const uint8_t*)iv) + 0);
    _state.i[7] = U8TO32_LITTLE(((const uint8_t*)iv) + 4);
    _state.i[8] = 0;
    _state.i[9] = 0;
    _state.i[10] = U8TO32_LITTLE(constants + 8);
    _state.i[11] = U8TO32_LITTLE(k + 16);
    _state.i[12] = U8TO32_LITTLE(k + 20);
    _state.i[13] = U8TO32_LITTLE(k + 24);
    _state.i[14] = U8TO32_LITTLE(k + 28);
    _state.i[15] = U8TO32_LITTLE(constants + 12);
#endif
}

union p_SalsaState {
#ifdef ZT_SALSA20_SSE
    __m128i v[4];
#endif   // ZT_SALSA20_SSE
    uint32_t i[16];
};

template <unsigned int R> static ZT_INLINE void p_salsaCrypt(p_SalsaState* const state, const uint8_t* m, uint8_t* c, unsigned int bytes) noexcept
{
    if (unlikely(bytes == 0))
        return;

    uint8_t tmp[64];
    uint8_t* ctarget = c;

#ifdef ZT_SALSA20_SSE
    __m128i X0 = state->v[0];
    __m128i X1 = state->v[1];
    __m128i X2 = state->v[2];
    __m128i X3 = state->v[3];
    const __m128i maskLo32 = s_S20SSECONSTANTS.maskLo32;
    const __m128i maskHi32 = s_S20SSECONSTANTS.maskHi32;
    const __m128i add1 = _mm_set_epi32(0, 0, 0, 1);
#else
    uint32_t x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15;
    uint32_t j0, j1, j2, j3, j4, j5, j6, j7, j8, j9, j10, j11, j12, j13, j14, j15;
    j0 = state->i[0];
    j1 = state->i[1];
    j2 = state->i[2];
    j3 = state->i[3];
    j4 = state->i[4];
    j5 = state->i[5];
    j6 = state->i[6];
    j7 = state->i[7];
    j8 = state->i[8];
    j9 = state->i[9];
    j10 = state->i[10];
    j11 = state->i[11];
    j12 = state->i[12];
    j13 = state->i[13];
    j14 = state->i[14];
    j15 = state->i[15];
#endif

    for (;;) {
        if (unlikely(bytes < 64)) {
            for (unsigned int i = 0; i < bytes; ++i)
                tmp[i] = m[i];
            m = tmp;
            ctarget = c;
            c = tmp;
        }

#ifdef ZT_SALSA20_SSE
        __m128i X0s = X0;
        __m128i X1s = X1;
        __m128i X2s = X2;
        __m128i X3s = X3;
        __m128i T;

        for (unsigned int rr = 0; rr < (R / 2); ++rr) {
            T = _mm_add_epi32(X0, X3);
            X1 = _mm_xor_si128(_mm_xor_si128(X1, _mm_slli_epi32(T, 7)), _mm_srli_epi32(T, 25));
            T = _mm_add_epi32(X1, X0);
            X2 = _mm_xor_si128(_mm_xor_si128(X2, _mm_slli_epi32(T, 9)), _mm_srli_epi32(T, 23));
            T = _mm_add_epi32(X2, X1);
            X3 = _mm_xor_si128(_mm_xor_si128(X3, _mm_slli_epi32(T, 13)), _mm_srli_epi32(T, 19));
            T = _mm_add_epi32(X3, X2);
            X0 = _mm_xor_si128(_mm_xor_si128(X0, _mm_slli_epi32(T, 18)), _mm_srli_epi32(T, 14));
            X1 = _mm_shuffle_epi32(X1, 0x93);
            X2 = _mm_shuffle_epi32(X2, 0x4E);
            X3 = _mm_shuffle_epi32(X3, 0x39);
            T = _mm_add_epi32(X0, X1);
            X3 = _mm_xor_si128(_mm_xor_si128(X3, _mm_slli_epi32(T, 7)), _mm_srli_epi32(T, 25));
            T = _mm_add_epi32(X3, X0);
            X2 = _mm_xor_si128(_mm_xor_si128(X2, _mm_slli_epi32(T, 9)), _mm_srli_epi32(T, 23));
            T = _mm_add_epi32(X2, X3);
            X1 = _mm_xor_si128(_mm_xor_si128(X1, _mm_slli_epi32(T, 13)), _mm_srli_epi32(T, 19));
            T = _mm_add_epi32(X1, X2);
            X0 = _mm_xor_si128(_mm_xor_si128(X0, _mm_slli_epi32(T, 18)), _mm_srli_epi32(T, 14));
            X1 = _mm_shuffle_epi32(X1, 0x39);
            X2 = _mm_shuffle_epi32(X2, 0x4E);
            X3 = _mm_shuffle_epi32(X3, 0x93);
        }

        X0 = _mm_add_epi32(X0s, X0);
        X1 = _mm_add_epi32(X1s, X1);
        X2 = _mm_add_epi32(X2s, X2);
        X3 = _mm_add_epi32(X3s, X3);

        __m128i k02 = _mm_or_si128(_mm_slli_epi64(X0, 32), _mm_srli_epi64(X3, 32));
        __m128i k20 = _mm_or_si128(_mm_and_si128(X2, maskLo32), _mm_and_si128(X1, maskHi32));
        __m128i k13 = _mm_or_si128(_mm_slli_epi64(X1, 32), _mm_srli_epi64(X0, 32));
        __m128i k31 = _mm_or_si128(_mm_and_si128(X3, maskLo32), _mm_and_si128(X2, maskHi32));
        k02 = _mm_shuffle_epi32(k02, _MM_SHUFFLE(0, 1, 2, 3));
        k13 = _mm_shuffle_epi32(k13, _MM_SHUFFLE(0, 1, 2, 3));

        _mm_storeu_si128(reinterpret_cast<__m128i*>(c), _mm_xor_si128(_mm_unpackhi_epi64(k02, k20), _mm_loadu_si128(reinterpret_cast<const __m128i*>(m))));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(c) + 1, _mm_xor_si128(_mm_unpackhi_epi64(k13, k31), _mm_loadu_si128(reinterpret_cast<const __m128i*>(m) + 1)));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(c) + 2, _mm_xor_si128(_mm_unpacklo_epi64(k20, k02), _mm_loadu_si128(reinterpret_cast<const __m128i*>(m) + 2)));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(c) + 3, _mm_xor_si128(_mm_unpacklo_epi64(k31, k13), _mm_loadu_si128(reinterpret_cast<const __m128i*>(m) + 3)));

        X0 = X0s;
        X1 = X1s;
        X2 = _mm_add_epi32(X2s, add1);
        X3 = X3s;

#else

        x0 = j0;
        x1 = j1;
        x2 = j2;
        x3 = j3;
        x4 = j4;
        x5 = j5;
        x6 = j6;
        x7 = j7;
        x8 = j8;
        x9 = j9;
        x10 = j10;
        x11 = j11;
        x12 = j12;
        x13 = j13;
        x14 = j14;
        x15 = j15;

        for (unsigned int rr = 0; rr < (R / 2); ++rr) {
            x4 = XOR(x4, ROTATE(PLUS(x0, x12), 7));
            x8 = XOR(x8, ROTATE(PLUS(x4, x0), 9));
            x12 = XOR(x12, ROTATE(PLUS(x8, x4), 13));
            x0 = XOR(x0, ROTATE(PLUS(x12, x8), 18));
            x9 = XOR(x9, ROTATE(PLUS(x5, x1), 7));
            x13 = XOR(x13, ROTATE(PLUS(x9, x5), 9));
            x1 = XOR(x1, ROTATE(PLUS(x13, x9), 13));
            x5 = XOR(x5, ROTATE(PLUS(x1, x13), 18));
            x14 = XOR(x14, ROTATE(PLUS(x10, x6), 7));
            x2 = XOR(x2, ROTATE(PLUS(x14, x10), 9));
            x6 = XOR(x6, ROTATE(PLUS(x2, x14), 13));
            x10 = XOR(x10, ROTATE(PLUS(x6, x2), 18));
            x3 = XOR(x3, ROTATE(PLUS(x15, x11), 7));
            x7 = XOR(x7, ROTATE(PLUS(x3, x15), 9));
            x11 = XOR(x11, ROTATE(PLUS(x7, x3), 13));
            x15 = XOR(x15, ROTATE(PLUS(x11, x7), 18));
            x1 = XOR(x1, ROTATE(PLUS(x0, x3), 7));
            x2 = XOR(x2, ROTATE(PLUS(x1, x0), 9));
            x3 = XOR(x3, ROTATE(PLUS(x2, x1), 13));
            x0 = XOR(x0, ROTATE(PLUS(x3, x2), 18));
            x6 = XOR(x6, ROTATE(PLUS(x5, x4), 7));
            x7 = XOR(x7, ROTATE(PLUS(x6, x5), 9));
            x4 = XOR(x4, ROTATE(PLUS(x7, x6), 13));
            x5 = XOR(x5, ROTATE(PLUS(x4, x7), 18));
            x11 = XOR(x11, ROTATE(PLUS(x10, x9), 7));
            x8 = XOR(x8, ROTATE(PLUS(x11, x10), 9));
            x9 = XOR(x9, ROTATE(PLUS(x8, x11), 13));
            x10 = XOR(x10, ROTATE(PLUS(x9, x8), 18));
            x12 = XOR(x12, ROTATE(PLUS(x15, x14), 7));
            x13 = XOR(x13, ROTATE(PLUS(x12, x15), 9));
            x14 = XOR(x14, ROTATE(PLUS(x13, x12), 13));
            x15 = XOR(x15, ROTATE(PLUS(x14, x13), 18));
        }

        x0 = PLUS(x0, j0);
        x1 = PLUS(x1, j1);
        x2 = PLUS(x2, j2);
        x3 = PLUS(x3, j3);
        x4 = PLUS(x4, j4);
        x5 = PLUS(x5, j5);
        x6 = PLUS(x6, j6);
        x7 = PLUS(x7, j7);
        x8 = PLUS(x8, j8);
        x9 = PLUS(x9, j9);
        x10 = PLUS(x10, j10);
        x11 = PLUS(x11, j11);
        x12 = PLUS(x12, j12);
        x13 = PLUS(x13, j13);
        x14 = PLUS(x14, j14);
        x15 = PLUS(x15, j15);

        U32TO8_LITTLE(c + 0, XOR(x0, U8TO32_LITTLE(m + 0)));
        U32TO8_LITTLE(c + 4, XOR(x1, U8TO32_LITTLE(m + 4)));
        U32TO8_LITTLE(c + 8, XOR(x2, U8TO32_LITTLE(m + 8)));
        U32TO8_LITTLE(c + 12, XOR(x3, U8TO32_LITTLE(m + 12)));
        U32TO8_LITTLE(c + 16, XOR(x4, U8TO32_LITTLE(m + 16)));
        U32TO8_LITTLE(c + 20, XOR(x5, U8TO32_LITTLE(m + 20)));
        U32TO8_LITTLE(c + 24, XOR(x6, U8TO32_LITTLE(m + 24)));
        U32TO8_LITTLE(c + 28, XOR(x7, U8TO32_LITTLE(m + 28)));
        U32TO8_LITTLE(c + 32, XOR(x8, U8TO32_LITTLE(m + 32)));
        U32TO8_LITTLE(c + 36, XOR(x9, U8TO32_LITTLE(m + 36)));
        U32TO8_LITTLE(c + 40, XOR(x10, U8TO32_LITTLE(m + 40)));
        U32TO8_LITTLE(c + 44, XOR(x11, U8TO32_LITTLE(m + 44)));
        U32TO8_LITTLE(c + 48, XOR(x12, U8TO32_LITTLE(m + 48)));
        U32TO8_LITTLE(c + 52, XOR(x13, U8TO32_LITTLE(m + 52)));
        U32TO8_LITTLE(c + 56, XOR(x14, U8TO32_LITTLE(m + 56)));
        U32TO8_LITTLE(c + 60, XOR(x15, U8TO32_LITTLE(m + 60)));

        ++j8;

#endif

        if (likely(bytes > 64)) {
            bytes -= 64;
            c += 64;
            m += 64;
        }
        else {
            if (bytes < 64) {
                for (unsigned int i = 0; i < bytes; ++i)
                    ctarget[i] = c[i];
            }
#ifdef ZT_SALSA20_SSE
            state->v[2] = X2;
#else
            state->i[8] = j8;
#endif
            return;
        }
    }
}

void Salsa20::crypt12(const void* in, void* out, unsigned int bytes) noexcept
{
    p_salsaCrypt<12>(reinterpret_cast<p_SalsaState*>(&_state), reinterpret_cast<const uint8_t*>(in), reinterpret_cast<uint8_t*>(out), bytes);
}

void Salsa20::crypt20(const void* in, void* out, unsigned int bytes) noexcept
{
    p_salsaCrypt<20>(reinterpret_cast<p_SalsaState*>(&_state), reinterpret_cast<const uint8_t*>(in), reinterpret_cast<uint8_t*>(out), bytes);
}

}   // namespace ZeroTier
