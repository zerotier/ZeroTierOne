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

// AES for ARM crypto extensions and NEON.

#include "AES.hpp"
#include "Constants.hpp"

#ifdef ZT_AES_NEON

namespace ZeroTier {

namespace {

ZT_INLINE uint8x16_t s_clmul_armneon_crypto(uint8x16_t h, uint8x16_t y, const uint8_t b[16]) noexcept
{
    uint8x16_t r0, r1, t0, t1;
    r0 = vld1q_u8(b);
    const uint8x16_t z = veorq_u8(h, h);
    y = veorq_u8(r0, y);
    y = vrbitq_u8(y);
    const uint8x16_t p = vreinterpretq_u8_u64(vdupq_n_u64(0x0000000000000087));
    t0 = vextq_u8(y, y, 8);
    __asm__ __volatile__("pmull     %0.1q, %1.1d, %2.1d \n\t" : "=w"(r0) : "w"(h), "w"(y));
    __asm__ __volatile__("pmull2   %0.1q, %1.2d, %2.2d \n\t" : "=w"(r1) : "w"(h), "w"(y));
    __asm__ __volatile__("pmull     %0.1q, %1.1d, %2.1d \n\t" : "=w"(t1) : "w"(h), "w"(t0));
    __asm__ __volatile__("pmull2   %0.1q, %1.2d, %2.2d \n\t" : "=w"(t0) : "w"(h), "w"(t0));
    t0 = veorq_u8(t0, t1);
    t1 = vextq_u8(z, t0, 8);
    r0 = veorq_u8(r0, t1);
    t1 = vextq_u8(t0, z, 8);
    r1 = veorq_u8(r1, t1);
    __asm__ __volatile__("pmull2   %0.1q, %1.2d, %2.2d \n\t" : "=w"(t0) : "w"(r1), "w"(p));
    t1 = vextq_u8(t0, z, 8);
    r1 = veorq_u8(r1, t1);
    t1 = vextq_u8(z, t0, 8);
    r0 = veorq_u8(r0, t1);
    __asm__ __volatile__("pmull     %0.1q, %1.1d, %2.1d \n\t" : "=w"(t0) : "w"(r1), "w"(p));
    return vrbitq_u8(veorq_u8(r0, t0));
}

}   // anonymous namespace

void AES::GMAC::p_armUpdate(const uint8_t* in, unsigned int len) noexcept
{
    uint8x16_t y = vld1q_u8(reinterpret_cast<const uint8_t*>(_y));
    const uint8x16_t h = _aes.p_k.neon.h;

    if (_rp) {
        for (;;) {
            if (! len)
                return;
            --len;
            _r[_rp++] = *(in++);
            if (_rp == 16) {
                y = s_clmul_armneon_crypto(h, y, _r);
                break;
            }
        }
    }

    while (len >= 16) {
        y = s_clmul_armneon_crypto(h, y, in);
        in += 16;
        len -= 16;
    }

    vst1q_u8(reinterpret_cast<uint8_t*>(_y), y);

    for (unsigned int i = 0; i < len; ++i)
        _r[i] = in[i];
    _rp = len;   // len is always less than 16 here
}

void AES::GMAC::p_armFinish(uint8_t tag[16]) noexcept
{
    uint64_t tmp[2];
    uint8x16_t y = vld1q_u8(reinterpret_cast<const uint8_t*>(_y));
    const uint8x16_t h = _aes.p_k.neon.h;

    if (_rp) {
        while (_rp < 16)
            _r[_rp++] = 0;
        y = s_clmul_armneon_crypto(h, y, _r);
    }

    tmp[0] = Utils::hton((uint64_t)_len << 3U);
    tmp[1] = 0;
    y = s_clmul_armneon_crypto(h, y, reinterpret_cast<const uint8_t*>(tmp));

    Utils::copy<12>(tmp, _iv);
#if __BYTE_ORDER == __BIG_ENDIAN
    reinterpret_cast<uint32_t*>(tmp)[3] = 0x00000001;
#else
    reinterpret_cast<uint32_t*>(tmp)[3] = 0x01000000;
#endif
    _aes.encrypt(tmp, tmp);

    uint8x16_t yy = y;
    Utils::storeMachineEndian<uint64_t>(tag, tmp[0] ^ reinterpret_cast<const uint64_t*>(&yy)[0]);
    Utils::storeMachineEndian<uint64_t>(tag + 8, tmp[1] ^ reinterpret_cast<const uint64_t*>(&yy)[1]);
}

void AES::CTR::p_armCrypt(const uint8_t* in, uint8_t* out, unsigned int len) noexcept
{
    uint8x16_t dd = vrev32q_u8(vld1q_u8(reinterpret_cast<uint8_t*>(_ctr)));
    const uint32x4_t one = { 0, 0, 0, 1 };

    uint8x16_t k0 = _aes.p_k.neon.ek[0];
    uint8x16_t k1 = _aes.p_k.neon.ek[1];
    uint8x16_t k2 = _aes.p_k.neon.ek[2];
    uint8x16_t k3 = _aes.p_k.neon.ek[3];
    uint8x16_t k4 = _aes.p_k.neon.ek[4];
    uint8x16_t k5 = _aes.p_k.neon.ek[5];
    uint8x16_t k6 = _aes.p_k.neon.ek[6];
    uint8x16_t k7 = _aes.p_k.neon.ek[7];
    uint8x16_t k8 = _aes.p_k.neon.ek[8];
    uint8x16_t k9 = _aes.p_k.neon.ek[9];
    uint8x16_t k10 = _aes.p_k.neon.ek[10];
    uint8x16_t k11 = _aes.p_k.neon.ek[11];
    uint8x16_t k12 = _aes.p_k.neon.ek[12];
    uint8x16_t k13 = _aes.p_k.neon.ek[13];
    uint8x16_t k14 = _aes.p_k.neon.ek[14];

    unsigned int totalLen = _len;
    if ((totalLen & 15U) != 0) {
        for (;;) {
            if (unlikely(! len)) {
                vst1q_u8(reinterpret_cast<uint8_t*>(_ctr), vrev32q_u8(dd));
                _len = totalLen;
                return;
            }
            --len;
            out[totalLen++] = *(in++);
            if ((totalLen & 15U) == 0) {
                uint8_t* const otmp = out + (totalLen - 16);
                uint8x16_t d0 = vrev32q_u8(dd);
                uint8x16_t pt = vld1q_u8(otmp);
                d0 = vaesmcq_u8(vaeseq_u8(d0, k0));
                d0 = vaesmcq_u8(vaeseq_u8(d0, k1));
                d0 = vaesmcq_u8(vaeseq_u8(d0, k2));
                d0 = vaesmcq_u8(vaeseq_u8(d0, k3));
                d0 = vaesmcq_u8(vaeseq_u8(d0, k4));
                d0 = vaesmcq_u8(vaeseq_u8(d0, k5));
                d0 = vaesmcq_u8(vaeseq_u8(d0, k6));
                d0 = vaesmcq_u8(vaeseq_u8(d0, k7));
                d0 = vaesmcq_u8(vaeseq_u8(d0, k8));
                d0 = vaesmcq_u8(vaeseq_u8(d0, k9));
                d0 = vaesmcq_u8(vaeseq_u8(d0, k10));
                d0 = vaesmcq_u8(vaeseq_u8(d0, k11));
                d0 = vaesmcq_u8(vaeseq_u8(d0, k12));
                d0 = veorq_u8(vaeseq_u8(d0, k13), k14);
                vst1q_u8(otmp, veorq_u8(pt, d0));
                dd = (uint8x16_t)vaddq_u32((uint32x4_t)dd, one);
                break;
            }
        }
    }

    out += totalLen;
    _len = totalLen + len;

    if (likely(len >= 64)) {
        const uint32x4_t four = vshlq_n_u32(one, 2);
        uint8x16_t dd1 = (uint8x16_t)vaddq_u32((uint32x4_t)dd, one);
        uint8x16_t dd2 = (uint8x16_t)vaddq_u32((uint32x4_t)dd1, one);
        uint8x16_t dd3 = (uint8x16_t)vaddq_u32((uint32x4_t)dd2, one);
        for (;;) {
            len -= 64;
            uint8x16_t d0 = vrev32q_u8(dd);
            uint8x16_t d1 = vrev32q_u8(dd1);
            uint8x16_t d2 = vrev32q_u8(dd2);
            uint8x16_t d3 = vrev32q_u8(dd3);
            uint8x16_t pt0 = vld1q_u8(in);
            uint8x16_t pt1 = vld1q_u8(in + 16);
            uint8x16_t pt2 = vld1q_u8(in + 32);
            uint8x16_t pt3 = vld1q_u8(in + 48);

            d0 = vaesmcq_u8(vaeseq_u8(d0, k0));
            d1 = vaesmcq_u8(vaeseq_u8(d1, k0));
            d2 = vaesmcq_u8(vaeseq_u8(d2, k0));
            d3 = vaesmcq_u8(vaeseq_u8(d3, k0));
            d0 = vaesmcq_u8(vaeseq_u8(d0, k1));
            d1 = vaesmcq_u8(vaeseq_u8(d1, k1));
            d2 = vaesmcq_u8(vaeseq_u8(d2, k1));
            d3 = vaesmcq_u8(vaeseq_u8(d3, k1));
            d0 = vaesmcq_u8(vaeseq_u8(d0, k2));
            d1 = vaesmcq_u8(vaeseq_u8(d1, k2));
            d2 = vaesmcq_u8(vaeseq_u8(d2, k2));
            d3 = vaesmcq_u8(vaeseq_u8(d3, k2));
            d0 = vaesmcq_u8(vaeseq_u8(d0, k3));
            d1 = vaesmcq_u8(vaeseq_u8(d1, k3));
            d2 = vaesmcq_u8(vaeseq_u8(d2, k3));
            d3 = vaesmcq_u8(vaeseq_u8(d3, k3));
            d0 = vaesmcq_u8(vaeseq_u8(d0, k4));
            d1 = vaesmcq_u8(vaeseq_u8(d1, k4));
            d2 = vaesmcq_u8(vaeseq_u8(d2, k4));
            d3 = vaesmcq_u8(vaeseq_u8(d3, k4));
            d0 = vaesmcq_u8(vaeseq_u8(d0, k5));
            d1 = vaesmcq_u8(vaeseq_u8(d1, k5));
            d2 = vaesmcq_u8(vaeseq_u8(d2, k5));
            d3 = vaesmcq_u8(vaeseq_u8(d3, k5));
            d0 = vaesmcq_u8(vaeseq_u8(d0, k6));
            d1 = vaesmcq_u8(vaeseq_u8(d1, k6));
            d2 = vaesmcq_u8(vaeseq_u8(d2, k6));
            d3 = vaesmcq_u8(vaeseq_u8(d3, k6));
            d0 = vaesmcq_u8(vaeseq_u8(d0, k7));
            d1 = vaesmcq_u8(vaeseq_u8(d1, k7));
            d2 = vaesmcq_u8(vaeseq_u8(d2, k7));
            d3 = vaesmcq_u8(vaeseq_u8(d3, k7));
            d0 = vaesmcq_u8(vaeseq_u8(d0, k8));
            d1 = vaesmcq_u8(vaeseq_u8(d1, k8));
            d2 = vaesmcq_u8(vaeseq_u8(d2, k8));
            d3 = vaesmcq_u8(vaeseq_u8(d3, k8));
            d0 = vaesmcq_u8(vaeseq_u8(d0, k9));
            d1 = vaesmcq_u8(vaeseq_u8(d1, k9));
            d2 = vaesmcq_u8(vaeseq_u8(d2, k9));
            d3 = vaesmcq_u8(vaeseq_u8(d3, k9));
            d0 = vaesmcq_u8(vaeseq_u8(d0, k10));
            d1 = vaesmcq_u8(vaeseq_u8(d1, k10));
            d2 = vaesmcq_u8(vaeseq_u8(d2, k10));
            d3 = vaesmcq_u8(vaeseq_u8(d3, k10));
            d0 = vaesmcq_u8(vaeseq_u8(d0, k11));
            d1 = vaesmcq_u8(vaeseq_u8(d1, k11));
            d2 = vaesmcq_u8(vaeseq_u8(d2, k11));
            d3 = vaesmcq_u8(vaeseq_u8(d3, k11));
            d0 = vaesmcq_u8(vaeseq_u8(d0, k12));
            d1 = vaesmcq_u8(vaeseq_u8(d1, k12));
            d2 = vaesmcq_u8(vaeseq_u8(d2, k12));
            d3 = vaesmcq_u8(vaeseq_u8(d3, k12));
            d0 = veorq_u8(vaeseq_u8(d0, k13), k14);
            d1 = veorq_u8(vaeseq_u8(d1, k13), k14);
            d2 = veorq_u8(vaeseq_u8(d2, k13), k14);
            d3 = veorq_u8(vaeseq_u8(d3, k13), k14);

            d0 = veorq_u8(pt0, d0);
            d1 = veorq_u8(pt1, d1);
            d2 = veorq_u8(pt2, d2);
            d3 = veorq_u8(pt3, d3);

            vst1q_u8(out, d0);
            vst1q_u8(out + 16, d1);
            vst1q_u8(out + 32, d2);
            vst1q_u8(out + 48, d3);

            out += 64;
            in += 64;

            dd = (uint8x16_t)vaddq_u32((uint32x4_t)dd, four);
            if (unlikely(len < 64))
                break;
            dd1 = (uint8x16_t)vaddq_u32((uint32x4_t)dd1, four);
            dd2 = (uint8x16_t)vaddq_u32((uint32x4_t)dd2, four);
            dd3 = (uint8x16_t)vaddq_u32((uint32x4_t)dd3, four);
        }
    }

    while (len >= 16) {
        len -= 16;
        uint8x16_t d0 = vrev32q_u8(dd);
        uint8x16_t pt = vld1q_u8(in);
        in += 16;
        dd = (uint8x16_t)vaddq_u32((uint32x4_t)dd, one);
        d0 = vaesmcq_u8(vaeseq_u8(d0, k0));
        d0 = vaesmcq_u8(vaeseq_u8(d0, k1));
        d0 = vaesmcq_u8(vaeseq_u8(d0, k2));
        d0 = vaesmcq_u8(vaeseq_u8(d0, k3));
        d0 = vaesmcq_u8(vaeseq_u8(d0, k4));
        d0 = vaesmcq_u8(vaeseq_u8(d0, k5));
        d0 = vaesmcq_u8(vaeseq_u8(d0, k6));
        d0 = vaesmcq_u8(vaeseq_u8(d0, k7));
        d0 = vaesmcq_u8(vaeseq_u8(d0, k8));
        d0 = vaesmcq_u8(vaeseq_u8(d0, k9));
        d0 = vaesmcq_u8(vaeseq_u8(d0, k10));
        d0 = vaesmcq_u8(vaeseq_u8(d0, k11));
        d0 = vaesmcq_u8(vaeseq_u8(d0, k12));
        d0 = veorq_u8(vaeseq_u8(d0, k13), k14);
        vst1q_u8(out, veorq_u8(pt, d0));
        out += 16;
    }

    // Any remaining input is placed in _out. This will be picked up and crypted
    // on subsequent calls to crypt() or finish() as it'll mean _len will not be
    // an even multiple of 16.
    for (unsigned int i = 0; i < len; ++i)
        out[i] = in[i];

    vst1q_u8(reinterpret_cast<uint8_t*>(_ctr), vrev32q_u8(dd));
}

#define ZT_INIT_ARMNEON_CRYPTO_SUBWORD(w) ((uint32_t)s_sbox[w & 0xffU] + ((uint32_t)s_sbox[(w >> 8U) & 0xffU] << 8U) + ((uint32_t)s_sbox[(w >> 16U) & 0xffU] << 16U) + ((uint32_t)s_sbox[(w >> 24U) & 0xffU] << 24U))
#define ZT_INIT_ARMNEON_CRYPTO_ROTWORD(w) (((w) << 8U) | ((w) >> 24U))
#define ZT_INIT_ARMNEON_CRYPTO_NK         8
#define ZT_INIT_ARMNEON_CRYPTO_NB         4
#define ZT_INIT_ARMNEON_CRYPTO_NR         14

void AES::p_init_armneon_crypto(const uint8_t* key) noexcept
{
    static const uint8_t s_sbox[256] = { 0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76, 0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
                                         0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15, 0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
                                         0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84, 0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
                                         0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8, 0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
                                         0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73, 0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
                                         0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79, 0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
                                         0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a, 0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
                                         0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf, 0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16 };

    uint64_t h[2];
    uint32_t* const w = reinterpret_cast<uint32_t*>(p_k.neon.ek);

    for (unsigned int i = 0; i < ZT_INIT_ARMNEON_CRYPTO_NK; ++i) {
        const unsigned int j = i * 4;
        w[i] = ((uint32_t)key[j] << 24U) | ((uint32_t)key[j + 1] << 16U) | ((uint32_t)key[j + 2] << 8U) | (uint32_t)key[j + 3];
    }

    for (unsigned int i = ZT_INIT_ARMNEON_CRYPTO_NK; i < (ZT_INIT_ARMNEON_CRYPTO_NB * (ZT_INIT_ARMNEON_CRYPTO_NR + 1)); ++i) {
        uint32_t t = w[i - 1];
        const unsigned int imod = i & (ZT_INIT_ARMNEON_CRYPTO_NK - 1);
        if (imod == 0) {
            t = ZT_INIT_ARMNEON_CRYPTO_SUBWORD(ZT_INIT_ARMNEON_CRYPTO_ROTWORD(t)) ^ rcon[(i - 1) / ZT_INIT_ARMNEON_CRYPTO_NK];
        }
        else if (imod == 4) {
            t = ZT_INIT_ARMNEON_CRYPTO_SUBWORD(t);
        }
        w[i] = w[i - ZT_INIT_ARMNEON_CRYPTO_NK] ^ t;
    }

    for (unsigned int i = 0; i < (ZT_INIT_ARMNEON_CRYPTO_NB * (ZT_INIT_ARMNEON_CRYPTO_NR + 1)); ++i)
        w[i] = Utils::hton(w[i]);

    p_k.neon.dk[0] = p_k.neon.ek[14];
    for (int i = 1; i < 14; ++i)
        p_k.neon.dk[i] = vaesimcq_u8(p_k.neon.ek[14 - i]);
    p_k.neon.dk[14] = p_k.neon.ek[0];

    p_encrypt_armneon_crypto(Utils::ZERO256, h);
    Utils::copy<16>(&(p_k.neon.h), h);
    p_k.neon.h = vrbitq_u8(p_k.neon.h);
    p_k.sw.h[0] = Utils::ntoh(h[0]);
    p_k.sw.h[1] = Utils::ntoh(h[1]);
}

void AES::p_encrypt_armneon_crypto(const void* const in, void* const out) const noexcept
{
    uint8x16_t tmp = vld1q_u8(reinterpret_cast<const uint8_t*>(in));
    tmp = vaesmcq_u8(vaeseq_u8(tmp, p_k.neon.ek[0]));
    tmp = vaesmcq_u8(vaeseq_u8(tmp, p_k.neon.ek[1]));
    tmp = vaesmcq_u8(vaeseq_u8(tmp, p_k.neon.ek[2]));
    tmp = vaesmcq_u8(vaeseq_u8(tmp, p_k.neon.ek[3]));
    tmp = vaesmcq_u8(vaeseq_u8(tmp, p_k.neon.ek[4]));
    tmp = vaesmcq_u8(vaeseq_u8(tmp, p_k.neon.ek[5]));
    tmp = vaesmcq_u8(vaeseq_u8(tmp, p_k.neon.ek[6]));
    tmp = vaesmcq_u8(vaeseq_u8(tmp, p_k.neon.ek[7]));
    tmp = vaesmcq_u8(vaeseq_u8(tmp, p_k.neon.ek[8]));
    tmp = vaesmcq_u8(vaeseq_u8(tmp, p_k.neon.ek[9]));
    tmp = vaesmcq_u8(vaeseq_u8(tmp, p_k.neon.ek[10]));
    tmp = vaesmcq_u8(vaeseq_u8(tmp, p_k.neon.ek[11]));
    tmp = vaesmcq_u8(vaeseq_u8(tmp, p_k.neon.ek[12]));
    tmp = veorq_u8(vaeseq_u8(tmp, p_k.neon.ek[13]), p_k.neon.ek[14]);
    vst1q_u8(reinterpret_cast<uint8_t*>(out), tmp);
}

void AES::p_decrypt_armneon_crypto(const void* const in, void* const out) const noexcept
{
    uint8x16_t tmp = vld1q_u8(reinterpret_cast<const uint8_t*>(in));
    tmp = vaesimcq_u8(vaesdq_u8(tmp, p_k.neon.dk[0]));
    tmp = vaesimcq_u8(vaesdq_u8(tmp, p_k.neon.dk[1]));
    tmp = vaesimcq_u8(vaesdq_u8(tmp, p_k.neon.dk[2]));
    tmp = vaesimcq_u8(vaesdq_u8(tmp, p_k.neon.dk[3]));
    tmp = vaesimcq_u8(vaesdq_u8(tmp, p_k.neon.dk[4]));
    tmp = vaesimcq_u8(vaesdq_u8(tmp, p_k.neon.dk[5]));
    tmp = vaesimcq_u8(vaesdq_u8(tmp, p_k.neon.dk[6]));
    tmp = vaesimcq_u8(vaesdq_u8(tmp, p_k.neon.dk[7]));
    tmp = vaesimcq_u8(vaesdq_u8(tmp, p_k.neon.dk[8]));
    tmp = vaesimcq_u8(vaesdq_u8(tmp, p_k.neon.dk[9]));
    tmp = vaesimcq_u8(vaesdq_u8(tmp, p_k.neon.dk[10]));
    tmp = vaesimcq_u8(vaesdq_u8(tmp, p_k.neon.dk[11]));
    tmp = vaesimcq_u8(vaesdq_u8(tmp, p_k.neon.dk[12]));
    tmp = veorq_u8(vaesdq_u8(tmp, p_k.neon.dk[13]), p_k.neon.dk[14]);
    vst1q_u8(reinterpret_cast<uint8_t*>(out), tmp);
}

}   // namespace ZeroTier

#endif   // ZT_AES_NEON
