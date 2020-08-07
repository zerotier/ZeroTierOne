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

#include "Constants.hpp"
#include "AES.hpp"

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif

#define Te1_r(x) ZT_ROR32(Te0[x], 8)
#define Te2_r(x) ZT_ROR32(Te0[x], 16)
#define Te3_r(x) ZT_ROR32(Te0[x], 24)
#define Td1_r(x) ZT_ROR32(Td0[x], 8)
#define Td2_r(x) ZT_ROR32(Td0[x], 16)
#define Td3_r(x) ZT_ROR32(Td0[x], 24)

namespace ZeroTier {

// GMAC ---------------------------------------------------------------------------------------------------------------

namespace {

#ifdef ZT_AES_NEON

ZT_INLINE uint8x16_t s_clmul_armneon_crypto(uint8x16_t h, uint8x16_t y, const uint8_t b[16]) noexcept
{
	uint8x16_t r0, r1, t0, t1;
	r0 = vld1q_u8(b);
	const uint8x16_t z = veorq_u8(h, h);
	y = veorq_u8(r0, y);
	y = vrbitq_u8(y);
	const uint8x16_t p = vreinterpretq_u8_u64(vdupq_n_u64(0x0000000000000087));
	t0 = vextq_u8(y, y, 8);
	__asm__ __volatile__("pmull     %0.1q, %1.1d, %2.1d \n\t" : "=w" (r0) : "w" (h), "w" (y));
	__asm__ __volatile__("pmull2   %0.1q, %1.2d, %2.2d \n\t" :"=w" (r1) : "w" (h), "w" (y));
	__asm__ __volatile__("pmull     %0.1q, %1.1d, %2.1d \n\t" : "=w" (t1) : "w" (h), "w" (t0));
	__asm__ __volatile__("pmull2   %0.1q, %1.2d, %2.2d \n\t" :"=w" (t0) : "w" (h), "w" (t0));
	t0 = veorq_u8(t0, t1);
	t1 = vextq_u8(z, t0, 8);
	r0 = veorq_u8(r0, t1);
	t1 = vextq_u8(t0, z, 8);
	r1 = veorq_u8(r1, t1);
	__asm__ __volatile__("pmull2   %0.1q, %1.2d, %2.2d \n\t" :"=w" (t0) : "w" (r1), "w" (p));
	t1 = vextq_u8(t0, z, 8);
	r1 = veorq_u8(r1, t1);
	t1 = vextq_u8(z, t0, 8);
	r0 = veorq_u8(r0, t1);
	__asm__ __volatile__("pmull     %0.1q, %1.1d, %2.1d \n\t" : "=w" (t0) : "w" (r1), "w" (p));
	return vrbitq_u8(veorq_u8(r0, t0));
}

#endif // ZT_AES_NEON

ZT_INLINE void s_bmul32(const uint32_t x, const uint32_t y, uint32_t &rh, uint32_t &rl) noexcept
{
	uint32_t x0 = x & 0x11111111;
	uint32_t x1 = x & 0x22222222;
	uint32_t x2 = x & 0x44444444;
	uint32_t x3 = x & 0x88888888;
	uint32_t y0 = y & 0x11111111;
	uint32_t y1 = y & 0x22222222;
	uint32_t y2 = y & 0x44444444;
	uint32_t y3 = y & 0x88888888;
	uint64_t z0 = (((uint64_t)x0 * y0) ^ ((uint64_t)x1 * y3) ^ ((uint64_t)x2 * y2) ^ ((uint64_t)x3 * y1)) & 0x1111111111111111ULL;
	uint64_t z1 = (((uint64_t)x0 * y1) ^ ((uint64_t)x1 * y0) ^ ((uint64_t)x2 * y3) ^ ((uint64_t)x3 * y2)) & 0x2222222222222222ULL;
	z0 |= z1;
	uint64_t z2 = (((uint64_t)x0 * y2) ^ ((uint64_t)x1 * y1) ^ ((uint64_t)x2 * y0) ^ ((uint64_t)x3 * y3)) & 0x4444444444444444ULL;
	z2 |= z0;
	uint64_t z3 = (((uint64_t)x0 * y3) ^ ((uint64_t)x1 * y2) ^ ((uint64_t)x2 * y1) ^ ((uint64_t)x3 * y0)) & 0x8888888888888888ULL;
	uint64_t z = z2 | z3;
	rh = (uint32_t)(z >> 32U);
	rl = (uint32_t)z;
}

void s_gfmul(const uint64_t hh,const uint64_t hl,uint64_t &y0,uint64_t &y1) noexcept
{
	uint32_t hhh = (uint32_t)(hh >> 32U);
	uint32_t hhl = (uint32_t)hh;
	uint32_t hlh = (uint32_t)(hl >> 32U);
	uint32_t hll = (uint32_t)hl;
	uint32_t hhXlh = hhh ^ hlh;
	uint32_t hhXll = hhl ^ hll;
	uint64_t yl = Utils::ntoh(y0);
	uint64_t yh = Utils::ntoh(y1);
	uint32_t cilh = (uint32_t)(yh >> 32U);
	uint32_t cill = (uint32_t)yh;
	uint32_t cihh = (uint32_t)(yl >> 32U);
	uint32_t cihl = (uint32_t)yl;
	uint32_t cihXlh = cihh ^ cilh;
	uint32_t cihXll = cihl ^ cill;
	uint32_t aah,aal,abh,abl,ach,acl;
	s_bmul32(cihh,hhh,aah,aal);
	s_bmul32(cihl,hhl,abh,abl);
	s_bmul32(cihh ^ cihl,hhh ^ hhl,ach,acl);
	ach ^= aah ^ abh;
	acl ^= aal ^ abl;
	aal ^= ach;
	abh ^= acl;
	uint32_t bah,bal,bbh,bbl,bch,bcl;
	s_bmul32(cilh,hlh,bah,bal);
	s_bmul32(cill,hll,bbh,bbl);
	s_bmul32(cilh ^ cill,hlh ^ hll,bch,bcl);
	bch ^= bah ^ bbh;
	bcl ^= bal ^ bbl;
	bal ^= bch;
	bbh ^= bcl;
	uint32_t cah,cal,cbh,cbl,cch,ccl;
	s_bmul32(cihXlh,hhXlh,cah,cal);
	s_bmul32(cihXll,hhXll,cbh,cbl);
	s_bmul32(cihXlh ^ cihXll, hhXlh ^ hhXll,cch,ccl);
	cch ^= cah ^ cbh;
	ccl ^= cal ^ cbl;
	cal ^= cch;
	cbh ^= ccl;
	cah ^= bah ^ aah;
	cal ^= bal ^ aal;
	cbh ^= bbh ^ abh;
	cbl ^= bbl ^ abl;
	uint64_t zhh = ((uint64_t)aah << 32U) | aal;
	uint64_t zhl = (((uint64_t)abh << 32U) | abl) ^ (((uint64_t)cah << 32U) | cal);
	uint64_t zlh = (((uint64_t)bah << 32U) | bal) ^ (((uint64_t)cbh << 32U) | cbl);
	uint64_t zll = ((uint64_t)bbh << 32U) | bbl;
	zhh = zhh << 1U | zhl >> 63U;
	zhl = zhl << 1U | zlh >> 63U;
	zlh = zlh << 1U | zll >> 63U;
	zll <<= 1;
	zlh ^= (zll << 63U) ^ (zll << 62U) ^ (zll << 57U);
	zhh ^= zlh ^ (zlh >> 1U) ^ (zlh >> 2U) ^ (zlh >> 7U);
	zhl ^= zll ^ (zll >> 1U) ^ (zll >> 2U) ^ (zll >> 7U) ^ (zlh << 63U) ^ (zlh << 62U) ^ (zlh << 57U);
	y0 = Utils::hton(zhh);
	y1 = Utils::hton(zhl);
}

} // anonymous namespace

#ifdef ZT_AES_AESNI

// SSE shuffle parameter to reverse bytes in a 128-bit vector.
static const __m128i s_sseSwapBytes = _mm_set_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);

static __m128i p_gmacPCLMUL128(const __m128i h, __m128i y) noexcept
{
	y = _mm_shuffle_epi8(y, s_sseSwapBytes);
	__m128i t1 = _mm_clmulepi64_si128(h, y, 0x00);
	__m128i t2 = _mm_clmulepi64_si128(h, y, 0x01);
	__m128i t3 = _mm_clmulepi64_si128(h, y, 0x10);
	__m128i t4 = _mm_clmulepi64_si128(h, y, 0x11);
	t2 = _mm_xor_si128(t2, t3);
	t3 = _mm_slli_si128(t2, 8);
	t2 = _mm_srli_si128(t2, 8);
	t1 = _mm_xor_si128(t1, t3);
	t4 = _mm_xor_si128(t4, t2);
	__m128i t5 = _mm_srli_epi32(t1, 31);
	t1 = _mm_or_si128(_mm_slli_epi32(t1, 1), _mm_slli_si128(t5, 4));
	t4 = _mm_or_si128(_mm_or_si128(_mm_slli_epi32(t4, 1), _mm_slli_si128(_mm_srli_epi32(t4, 31), 4)), _mm_srli_si128(t5, 12));
	t5 = _mm_xor_si128(_mm_xor_si128(_mm_slli_epi32(t1, 31), _mm_slli_epi32(t1, 30)), _mm_slli_epi32(t1, 25));
	t1 = _mm_xor_si128(t1, _mm_slli_si128(t5, 12));
	t4 = _mm_xor_si128(_mm_xor_si128(_mm_xor_si128(_mm_xor_si128(_mm_xor_si128(t4, _mm_srli_si128(t5, 4)), t1), _mm_srli_epi32(t1, 2)), _mm_srli_epi32(t1, 7)), _mm_srli_epi32(t1, 1));
	return _mm_shuffle_epi8(t4, s_sseSwapBytes);
}

#endif

void AES::GMAC::update(const void *const data, unsigned int len) noexcept
{
	const uint8_t *in = reinterpret_cast<const uint8_t *>(data);
	_len += len;

#ifdef ZT_AES_AESNI
	if (likely(Utils::CPUID.aes)) {
		__m128i y = _mm_loadu_si128(reinterpret_cast<const __m128i *>(_y));

		// Handle anything left over from a previous run that wasn't a multiple of 16 bytes.
		if (_rp) {
			for (;;) {
				if (!len)
					return;
				--len;
				_r[_rp++] = *(in++);
				if (_rp == 16) {
					y = p_gmacPCLMUL128(_aes._k.ni.h[0], _mm_xor_si128(y, _mm_loadu_si128(reinterpret_cast<__m128i *>(_r))));
					break;
				}
			}
		}

		if (likely(len >= 64)) {
			const __m128i sb = s_sseSwapBytes;
			const __m128i h = _aes._k.ni.h[0];
			const __m128i hh = _aes._k.ni.h[1];
			const __m128i hhh = _aes._k.ni.h[2];
			const __m128i hhhh = _aes._k.ni.h[3];
			const __m128i h2 = _aes._k.ni.h2[0];
			const __m128i hh2 = _aes._k.ni.h2[1];
			const __m128i hhh2 = _aes._k.ni.h2[2];
			const __m128i hhhh2 = _aes._k.ni.h2[3];
			const uint8_t *const end64 = in + (len & ~((unsigned int)63));
			len &= 63;
			do {
				__m128i d1 = _mm_shuffle_epi8(_mm_xor_si128(y, _mm_loadu_si128(reinterpret_cast<const __m128i *>(in))), sb);
				__m128i d2 = _mm_shuffle_epi8(_mm_loadu_si128(reinterpret_cast<const __m128i *>(in + 16)), sb);
				__m128i d3 = _mm_shuffle_epi8(_mm_loadu_si128(reinterpret_cast<const __m128i *>(in + 32)), sb);
				__m128i d4 = _mm_shuffle_epi8(_mm_loadu_si128(reinterpret_cast<const __m128i *>(in + 48)), sb);
				in += 64;
				__m128i a = _mm_xor_si128(_mm_xor_si128(_mm_clmulepi64_si128(hhhh, d1, 0x00), _mm_clmulepi64_si128(hhh, d2, 0x00)), _mm_xor_si128(_mm_clmulepi64_si128(hh, d3, 0x00), _mm_clmulepi64_si128(h, d4, 0x00)));
				__m128i b = _mm_xor_si128(_mm_xor_si128(_mm_clmulepi64_si128(hhhh, d1, 0x11), _mm_clmulepi64_si128(hhh, d2, 0x11)), _mm_xor_si128(_mm_clmulepi64_si128(hh, d3, 0x11), _mm_clmulepi64_si128(h, d4, 0x11)));
				__m128i c = _mm_xor_si128(_mm_xor_si128(_mm_xor_si128(_mm_clmulepi64_si128(hhhh2, _mm_xor_si128(_mm_shuffle_epi32(d1, 78), d1), 0x00), _mm_clmulepi64_si128(hhh2, _mm_xor_si128(_mm_shuffle_epi32(d2, 78), d2), 0x00)), _mm_xor_si128(_mm_clmulepi64_si128(hh2, _mm_xor_si128(_mm_shuffle_epi32(d3, 78), d3), 0x00), _mm_clmulepi64_si128(h2, _mm_xor_si128(_mm_shuffle_epi32(d4, 78), d4), 0x00))), _mm_xor_si128(a, b));
				a = _mm_xor_si128(_mm_slli_si128(c, 8), a);
				b = _mm_xor_si128(_mm_srli_si128(c, 8), b);
				c = _mm_srli_epi32(a, 31);
				a = _mm_or_si128(_mm_slli_epi32(a, 1), _mm_slli_si128(c, 4));
				b = _mm_or_si128(_mm_or_si128(_mm_slli_epi32(b, 1), _mm_slli_si128(_mm_srli_epi32(b, 31), 4)), _mm_srli_si128(c, 12));
				c = _mm_xor_si128(_mm_slli_epi32(a, 31), _mm_xor_si128(_mm_slli_epi32(a, 30), _mm_slli_epi32(a, 25)));
				a = _mm_xor_si128(a, _mm_slli_si128(c, 12));
				b = _mm_xor_si128(b, _mm_xor_si128(a, _mm_xor_si128(_mm_xor_si128(_mm_srli_epi32(a, 1), _mm_srli_si128(c, 4)), _mm_xor_si128(_mm_srli_epi32(a, 2), _mm_srli_epi32(a, 7)))));
				y = _mm_shuffle_epi8(b, sb);
			} while (likely(in != end64));
		}

		while (len >= 16) {
			y = p_gmacPCLMUL128(_aes._k.ni.h[0], _mm_xor_si128(y, _mm_loadu_si128(reinterpret_cast<const __m128i *>(in))));
			in += 16;
			len -= 16;
		}

		_mm_storeu_si128(reinterpret_cast<__m128i *>(_y), y);

		// Any overflow is cached for a later run or finish().
		for (unsigned int i = 0; i < len; ++i)
			_r[i] = in[i];
		_rp = len; // len is always less than 16 here

		return;
	}
#endif // ZT_AES_AESNI

#ifdef ZT_AES_NEON
	if (Utils::ARMCAP.pmull) {
		uint8x16_t y = vld1q_u8(reinterpret_cast<const uint8_t *>(_y));
		const uint8x16_t h = _aes._k.neon.h;

		if (_rp) {
			for(;;) {
				if (!len)
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

		vst1q_u8(reinterpret_cast<uint8_t *>(_y), y);

		for (unsigned int i = 0; i < len; ++i)
			_r[i] = in[i];
		_rp = len; // len is always less than 16 here

		return;
	}
#endif // ZT_AES_NEON

	const uint64_t h0 = _aes._k.sw.h[0];
	const uint64_t h1 = _aes._k.sw.h[1];
	uint64_t y0 = _y[0];
	uint64_t y1 = _y[1];

	if (_rp) {
		for (;;) {
			if (!len)
				return;
			--len;
			_r[_rp++] = *(in++);
			if (_rp == 16) {
				y0 ^= Utils::loadMachineEndian< uint64_t >(_r);
				y1 ^= Utils::loadMachineEndian< uint64_t >(_r + 8);
				s_gfmul(h0, h1, y0, y1);
				break;
			}
		}
	}

	if (likely(((uintptr_t)in & 7U) == 0U)) {
		while (len >= 16) {
			y0 ^= *reinterpret_cast<const uint64_t *>(in);
			y1 ^= *reinterpret_cast<const uint64_t *>(in + 8);
			in += 16;
			s_gfmul(h0, h1, y0, y1);
			len -= 16;
		}
	} else {
		while (len >= 16) {
			y0 ^= Utils::loadMachineEndian< uint64_t >(in);
			y1 ^= Utils::loadMachineEndian< uint64_t >(in + 8);
			in += 16;
			s_gfmul(h0, h1, y0, y1);
			len -= 16;
		}
	}

	_y[0] = y0;
	_y[1] = y1;

	for (unsigned int i = 0; i < len; ++i)
		_r[i] = in[i];
	_rp = len; // len is always less than 16 here
}

void AES::GMAC::finish(uint8_t tag[16]) noexcept
{
#ifdef ZT_AES_AESNI
	if (likely(Utils::CPUID.aes)) {
		__m128i y = _mm_loadu_si128(reinterpret_cast<const __m128i *>(_y));

		// Handle any remaining bytes, padding the last block with zeroes.
		if (_rp) {
			while (_rp < 16)
				_r[_rp++] = 0;
			y = p_gmacPCLMUL128(_aes._k.ni.h[0], _mm_xor_si128(y, _mm_loadu_si128(reinterpret_cast<__m128i *>(_r))));
		}

		// Interleave encryption of IV with the final GHASH of y XOR (length * 8).
		// Then XOR these together to get the final tag.
		const __m128i *const k = _aes._k.ni.k;
		const __m128i h = _aes._k.ni.h[0];
		y = _mm_xor_si128(y, _mm_set_epi64x(0LL, (long long)Utils::hton((uint64_t)_len << 3U)));
		y = _mm_shuffle_epi8(y, s_sseSwapBytes);
		__m128i encIV = _mm_xor_si128(_mm_loadu_si128(reinterpret_cast<const __m128i *>(_iv)), k[0]);
		__m128i t1 = _mm_clmulepi64_si128(h, y, 0x00);
		__m128i t2 = _mm_clmulepi64_si128(h, y, 0x01);
		__m128i t3 = _mm_clmulepi64_si128(h, y, 0x10);
		__m128i t4 = _mm_clmulepi64_si128(h, y, 0x11);
		encIV = _mm_aesenc_si128(encIV, k[1]);
		t2 = _mm_xor_si128(t2, t3);
		t3 = _mm_slli_si128(t2, 8);
		encIV = _mm_aesenc_si128(encIV, k[2]);
		t2 = _mm_srli_si128(t2, 8);
		t1 = _mm_xor_si128(t1, t3);
		encIV = _mm_aesenc_si128(encIV, k[3]);
		t4 = _mm_xor_si128(t4, t2);
		__m128i t5 = _mm_srli_epi32(t1, 31);
		t1 = _mm_slli_epi32(t1, 1);
		__m128i t6 = _mm_srli_epi32(t4, 31);
		encIV = _mm_aesenc_si128(encIV, k[4]);
		t4 = _mm_slli_epi32(t4, 1);
		t3 = _mm_srli_si128(t5, 12);
		encIV = _mm_aesenc_si128(encIV, k[5]);
		t6 = _mm_slli_si128(t6, 4);
		t5 = _mm_slli_si128(t5, 4);
		encIV = _mm_aesenc_si128(encIV, k[6]);
		t1 = _mm_or_si128(t1, t5);
		t4 = _mm_or_si128(t4, t6);
		encIV = _mm_aesenc_si128(encIV, k[7]);
		t4 = _mm_or_si128(t4, t3);
		t5 = _mm_slli_epi32(t1, 31);
		encIV = _mm_aesenc_si128(encIV, k[8]);
		t6 = _mm_slli_epi32(t1, 30);
		t3 = _mm_slli_epi32(t1, 25);
		encIV = _mm_aesenc_si128(encIV, k[9]);
		t5 = _mm_xor_si128(t5, t6);
		t5 = _mm_xor_si128(t5, t3);
		encIV = _mm_aesenc_si128(encIV, k[10]);
		t6 = _mm_srli_si128(t5, 4);
		t4 = _mm_xor_si128(t4, t6);
		encIV = _mm_aesenc_si128(encIV, k[11]);
		t5 = _mm_slli_si128(t5, 12);
		t1 = _mm_xor_si128(t1, t5);
		t4 = _mm_xor_si128(t4, t1);
		t5 = _mm_srli_epi32(t1, 1);
		encIV = _mm_aesenc_si128(encIV, k[12]);
		t2 = _mm_srli_epi32(t1, 2);
		t3 = _mm_srli_epi32(t1, 7);
		encIV = _mm_aesenc_si128(encIV, k[13]);
		t4 = _mm_xor_si128(t4, t2);
		t4 = _mm_xor_si128(t4, t3);
		encIV = _mm_aesenclast_si128(encIV, k[14]);
		t4 = _mm_xor_si128(t4, t5);
		_mm_storeu_si128(reinterpret_cast<__m128i *>(tag), _mm_xor_si128(_mm_shuffle_epi8(t4, s_sseSwapBytes), encIV));

		return;
	}
#endif // ZT_AES_AESNI

#ifdef ZT_AES_NEON
	if (Utils::ARMCAP.pmull) {
		uint64_t tmp[2];
		uint8x16_t y = vld1q_u8(reinterpret_cast<const uint8_t *>(_y));
		const uint8x16_t h = _aes._k.neon.h;

		if (_rp) {
			while (_rp < 16)
				_r[_rp++] = 0;
			y = s_clmul_armneon_crypto(h, y, _r);
		}

		tmp[0] = Utils::hton((uint64_t)_len << 3U);
		tmp[1] = 0;
		y = s_clmul_armneon_crypto(h, y, reinterpret_cast<const uint8_t *>(tmp));

		Utils::copy< 12 >(tmp, _iv);
#if __BYTE_ORDER == __BIG_ENDIAN
		reinterpret_cast<uint32_t *>(tmp)[3] = 0x00000001;
#else
		reinterpret_cast<uint32_t *>(tmp)[3] = 0x01000000;
#endif
		_aes.encrypt(tmp, tmp);

		uint8x16_t yy = y;
		Utils::storeMachineEndian< uint64_t >(tag, tmp[0] ^ reinterpret_cast<const uint64_t *>(&yy)[0]);
		Utils::storeMachineEndian< uint64_t >(tag + 8, tmp[1] ^ reinterpret_cast<const uint64_t *>(&yy)[1]);

		return;
	}
#endif // ZT_AES_NEON

	const uint64_t h0 = _aes._k.sw.h[0];
	const uint64_t h1 = _aes._k.sw.h[1];
	uint64_t y0 = _y[0];
	uint64_t y1 = _y[1];

	if (_rp) {
		while (_rp < 16)
			_r[_rp++] = 0;
		y0 ^= Utils::loadMachineEndian< uint64_t >(_r);
		y1 ^= Utils::loadMachineEndian< uint64_t >(_r + 8);
		s_gfmul(h0, h1, y0, y1);
	}

	y0 ^= Utils::hton((uint64_t)_len << 3U);
	s_gfmul(h0, h1, y0, y1);

	uint64_t iv2[2];
	Utils::copy< 12 >(iv2, _iv);
#if __BYTE_ORDER == __BIG_ENDIAN
	reinterpret_cast<uint32_t *>(iv2)[3] = 0x00000001;
#else
	reinterpret_cast<uint32_t *>(iv2)[3] = 0x01000000;
#endif
	_aes.encrypt(iv2, iv2);

	Utils::storeMachineEndian< uint64_t >(tag, iv2[0] ^ y0);
	Utils::storeMachineEndian< uint64_t >(tag + 8, iv2[1] ^ y1);
}

// AES-CTR ------------------------------------------------------------------------------------------------------------

#ifdef ZT_AES_AESNI

/* Disable VAES stuff on compilers too old to compile these intrinsics,
 * and MinGW64 also seems not to support them so disable on Windows.
 * The performance gain can be significant but regular SSE is already so
 * fast it's highly unlikely to be a rate limiting factor except on massive
 * servers and network infrastructure stuff. */
#if !defined(__WINDOWS__) && ((__GNUC__ >= 8) || (__clang_major__ >= 7))

#define ZT_AES_VAES512 1

static
__attribute__((__target__("sse4,avx,avx2,vaes,avx512f,avx512bw")))
void p_aesCtrInnerVAES512(unsigned int &len, const uint64_t c0, uint64_t &c1, const uint8_t *&in, uint8_t *&out, const __m128i *const k) noexcept
{
	const __m512i kk0 = _mm512_broadcast_i32x4(k[0]);
	const __m512i kk1 = _mm512_broadcast_i32x4(k[1]);
	const __m512i kk2 = _mm512_broadcast_i32x4(k[2]);
	const __m512i kk3 = _mm512_broadcast_i32x4(k[3]);
	const __m512i kk4 = _mm512_broadcast_i32x4(k[4]);
	const __m512i kk5 = _mm512_broadcast_i32x4(k[5]);
	const __m512i kk6 = _mm512_broadcast_i32x4(k[6]);
	const __m512i kk7 = _mm512_broadcast_i32x4(k[7]);
	const __m512i kk8 = _mm512_broadcast_i32x4(k[8]);
	const __m512i kk9 = _mm512_broadcast_i32x4(k[9]);
	const __m512i kk10 = _mm512_broadcast_i32x4(k[10]);
	const __m512i kk11 = _mm512_broadcast_i32x4(k[11]);
	const __m512i kk12 = _mm512_broadcast_i32x4(k[12]);
	const __m512i kk13 = _mm512_broadcast_i32x4(k[13]);
	const __m512i kk14 = _mm512_broadcast_i32x4(k[14]);
	do {
		__m512i p0 = _mm512_loadu_si512(reinterpret_cast<const __m512i *>(in));
		__m512i d0 = _mm512_set_epi64(
			(long long)Utils::hton(c1 + 3ULL), (long long)c0,
			(long long)Utils::hton(c1 + 2ULL), (long long)c0,
			(long long)Utils::hton(c1 + 1ULL), (long long)c0,
			(long long)Utils::hton(c1), (long long)c0);
		c1 += 4;
		in += 64;
		len -= 64;
		d0 = _mm512_xor_si512(d0, kk0);
		d0 = _mm512_aesenc_epi128(d0, kk1);
		d0 = _mm512_aesenc_epi128(d0, kk2);
		d0 = _mm512_aesenc_epi128(d0, kk3);
		d0 = _mm512_aesenc_epi128(d0, kk4);
		d0 = _mm512_aesenc_epi128(d0, kk5);
		d0 = _mm512_aesenc_epi128(d0, kk6);
		d0 = _mm512_aesenc_epi128(d0, kk7);
		d0 = _mm512_aesenc_epi128(d0, kk8);
		d0 = _mm512_aesenc_epi128(d0, kk9);
		d0 = _mm512_aesenc_epi128(d0, kk10);
		d0 = _mm512_aesenc_epi128(d0, kk11);
		d0 = _mm512_aesenc_epi128(d0, kk12);
		d0 = _mm512_aesenc_epi128(d0, kk13);
		d0 = _mm512_aesenclast_epi128(d0, kk14);
		_mm512_storeu_si512(reinterpret_cast<__m512i *>(out), _mm512_xor_si512(p0, d0));
		out += 64;
	} while (likely(len >= 64));
}

#define ZT_AES_VAES256 1

static
__attribute__((__target__("sse4,avx,avx2,vaes")))
void p_aesCtrInnerVAES256(unsigned int &len, const uint64_t c0, uint64_t &c1, const uint8_t *&in, uint8_t *&out, const __m128i *const k) noexcept
{
	const __m256i kk0 = _mm256_broadcastsi128_si256(k[0]);
	const __m256i kk1 = _mm256_broadcastsi128_si256(k[1]);
	const __m256i kk2 = _mm256_broadcastsi128_si256(k[2]);
	const __m256i kk3 = _mm256_broadcastsi128_si256(k[3]);
	const __m256i kk4 = _mm256_broadcastsi128_si256(k[4]);
	const __m256i kk5 = _mm256_broadcastsi128_si256(k[5]);
	const __m256i kk6 = _mm256_broadcastsi128_si256(k[6]);
	const __m256i kk7 = _mm256_broadcastsi128_si256(k[7]);
	const __m256i kk8 = _mm256_broadcastsi128_si256(k[8]);
	const __m256i kk9 = _mm256_broadcastsi128_si256(k[9]);
	const __m256i kk10 = _mm256_broadcastsi128_si256(k[10]);
	const __m256i kk11 = _mm256_broadcastsi128_si256(k[11]);
	const __m256i kk12 = _mm256_broadcastsi128_si256(k[12]);
	const __m256i kk13 = _mm256_broadcastsi128_si256(k[13]);
	const __m256i kk14 = _mm256_broadcastsi128_si256(k[14]);
	do {
		__m256i p0 = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(in));
		__m256i p1 = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(in + 32));
		__m256i d0 = _mm256_set_epi64x(
			(long long)Utils::hton(c1 + 1ULL), (long long)c0,
			(long long)Utils::hton(c1), (long long)c0);
		__m256i d1 = _mm256_set_epi64x(
			(long long)Utils::hton(c1 + 3ULL), (long long)c0,
			(long long)Utils::hton(c1 + 2ULL), (long long)c0);
		c1 += 4;
		in += 64;
		len -= 64;
		d0 = _mm256_xor_si256(d0, kk0);
		d1 = _mm256_xor_si256(d1, kk0);
		d0 = _mm256_aesenc_epi128(d0, kk1);
		d1 = _mm256_aesenc_epi128(d1, kk1);
		d0 = _mm256_aesenc_epi128(d0, kk2);
		d1 = _mm256_aesenc_epi128(d1, kk2);
		d0 = _mm256_aesenc_epi128(d0, kk3);
		d1 = _mm256_aesenc_epi128(d1, kk3);
		d0 = _mm256_aesenc_epi128(d0, kk4);
		d1 = _mm256_aesenc_epi128(d1, kk4);
		d0 = _mm256_aesenc_epi128(d0, kk5);
		d1 = _mm256_aesenc_epi128(d1, kk5);
		d0 = _mm256_aesenc_epi128(d0, kk6);
		d1 = _mm256_aesenc_epi128(d1, kk6);
		d0 = _mm256_aesenc_epi128(d0, kk7);
		d1 = _mm256_aesenc_epi128(d1, kk7);
		d0 = _mm256_aesenc_epi128(d0, kk8);
		d1 = _mm256_aesenc_epi128(d1, kk8);
		d0 = _mm256_aesenc_epi128(d0, kk9);
		d1 = _mm256_aesenc_epi128(d1, kk9);
		d0 = _mm256_aesenc_epi128(d0, kk10);
		d1 = _mm256_aesenc_epi128(d1, kk10);
		d0 = _mm256_aesenc_epi128(d0, kk11);
		d1 = _mm256_aesenc_epi128(d1, kk11);
		d0 = _mm256_aesenc_epi128(d0, kk12);
		d1 = _mm256_aesenc_epi128(d1, kk12);
		d0 = _mm256_aesenc_epi128(d0, kk13);
		d1 = _mm256_aesenc_epi128(d1, kk13);
		d0 = _mm256_aesenclast_epi128(d0, kk14);
		d1 = _mm256_aesenclast_epi128(d1, kk14);
		_mm256_storeu_si256(reinterpret_cast<__m256i *>(out), _mm256_xor_si256(d0, p0));
		_mm256_storeu_si256(reinterpret_cast<__m256i *>(out + 32), _mm256_xor_si256(d1, p1));
		out += 64;
	} while (likely(len >= 64));
}

#endif // does compiler support AVX2 and AVX512 AES intrinsics?

#endif // ZT_AES_AESNI

void AES::CTR::crypt(const void *const input, unsigned int len) noexcept
{
	const uint8_t *in = reinterpret_cast<const uint8_t *>(input);
	uint8_t *out = _out;

#ifdef ZT_AES_AESNI
	if (likely(Utils::CPUID.aes)) {
		const __m128i dd = _mm_set_epi64x(0, (long long)_ctr[0]);
		uint64_t c1 = Utils::ntoh(_ctr[1]);

		const __m128i *const k = _aes._k.ni.k;
		const __m128i k0 = k[0];
		const __m128i k1 = k[1];
		const __m128i k2 = k[2];
		const __m128i k3 = k[3];
		const __m128i k4 = k[4];
		const __m128i k5 = k[5];
		const __m128i k6 = k[6];
		const __m128i k7 = k[7];
		const __m128i k8 = k[8];
		const __m128i k9 = k[9];
		const __m128i k10 = k[10];
		const __m128i k11 = k[11];
		const __m128i k12 = k[12];
		const __m128i k13 = k[13];
		const __m128i k14 = k[14];

		// Complete any unfinished blocks from previous calls to crypt().
		unsigned int totalLen = _len;
		if ((totalLen & 15U)) {
			for (;;) {
				if (unlikely(!len)) {
					_ctr[1] = Utils::hton(c1);
					_len = totalLen;
					return;
				}
				--len;
				out[totalLen++] = *(in++);
				if (!(totalLen & 15U)) {
					__m128i d0 = _mm_insert_epi64(dd, (long long)Utils::hton(c1++), 1);
					d0 = _mm_xor_si128(d0, k0);
					d0 = _mm_aesenc_si128(d0, k1);
					d0 = _mm_aesenc_si128(d0, k2);
					d0 = _mm_aesenc_si128(d0, k3);
					d0 = _mm_aesenc_si128(d0, k4);
					d0 = _mm_aesenc_si128(d0, k5);
					d0 = _mm_aesenc_si128(d0, k6);
					d0 = _mm_aesenc_si128(d0, k7);
					d0 = _mm_aesenc_si128(d0, k8);
					d0 = _mm_aesenc_si128(d0, k9);
					d0 = _mm_aesenc_si128(d0, k10);
					__m128i *const outblk = reinterpret_cast<__m128i *>(out + (totalLen - 16));
					d0 = _mm_aesenc_si128(d0, k11);
					const __m128i p0 = _mm_loadu_si128(outblk);
					d0 = _mm_aesenc_si128(d0, k12);
					d0 = _mm_aesenc_si128(d0, k13);
					d0 = _mm_aesenclast_si128(d0, k14);
					_mm_storeu_si128(outblk, _mm_xor_si128(p0, d0));
					break;
				}
			}
		}

		out += totalLen;
		_len = totalLen + len;

		if (likely(len >= 64)) {

#if defined(ZT_AES_VAES512) && defined(ZT_AES_VAES256)
			if (Utils::CPUID.vaes && (len >= 256)) {
				if (Utils::CPUID.avx512f) {
					p_aesCtrInnerVAES512(len, _ctr[0], c1, in, out, k);
				} else {
					p_aesCtrInnerVAES256(len, _ctr[0], c1, in, out, k);
				}
				goto skip_conventional_aesni_64;
			}
#endif

#if !defined(ZT_AES_VAES512) && defined(ZT_AES_VAES256)
			if (Utils::CPUID.vaes && (len >= 256)) {
				p_aesCtrInnerVAES256(len, _ctr[0], c1, in, out, k);
				goto skip_conventional_aesni_64;
			}
#endif

			const uint8_t *const eof64 = in + (len & ~((unsigned int)63));
			len &= 63;
			__m128i d0, d1, d2, d3;
			do {
				const uint64_t c10 = Utils::hton(c1);
				const uint64_t c11 = Utils::hton(c1 + 1ULL);
				const uint64_t c12 = Utils::hton(c1 + 2ULL);
				const uint64_t c13 = Utils::hton(c1 + 3ULL);
				d0 = _mm_insert_epi64(dd, (long long)c10, 1);
				d1 = _mm_insert_epi64(dd, (long long)c11, 1);
				d2 = _mm_insert_epi64(dd, (long long)c12, 1);
				d3 = _mm_insert_epi64(dd, (long long)c13, 1);
				c1 += 4;
				d0 = _mm_xor_si128(d0, k0);
				d1 = _mm_xor_si128(d1, k0);
				d2 = _mm_xor_si128(d2, k0);
				d3 = _mm_xor_si128(d3, k0);
				d0 = _mm_aesenc_si128(d0, k1);
				d1 = _mm_aesenc_si128(d1, k1);
				d2 = _mm_aesenc_si128(d2, k1);
				d3 = _mm_aesenc_si128(d3, k1);
				d0 = _mm_aesenc_si128(d0, k2);
				d1 = _mm_aesenc_si128(d1, k2);
				d2 = _mm_aesenc_si128(d2, k2);
				d3 = _mm_aesenc_si128(d3, k2);
				d0 = _mm_aesenc_si128(d0, k3);
				d1 = _mm_aesenc_si128(d1, k3);
				d2 = _mm_aesenc_si128(d2, k3);
				d3 = _mm_aesenc_si128(d3, k3);
				d0 = _mm_aesenc_si128(d0, k4);
				d1 = _mm_aesenc_si128(d1, k4);
				d2 = _mm_aesenc_si128(d2, k4);
				d3 = _mm_aesenc_si128(d3, k4);
				d0 = _mm_aesenc_si128(d0, k5);
				d1 = _mm_aesenc_si128(d1, k5);
				d2 = _mm_aesenc_si128(d2, k5);
				d3 = _mm_aesenc_si128(d3, k5);
				d0 = _mm_aesenc_si128(d0, k6);
				d1 = _mm_aesenc_si128(d1, k6);
				d2 = _mm_aesenc_si128(d2, k6);
				d3 = _mm_aesenc_si128(d3, k6);
				d0 = _mm_aesenc_si128(d0, k7);
				d1 = _mm_aesenc_si128(d1, k7);
				d2 = _mm_aesenc_si128(d2, k7);
				d3 = _mm_aesenc_si128(d3, k7);
				d0 = _mm_aesenc_si128(d0, k8);
				d1 = _mm_aesenc_si128(d1, k8);
				d2 = _mm_aesenc_si128(d2, k8);
				d3 = _mm_aesenc_si128(d3, k8);
				d0 = _mm_aesenc_si128(d0, k9);
				d1 = _mm_aesenc_si128(d1, k9);
				d2 = _mm_aesenc_si128(d2, k9);
				d3 = _mm_aesenc_si128(d3, k9);
				d0 = _mm_aesenc_si128(d0, k10);
				d1 = _mm_aesenc_si128(d1, k10);
				d2 = _mm_aesenc_si128(d2, k10);
				d3 = _mm_aesenc_si128(d3, k10);
				d0 = _mm_aesenc_si128(d0, k11);
				d1 = _mm_aesenc_si128(d1, k11);
				d2 = _mm_aesenc_si128(d2, k11);
				d3 = _mm_aesenc_si128(d3, k11);
				d0 = _mm_aesenc_si128(d0, k12);
				d1 = _mm_aesenc_si128(d1, k12);
				d2 = _mm_aesenc_si128(d2, k12);
				d3 = _mm_aesenc_si128(d3, k12);
				d0 = _mm_aesenc_si128(d0, k13);
				d1 = _mm_aesenc_si128(d1, k13);
				d2 = _mm_aesenc_si128(d2, k13);
				d3 = _mm_aesenc_si128(d3, k13);
				d0 = _mm_xor_si128(_mm_aesenclast_si128(d0, k14), _mm_loadu_si128(reinterpret_cast<const __m128i *>(in)));
				d1 = _mm_xor_si128(_mm_aesenclast_si128(d1, k14), _mm_loadu_si128(reinterpret_cast<const __m128i *>(in + 16)));
				d2 = _mm_xor_si128(_mm_aesenclast_si128(d2, k14), _mm_loadu_si128(reinterpret_cast<const __m128i *>(in + 32)));
				d3 = _mm_xor_si128(_mm_aesenclast_si128(d3, k14), _mm_loadu_si128(reinterpret_cast<const __m128i *>(in + 48)));
				in += 64;
				_mm_storeu_si128(reinterpret_cast<__m128i *>(out), d0);
				_mm_storeu_si128(reinterpret_cast<__m128i *>(out + 16), d1);
				_mm_storeu_si128(reinterpret_cast<__m128i *>(out + 32), d2);
				_mm_storeu_si128(reinterpret_cast<__m128i *>(out + 48), d3);
				out += 64;
			} while (likely(in != eof64));

		}

		skip_conventional_aesni_64:
		while (len >= 16) {
			__m128i d0 = _mm_insert_epi64(dd, (long long)Utils::hton(c1++), 1);
			d0 = _mm_xor_si128(d0, k0);
			d0 = _mm_aesenc_si128(d0, k1);
			d0 = _mm_aesenc_si128(d0, k2);
			d0 = _mm_aesenc_si128(d0, k3);
			d0 = _mm_aesenc_si128(d0, k4);
			d0 = _mm_aesenc_si128(d0, k5);
			d0 = _mm_aesenc_si128(d0, k6);
			d0 = _mm_aesenc_si128(d0, k7);
			d0 = _mm_aesenc_si128(d0, k8);
			d0 = _mm_aesenc_si128(d0, k9);
			d0 = _mm_aesenc_si128(d0, k10);
			d0 = _mm_aesenc_si128(d0, k11);
			d0 = _mm_aesenc_si128(d0, k12);
			d0 = _mm_aesenc_si128(d0, k13);
			_mm_storeu_si128(reinterpret_cast<__m128i *>(out), _mm_xor_si128(_mm_aesenclast_si128(d0, k14), _mm_loadu_si128(reinterpret_cast<const __m128i *>(in))));
			in += 16;
			len -= 16;
			out += 16;
		}

		// Any remaining input is placed in _out. This will be picked up and crypted
		// on subsequent calls to crypt() or finish() as it'll mean _len will not be
		// an even multiple of 16.
		for (unsigned int i = 0; i < len; ++i)
			out[i] = in[i];

		_ctr[1] = Utils::hton(c1);
		return;
	}
#endif // ZT_AES_AESNI

#ifdef ZT_AES_NEON
	if (Utils::ARMCAP.aes) {
		uint8x16_t dd = vrev32q_u8(vld1q_u8(reinterpret_cast<uint8_t *>(_ctr)));
		const uint32x4_t one = {0,0,0,1};

		uint8x16_t k0 = _aes._k.neon.ek[0];
		uint8x16_t k1 = _aes._k.neon.ek[1];
		uint8x16_t k2 = _aes._k.neon.ek[2];
		uint8x16_t k3 = _aes._k.neon.ek[3];
		uint8x16_t k4 = _aes._k.neon.ek[4];
		uint8x16_t k5 = _aes._k.neon.ek[5];
		uint8x16_t k6 = _aes._k.neon.ek[6];
		uint8x16_t k7 = _aes._k.neon.ek[7];
		uint8x16_t k8 = _aes._k.neon.ek[8];
		uint8x16_t k9 = _aes._k.neon.ek[9];
		uint8x16_t k10 = _aes._k.neon.ek[10];
		uint8x16_t k11 = _aes._k.neon.ek[11];
		uint8x16_t k12 = _aes._k.neon.ek[12];
		uint8x16_t k13 = _aes._k.neon.ek[13];
		uint8x16_t k14 = _aes._k.neon.ek[14];

		unsigned int totalLen = _len;
		if ((totalLen & 15U)) {
			for (;;) {
				if (unlikely(!len)) {
					vst1q_u8(reinterpret_cast<uint8_t *>(_ctr), vrev32q_u8(dd));
					_len = totalLen;
					return;
				}
				--len;
				out[totalLen++] = *(in++);
				if (!(totalLen & 15U)) {
					uint8_t *const otmp = out + (totalLen - 16);
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
				in += 16;
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
				uint8x16_t pt1 = vld1q_u8(in);
				in += 16;
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
				uint8x16_t pt2 = vld1q_u8(in);
				in += 16;
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
				uint8x16_t pt3 = vld1q_u8(in);
				in += 16;
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

		vst1q_u8(reinterpret_cast<uint8_t *>(_ctr), vrev32q_u8(dd));
		return;
	}
#endif // ZT_AES_NEON

	uint64_t keyStream[2];
	uint32_t ctr = Utils::ntoh(reinterpret_cast<uint32_t *>(_ctr)[3]);

	unsigned int totalLen = _len;
	if ((totalLen & 15U)) {
		for (;;) {
			if (!len) {
				_len = (totalLen + len);
				return;
			}
			--len;
			out[totalLen++] = *(in++);
			if (!(totalLen & 15U)) {
				_aes._encryptSW(reinterpret_cast<const uint8_t *>(_ctr), reinterpret_cast<uint8_t *>(keyStream));
				reinterpret_cast<uint32_t *>(_ctr)[3] = Utils::hton(++ctr);
				uint8_t *outblk = out + (totalLen - 16);
				for (int i = 0; i < 16; ++i)
					outblk[i] ^= reinterpret_cast<uint8_t *>(keyStream)[i];
				break;
			}
		}
	}

	out += totalLen;
	_len = (totalLen + len);

	if (likely(len >= 16)) {
		const uint32_t *const restrict rk = _aes._k.sw.ek;
		const uint32_t ctr0rk0 = Utils::ntoh(reinterpret_cast<const uint32_t *>(_ctr)[0]) ^ rk[0];
		const uint32_t ctr1rk1 = Utils::ntoh(reinterpret_cast<const uint32_t *>(_ctr)[1]) ^ rk[1];
		const uint32_t ctr2rk2 = Utils::ntoh(reinterpret_cast<const uint32_t *>(_ctr)[2]) ^ rk[2];
		const uint32_t m8 = 0x000000ff;
		const uint32_t m8_8 = 0x0000ff00;
		const uint32_t m8_16 = 0x00ff0000;
		const uint32_t m8_24 = 0xff000000;
		if (likely((((uintptr_t)out & 7U) == 0U) && (((uintptr_t)in & 7U) == 0U))) {
			do {
				uint32_t s0, s1, s2, s3, t0, t1, t2, t3;
				s0 = ctr0rk0;
				s1 = ctr1rk1;
				s2 = ctr2rk2;
				s3 = ctr++ ^ rk[3];

				const uint64_t in0 = *reinterpret_cast<const uint64_t *>(in);
				const uint64_t in1 = *reinterpret_cast<const uint64_t *>(in + 8);
				in += 16;

				t0 = Te0[s0 >> 24U] ^ Te1_r((s1 >> 16U) & m8) ^ Te2_r((s2 >> 8U) & m8) ^ Te3_r(s3 & m8) ^ rk[4];
				t1 = Te0[s1 >> 24U] ^ Te1_r((s2 >> 16U) & m8) ^ Te2_r((s3 >> 8U) & m8) ^ Te3_r(s0 & m8) ^ rk[5];
				t2 = Te0[s2 >> 24U] ^ Te1_r((s3 >> 16U) & m8) ^ Te2_r((s0 >> 8U) & m8) ^ Te3_r(s1 & m8) ^ rk[6];
				t3 = Te0[s3 >> 24U] ^ Te1_r((s0 >> 16U) & m8) ^ Te2_r((s1 >> 8U) & m8) ^ Te3_r(s2 & m8) ^ rk[7];
				s0 = Te0[t0 >> 24U] ^ Te1_r((t1 >> 16U) & m8) ^ Te2_r((t2 >> 8U) & m8) ^ Te3_r(t3 & m8) ^ rk[8];
				s1 = Te0[t1 >> 24U] ^ Te1_r((t2 >> 16U) & m8) ^ Te2_r((t3 >> 8U) & m8) ^ Te3_r(t0 & m8) ^ rk[9];
				s2 = Te0[t2 >> 24U] ^ Te1_r((t3 >> 16U) & m8) ^ Te2_r((t0 >> 8U) & m8) ^ Te3_r(t1 & m8) ^ rk[10];
				s3 = Te0[t3 >> 24U] ^ Te1_r((t0 >> 16U) & m8) ^ Te2_r((t1 >> 8U) & m8) ^ Te3_r(t2 & m8) ^ rk[11];
				t0 = Te0[s0 >> 24U] ^ Te1_r((s1 >> 16U) & m8) ^ Te2_r((s2 >> 8U) & m8) ^ Te3_r(s3 & m8) ^ rk[12];
				t1 = Te0[s1 >> 24U] ^ Te1_r((s2 >> 16U) & m8) ^ Te2_r((s3 >> 8U) & m8) ^ Te3_r(s0 & m8) ^ rk[13];
				t2 = Te0[s2 >> 24U] ^ Te1_r((s3 >> 16U) & m8) ^ Te2_r((s0 >> 8U) & m8) ^ Te3_r(s1 & m8) ^ rk[14];
				t3 = Te0[s3 >> 24U] ^ Te1_r((s0 >> 16U) & m8) ^ Te2_r((s1 >> 8U) & m8) ^ Te3_r(s2 & m8) ^ rk[15];
				s0 = Te0[t0 >> 24U] ^ Te1_r((t1 >> 16U) & m8) ^ Te2_r((t2 >> 8U) & m8) ^ Te3_r(t3 & m8) ^ rk[16];
				s1 = Te0[t1 >> 24U] ^ Te1_r((t2 >> 16U) & m8) ^ Te2_r((t3 >> 8U) & m8) ^ Te3_r(t0 & m8) ^ rk[17];
				s2 = Te0[t2 >> 24U] ^ Te1_r((t3 >> 16U) & m8) ^ Te2_r((t0 >> 8U) & m8) ^ Te3_r(t1 & m8) ^ rk[18];
				s3 = Te0[t3 >> 24U] ^ Te1_r((t0 >> 16U) & m8) ^ Te2_r((t1 >> 8U) & m8) ^ Te3_r(t2 & m8) ^ rk[19];
				t0 = Te0[s0 >> 24U] ^ Te1_r((s1 >> 16U) & m8) ^ Te2_r((s2 >> 8U) & m8) ^ Te3_r(s3 & m8) ^ rk[20];
				t1 = Te0[s1 >> 24U] ^ Te1_r((s2 >> 16U) & m8) ^ Te2_r((s3 >> 8U) & m8) ^ Te3_r(s0 & m8) ^ rk[21];
				t2 = Te0[s2 >> 24U] ^ Te1_r((s3 >> 16U) & m8) ^ Te2_r((s0 >> 8U) & m8) ^ Te3_r(s1 & m8) ^ rk[22];
				t3 = Te0[s3 >> 24U] ^ Te1_r((s0 >> 16U) & m8) ^ Te2_r((s1 >> 8U) & m8) ^ Te3_r(s2 & m8) ^ rk[23];
				s0 = Te0[t0 >> 24U] ^ Te1_r((t1 >> 16U) & m8) ^ Te2_r((t2 >> 8U) & m8) ^ Te3_r(t3 & m8) ^ rk[24];
				s1 = Te0[t1 >> 24U] ^ Te1_r((t2 >> 16U) & m8) ^ Te2_r((t3 >> 8U) & m8) ^ Te3_r(t0 & m8) ^ rk[25];
				s2 = Te0[t2 >> 24U] ^ Te1_r((t3 >> 16U) & m8) ^ Te2_r((t0 >> 8U) & m8) ^ Te3_r(t1 & m8) ^ rk[26];
				s3 = Te0[t3 >> 24U] ^ Te1_r((t0 >> 16U) & m8) ^ Te2_r((t1 >> 8U) & m8) ^ Te3_r(t2 & m8) ^ rk[27];
				t0 = Te0[s0 >> 24U] ^ Te1_r((s1 >> 16U) & m8) ^ Te2_r((s2 >> 8U) & m8) ^ Te3_r(s3 & m8) ^ rk[28];
				t1 = Te0[s1 >> 24U] ^ Te1_r((s2 >> 16U) & m8) ^ Te2_r((s3 >> 8U) & m8) ^ Te3_r(s0 & m8) ^ rk[29];
				t2 = Te0[s2 >> 24U] ^ Te1_r((s3 >> 16U) & m8) ^ Te2_r((s0 >> 8U) & m8) ^ Te3_r(s1 & m8) ^ rk[30];
				t3 = Te0[s3 >> 24U] ^ Te1_r((s0 >> 16U) & m8) ^ Te2_r((s1 >> 8U) & m8) ^ Te3_r(s2 & m8) ^ rk[31];
				s0 = Te0[t0 >> 24U] ^ Te1_r((t1 >> 16U) & m8) ^ Te2_r((t2 >> 8U) & m8) ^ Te3_r(t3 & m8) ^ rk[32];
				s1 = Te0[t1 >> 24U] ^ Te1_r((t2 >> 16U) & m8) ^ Te2_r((t3 >> 8U) & m8) ^ Te3_r(t0 & m8) ^ rk[33];
				s2 = Te0[t2 >> 24U] ^ Te1_r((t3 >> 16U) & m8) ^ Te2_r((t0 >> 8U) & m8) ^ Te3_r(t1 & m8) ^ rk[34];
				s3 = Te0[t3 >> 24U] ^ Te1_r((t0 >> 16U) & m8) ^ Te2_r((t1 >> 8U) & m8) ^ Te3_r(t2 & m8) ^ rk[35];
				t0 = Te0[s0 >> 24U] ^ Te1_r((s1 >> 16U) & m8) ^ Te2_r((s2 >> 8U) & m8) ^ Te3_r(s3 & m8) ^ rk[36];
				t1 = Te0[s1 >> 24U] ^ Te1_r((s2 >> 16U) & m8) ^ Te2_r((s3 >> 8U) & m8) ^ Te3_r(s0 & m8) ^ rk[37];
				t2 = Te0[s2 >> 24U] ^ Te1_r((s3 >> 16U) & m8) ^ Te2_r((s0 >> 8U) & m8) ^ Te3_r(s1 & m8) ^ rk[38];
				t3 = Te0[s3 >> 24U] ^ Te1_r((s0 >> 16U) & m8) ^ Te2_r((s1 >> 8U) & m8) ^ Te3_r(s2 & m8) ^ rk[39];
				s0 = Te0[t0 >> 24U] ^ Te1_r((t1 >> 16U) & m8) ^ Te2_r((t2 >> 8U) & m8) ^ Te3_r(t3 & m8) ^ rk[40];
				s1 = Te0[t1 >> 24U] ^ Te1_r((t2 >> 16U) & m8) ^ Te2_r((t3 >> 8U) & m8) ^ Te3_r(t0 & m8) ^ rk[41];
				s2 = Te0[t2 >> 24U] ^ Te1_r((t3 >> 16U) & m8) ^ Te2_r((t0 >> 8U) & m8) ^ Te3_r(t1 & m8) ^ rk[42];
				s3 = Te0[t3 >> 24U] ^ Te1_r((t0 >> 16U) & m8) ^ Te2_r((t1 >> 8U) & m8) ^ Te3_r(t2 & m8) ^ rk[43];
				t0 = Te0[s0 >> 24U] ^ Te1_r((s1 >> 16U) & m8) ^ Te2_r((s2 >> 8U) & m8) ^ Te3_r(s3 & m8) ^ rk[44];
				t1 = Te0[s1 >> 24U] ^ Te1_r((s2 >> 16U) & m8) ^ Te2_r((s3 >> 8U) & m8) ^ Te3_r(s0 & m8) ^ rk[45];
				t2 = Te0[s2 >> 24U] ^ Te1_r((s3 >> 16U) & m8) ^ Te2_r((s0 >> 8U) & m8) ^ Te3_r(s1 & m8) ^ rk[46];
				t3 = Te0[s3 >> 24U] ^ Te1_r((s0 >> 16U) & m8) ^ Te2_r((s1 >> 8U) & m8) ^ Te3_r(s2 & m8) ^ rk[47];
				s0 = Te0[t0 >> 24U] ^ Te1_r((t1 >> 16U) & m8) ^ Te2_r((t2 >> 8U) & m8) ^ Te3_r(t3 & m8) ^ rk[48];
				s1 = Te0[t1 >> 24U] ^ Te1_r((t2 >> 16U) & m8) ^ Te2_r((t3 >> 8U) & m8) ^ Te3_r(t0 & m8) ^ rk[49];
				s2 = Te0[t2 >> 24U] ^ Te1_r((t3 >> 16U) & m8) ^ Te2_r((t0 >> 8U) & m8) ^ Te3_r(t1 & m8) ^ rk[50];
				s3 = Te0[t3 >> 24U] ^ Te1_r((t0 >> 16U) & m8) ^ Te2_r((t1 >> 8U) & m8) ^ Te3_r(t2 & m8) ^ rk[51];
				t0 = Te0[s0 >> 24U] ^ Te1_r((s1 >> 16U) & m8) ^ Te2_r((s2 >> 8U) & m8) ^ Te3_r(s3 & m8) ^ rk[52];
				t1 = Te0[s1 >> 24U] ^ Te1_r((s2 >> 16U) & m8) ^ Te2_r((s3 >> 8U) & m8) ^ Te3_r(s0 & m8) ^ rk[53];
				t2 = Te0[s2 >> 24U] ^ Te1_r((s3 >> 16U) & m8) ^ Te2_r((s0 >> 8U) & m8) ^ Te3_r(s1 & m8) ^ rk[54];
				t3 = Te0[s3 >> 24U] ^ Te1_r((s0 >> 16U) & m8) ^ Te2_r((s1 >> 8U) & m8) ^ Te3_r(s2 & m8) ^ rk[55];
				s0 = (Te2_r(t0 >> 24U) & m8_24) ^ (Te3_r((t1 >> 16U) & m8) & m8_16) ^ (Te0[(t2 >> 8U) & m8] & m8_8) ^ (Te1_r(t3 & m8) & m8) ^ rk[56];
				s1 = (Te2_r(t1 >> 24U) & m8_24) ^ (Te3_r((t2 >> 16U) & m8) & m8_16) ^ (Te0[(t3 >> 8U) & m8] & m8_8) ^ (Te1_r(t0 & m8) & m8) ^ rk[57];
				s2 = (Te2_r(t2 >> 24U) & m8_24) ^ (Te3_r((t3 >> 16U) & m8) & m8_16) ^ (Te0[(t0 >> 8U) & m8] & m8_8) ^ (Te1_r(t1 & m8) & m8) ^ rk[58];
				s3 = (Te2_r(t3 >> 24U) & m8_24) ^ (Te3_r((t0 >> 16U) & m8) & m8_16) ^ (Te0[(t1 >> 8U) & m8] & m8_8) ^ (Te1_r(t2 & m8) & m8) ^ rk[59];

				*reinterpret_cast<uint64_t *>(out) = in0 ^ Utils::hton(((uint64_t)s0 << 32U) | (uint64_t)s1);
				*reinterpret_cast<uint64_t *>(out + 8) = in1 ^ Utils::hton(((uint64_t)s2 << 32U) | (uint64_t)s3);
				out += 16;
			} while ((len -= 16) >= 16);
		} else {
			do {
				uint32_t s0, s1, s2, s3, t0, t1, t2, t3;
				s0 = ctr0rk0;
				s1 = ctr1rk1;
				s2 = ctr2rk2;
				s3 = ctr++ ^ rk[3];

				t0 = Te0[s0 >> 24U] ^ Te1_r((s1 >> 16U) & m8) ^ Te2_r((s2 >> 8U) & m8) ^ Te3_r(s3 & m8) ^ rk[4];
				t1 = Te0[s1 >> 24U] ^ Te1_r((s2 >> 16U) & m8) ^ Te2_r((s3 >> 8U) & m8) ^ Te3_r(s0 & m8) ^ rk[5];
				t2 = Te0[s2 >> 24U] ^ Te1_r((s3 >> 16U) & m8) ^ Te2_r((s0 >> 8U) & m8) ^ Te3_r(s1 & m8) ^ rk[6];
				t3 = Te0[s3 >> 24U] ^ Te1_r((s0 >> 16U) & m8) ^ Te2_r((s1 >> 8U) & m8) ^ Te3_r(s2 & m8) ^ rk[7];
				s0 = Te0[t0 >> 24U] ^ Te1_r((t1 >> 16U) & m8) ^ Te2_r((t2 >> 8U) & m8) ^ Te3_r(t3 & m8) ^ rk[8];
				s1 = Te0[t1 >> 24U] ^ Te1_r((t2 >> 16U) & m8) ^ Te2_r((t3 >> 8U) & m8) ^ Te3_r(t0 & m8) ^ rk[9];
				s2 = Te0[t2 >> 24U] ^ Te1_r((t3 >> 16U) & m8) ^ Te2_r((t0 >> 8U) & m8) ^ Te3_r(t1 & m8) ^ rk[10];
				s3 = Te0[t3 >> 24U] ^ Te1_r((t0 >> 16U) & m8) ^ Te2_r((t1 >> 8U) & m8) ^ Te3_r(t2 & m8) ^ rk[11];
				t0 = Te0[s0 >> 24U] ^ Te1_r((s1 >> 16U) & m8) ^ Te2_r((s2 >> 8U) & m8) ^ Te3_r(s3 & m8) ^ rk[12];
				t1 = Te0[s1 >> 24U] ^ Te1_r((s2 >> 16U) & m8) ^ Te2_r((s3 >> 8U) & m8) ^ Te3_r(s0 & m8) ^ rk[13];
				t2 = Te0[s2 >> 24U] ^ Te1_r((s3 >> 16U) & m8) ^ Te2_r((s0 >> 8U) & m8) ^ Te3_r(s1 & m8) ^ rk[14];
				t3 = Te0[s3 >> 24U] ^ Te1_r((s0 >> 16U) & m8) ^ Te2_r((s1 >> 8U) & m8) ^ Te3_r(s2 & m8) ^ rk[15];
				s0 = Te0[t0 >> 24U] ^ Te1_r((t1 >> 16U) & m8) ^ Te2_r((t2 >> 8U) & m8) ^ Te3_r(t3 & m8) ^ rk[16];
				s1 = Te0[t1 >> 24U] ^ Te1_r((t2 >> 16U) & m8) ^ Te2_r((t3 >> 8U) & m8) ^ Te3_r(t0 & m8) ^ rk[17];
				s2 = Te0[t2 >> 24U] ^ Te1_r((t3 >> 16U) & m8) ^ Te2_r((t0 >> 8U) & m8) ^ Te3_r(t1 & m8) ^ rk[18];
				s3 = Te0[t3 >> 24U] ^ Te1_r((t0 >> 16U) & m8) ^ Te2_r((t1 >> 8U) & m8) ^ Te3_r(t2 & m8) ^ rk[19];
				t0 = Te0[s0 >> 24U] ^ Te1_r((s1 >> 16U) & m8) ^ Te2_r((s2 >> 8U) & m8) ^ Te3_r(s3 & m8) ^ rk[20];
				t1 = Te0[s1 >> 24U] ^ Te1_r((s2 >> 16U) & m8) ^ Te2_r((s3 >> 8U) & m8) ^ Te3_r(s0 & m8) ^ rk[21];
				t2 = Te0[s2 >> 24U] ^ Te1_r((s3 >> 16U) & m8) ^ Te2_r((s0 >> 8U) & m8) ^ Te3_r(s1 & m8) ^ rk[22];
				t3 = Te0[s3 >> 24U] ^ Te1_r((s0 >> 16U) & m8) ^ Te2_r((s1 >> 8U) & m8) ^ Te3_r(s2 & m8) ^ rk[23];
				s0 = Te0[t0 >> 24U] ^ Te1_r((t1 >> 16U) & m8) ^ Te2_r((t2 >> 8U) & m8) ^ Te3_r(t3 & m8) ^ rk[24];
				s1 = Te0[t1 >> 24U] ^ Te1_r((t2 >> 16U) & m8) ^ Te2_r((t3 >> 8U) & m8) ^ Te3_r(t0 & m8) ^ rk[25];
				s2 = Te0[t2 >> 24U] ^ Te1_r((t3 >> 16U) & m8) ^ Te2_r((t0 >> 8U) & m8) ^ Te3_r(t1 & m8) ^ rk[26];
				s3 = Te0[t3 >> 24U] ^ Te1_r((t0 >> 16U) & m8) ^ Te2_r((t1 >> 8U) & m8) ^ Te3_r(t2 & m8) ^ rk[27];
				t0 = Te0[s0 >> 24U] ^ Te1_r((s1 >> 16U) & m8) ^ Te2_r((s2 >> 8U) & m8) ^ Te3_r(s3 & m8) ^ rk[28];
				t1 = Te0[s1 >> 24U] ^ Te1_r((s2 >> 16U) & m8) ^ Te2_r((s3 >> 8U) & m8) ^ Te3_r(s0 & m8) ^ rk[29];
				t2 = Te0[s2 >> 24U] ^ Te1_r((s3 >> 16U) & m8) ^ Te2_r((s0 >> 8U) & m8) ^ Te3_r(s1 & m8) ^ rk[30];
				t3 = Te0[s3 >> 24U] ^ Te1_r((s0 >> 16U) & m8) ^ Te2_r((s1 >> 8U) & m8) ^ Te3_r(s2 & m8) ^ rk[31];
				s0 = Te0[t0 >> 24U] ^ Te1_r((t1 >> 16U) & m8) ^ Te2_r((t2 >> 8U) & m8) ^ Te3_r(t3 & m8) ^ rk[32];
				s1 = Te0[t1 >> 24U] ^ Te1_r((t2 >> 16U) & m8) ^ Te2_r((t3 >> 8U) & m8) ^ Te3_r(t0 & m8) ^ rk[33];
				s2 = Te0[t2 >> 24U] ^ Te1_r((t3 >> 16U) & m8) ^ Te2_r((t0 >> 8U) & m8) ^ Te3_r(t1 & m8) ^ rk[34];
				s3 = Te0[t3 >> 24U] ^ Te1_r((t0 >> 16U) & m8) ^ Te2_r((t1 >> 8U) & m8) ^ Te3_r(t2 & m8) ^ rk[35];
				t0 = Te0[s0 >> 24U] ^ Te1_r((s1 >> 16U) & m8) ^ Te2_r((s2 >> 8U) & m8) ^ Te3_r(s3 & m8) ^ rk[36];
				t1 = Te0[s1 >> 24U] ^ Te1_r((s2 >> 16U) & m8) ^ Te2_r((s3 >> 8U) & m8) ^ Te3_r(s0 & m8) ^ rk[37];
				t2 = Te0[s2 >> 24U] ^ Te1_r((s3 >> 16U) & m8) ^ Te2_r((s0 >> 8U) & m8) ^ Te3_r(s1 & m8) ^ rk[38];
				t3 = Te0[s3 >> 24U] ^ Te1_r((s0 >> 16U) & m8) ^ Te2_r((s1 >> 8U) & m8) ^ Te3_r(s2 & m8) ^ rk[39];
				s0 = Te0[t0 >> 24U] ^ Te1_r((t1 >> 16U) & m8) ^ Te2_r((t2 >> 8U) & m8) ^ Te3_r(t3 & m8) ^ rk[40];
				s1 = Te0[t1 >> 24U] ^ Te1_r((t2 >> 16U) & m8) ^ Te2_r((t3 >> 8U) & m8) ^ Te3_r(t0 & m8) ^ rk[41];
				s2 = Te0[t2 >> 24U] ^ Te1_r((t3 >> 16U) & m8) ^ Te2_r((t0 >> 8U) & m8) ^ Te3_r(t1 & m8) ^ rk[42];
				s3 = Te0[t3 >> 24U] ^ Te1_r((t0 >> 16U) & m8) ^ Te2_r((t1 >> 8U) & m8) ^ Te3_r(t2 & m8) ^ rk[43];
				t0 = Te0[s0 >> 24U] ^ Te1_r((s1 >> 16U) & m8) ^ Te2_r((s2 >> 8U) & m8) ^ Te3_r(s3 & m8) ^ rk[44];
				t1 = Te0[s1 >> 24U] ^ Te1_r((s2 >> 16U) & m8) ^ Te2_r((s3 >> 8U) & m8) ^ Te3_r(s0 & m8) ^ rk[45];
				t2 = Te0[s2 >> 24U] ^ Te1_r((s3 >> 16U) & m8) ^ Te2_r((s0 >> 8U) & m8) ^ Te3_r(s1 & m8) ^ rk[46];
				t3 = Te0[s3 >> 24U] ^ Te1_r((s0 >> 16U) & m8) ^ Te2_r((s1 >> 8U) & m8) ^ Te3_r(s2 & m8) ^ rk[47];
				s0 = Te0[t0 >> 24U] ^ Te1_r((t1 >> 16U) & m8) ^ Te2_r((t2 >> 8U) & m8) ^ Te3_r(t3 & m8) ^ rk[48];
				s1 = Te0[t1 >> 24U] ^ Te1_r((t2 >> 16U) & m8) ^ Te2_r((t3 >> 8U) & m8) ^ Te3_r(t0 & m8) ^ rk[49];
				s2 = Te0[t2 >> 24U] ^ Te1_r((t3 >> 16U) & m8) ^ Te2_r((t0 >> 8U) & m8) ^ Te3_r(t1 & m8) ^ rk[50];
				s3 = Te0[t3 >> 24U] ^ Te1_r((t0 >> 16U) & m8) ^ Te2_r((t1 >> 8U) & m8) ^ Te3_r(t2 & m8) ^ rk[51];
				t0 = Te0[s0 >> 24U] ^ Te1_r((s1 >> 16U) & m8) ^ Te2_r((s2 >> 8U) & m8) ^ Te3_r(s3 & m8) ^ rk[52];
				t1 = Te0[s1 >> 24U] ^ Te1_r((s2 >> 16U) & m8) ^ Te2_r((s3 >> 8U) & m8) ^ Te3_r(s0 & m8) ^ rk[53];
				t2 = Te0[s2 >> 24U] ^ Te1_r((s3 >> 16U) & m8) ^ Te2_r((s0 >> 8U) & m8) ^ Te3_r(s1 & m8) ^ rk[54];
				t3 = Te0[s3 >> 24U] ^ Te1_r((s0 >> 16U) & m8) ^ Te2_r((s1 >> 8U) & m8) ^ Te3_r(s2 & m8) ^ rk[55];
				s0 = (Te2_r(t0 >> 24U) & m8_24) ^ (Te3_r((t1 >> 16U) & m8) & m8_16) ^ (Te0[(t2 >> 8U) & m8] & m8_8) ^ (Te1_r(t3 & m8) & m8) ^ rk[56];
				s1 = (Te2_r(t1 >> 24U) & m8_24) ^ (Te3_r((t2 >> 16U) & m8) & m8_16) ^ (Te0[(t3 >> 8U) & m8] & m8_8) ^ (Te1_r(t0 & m8) & m8) ^ rk[57];
				s2 = (Te2_r(t2 >> 24U) & m8_24) ^ (Te3_r((t3 >> 16U) & m8) & m8_16) ^ (Te0[(t0 >> 8U) & m8] & m8_8) ^ (Te1_r(t1 & m8) & m8) ^ rk[58];
				s3 = (Te2_r(t3 >> 24U) & m8_24) ^ (Te3_r((t0 >> 16U) & m8) & m8_16) ^ (Te0[(t1 >> 8U) & m8] & m8_8) ^ (Te1_r(t2 & m8) & m8) ^ rk[59];

				out[0] = in[0] ^ (uint8_t)(s0 >> 24U);
				out[1] = in[1] ^ (uint8_t)(s0 >> 16U);
				out[2] = in[2] ^ (uint8_t)(s0 >> 8U);
				out[3] = in[3] ^ (uint8_t)s0;
				out[4] = in[4] ^ (uint8_t)(s1 >> 24U);
				out[5] = in[5] ^ (uint8_t)(s1 >> 16U);
				out[6] = in[6] ^ (uint8_t)(s1 >> 8U);
				out[7] = in[7] ^ (uint8_t)s1;
				out[8] = in[8] ^ (uint8_t)(s2 >> 24U);
				out[9] = in[9] ^ (uint8_t)(s2 >> 16U);
				out[10] = in[10] ^ (uint8_t)(s2 >> 8U);
				out[11] = in[11] ^ (uint8_t)s2;
				out[12] = in[12] ^ (uint8_t)(s3 >> 24U);
				out[13] = in[13] ^ (uint8_t)(s3 >> 16U);
				out[14] = in[14] ^ (uint8_t)(s3 >> 8U);
				out[15] = in[15] ^ (uint8_t)s3;
				out += 16;
				in += 16;
			} while ((len -= 16) >= 16);
		}
		reinterpret_cast<uint32_t *>(_ctr)[3] = Utils::hton(ctr);
	}

	// Any remaining input is placed in _out. This will be picked up and crypted
	// on subsequent calls to crypt() or finish() as it'll mean _len will not be
	// an even multiple of 16.
	while (len) {
		--len;
		*(out++) = *(in++);
	}
}

void AES::CTR::finish() noexcept
{
	uint8_t tmp[16];
	const unsigned int rem = _len & 15U;
	if (rem) {
		_aes.encrypt(_ctr, tmp);
		for (unsigned int i = 0, j = _len - rem; i < rem; ++i)
			_out[j + i] ^= tmp[i];
	}
}

// Software AES and AES key expansion ---------------------------------------------------------------------------------

const uint32_t AES::Te0[256] = {0xc66363a5, 0xf87c7c84, 0xee777799, 0xf67b7b8d, 0xfff2f20d, 0xd66b6bbd, 0xde6f6fb1, 0x91c5c554, 0x60303050, 0x02010103, 0xce6767a9, 0x562b2b7d, 0xe7fefe19, 0xb5d7d762, 0x4dababe6, 0xec76769a, 0x8fcaca45, 0x1f82829d, 0x89c9c940, 0xfa7d7d87, 0xeffafa15, 0xb25959eb, 0x8e4747c9, 0xfbf0f00b, 0x41adadec, 0xb3d4d467, 0x5fa2a2fd, 0x45afafea, 0x239c9cbf, 0x53a4a4f7, 0xe4727296, 0x9bc0c05b, 0x75b7b7c2, 0xe1fdfd1c, 0x3d9393ae, 0x4c26266a, 0x6c36365a, 0x7e3f3f41, 0xf5f7f702, 0x83cccc4f, 0x6834345c, 0x51a5a5f4, 0xd1e5e534, 0xf9f1f108, 0xe2717193, 0xabd8d873, 0x62313153,
                                0x2a15153f, 0x0804040c, 0x95c7c752, 0x46232365, 0x9dc3c35e, 0x30181828, 0x379696a1, 0x0a05050f, 0x2f9a9ab5, 0x0e070709, 0x24121236, 0x1b80809b, 0xdfe2e23d, 0xcdebeb26, 0x4e272769, 0x7fb2b2cd, 0xea75759f,
                                0x1209091b, 0x1d83839e, 0x582c2c74, 0x341a1a2e, 0x361b1b2d, 0xdc6e6eb2, 0xb45a5aee, 0x5ba0a0fb, 0xa45252f6, 0x763b3b4d, 0xb7d6d661, 0x7db3b3ce, 0x5229297b, 0xdde3e33e, 0x5e2f2f71, 0x13848497, 0xa65353f5, 0xb9d1d168, 0x00000000, 0xc1eded2c, 0x40202060, 0xe3fcfc1f, 0x79b1b1c8, 0xb65b5bed, 0xd46a6abe, 0x8dcbcb46, 0x67bebed9, 0x7239394b, 0x944a4ade, 0x984c4cd4, 0xb05858e8, 0x85cfcf4a, 0xbbd0d06b, 0xc5efef2a, 0x4faaaae5, 0xedfbfb16, 0x864343c5, 0x9a4d4dd7, 0x66333355, 0x11858594, 0x8a4545cf, 0xe9f9f910, 0x04020206, 0xfe7f7f81, 0xa05050f0, 0x783c3c44, 0x259f9fba,
                                0x4ba8a8e3, 0xa25151f3, 0x5da3a3fe, 0x804040c0, 0x058f8f8a, 0x3f9292ad, 0x219d9dbc, 0x70383848, 0xf1f5f504, 0x63bcbcdf, 0x77b6b6c1, 0xafdada75, 0x42212163, 0x20101030, 0xe5ffff1a, 0xfdf3f30e, 0xbfd2d26d,
                                0x81cdcd4c, 0x180c0c14, 0x26131335, 0xc3ecec2f, 0xbe5f5fe1, 0x359797a2, 0x884444cc, 0x2e171739, 0x93c4c457, 0x55a7a7f2, 0xfc7e7e82, 0x7a3d3d47, 0xc86464ac, 0xba5d5de7, 0x3219192b, 0xe6737395, 0xc06060a0, 0x19818198, 0x9e4f4fd1, 0xa3dcdc7f, 0x44222266, 0x542a2a7e, 0x3b9090ab, 0x0b888883, 0x8c4646ca, 0xc7eeee29, 0x6bb8b8d3, 0x2814143c, 0xa7dede79, 0xbc5e5ee2, 0x160b0b1d, 0xaddbdb76, 0xdbe0e03b, 0x64323256, 0x743a3a4e, 0x140a0a1e, 0x924949db, 0x0c06060a, 0x4824246c, 0xb85c5ce4, 0x9fc2c25d, 0xbdd3d36e, 0x43acacef, 0xc46262a6, 0x399191a8, 0x319595a4, 0xd3e4e437,
                                0xf279798b, 0xd5e7e732, 0x8bc8c843, 0x6e373759, 0xda6d6db7, 0x018d8d8c, 0xb1d5d564, 0x9c4e4ed2, 0x49a9a9e0, 0xd86c6cb4, 0xac5656fa, 0xf3f4f407, 0xcfeaea25, 0xca6565af, 0xf47a7a8e, 0x47aeaee9, 0x10080818,
                                0x6fbabad5, 0xf0787888, 0x4a25256f, 0x5c2e2e72, 0x381c1c24, 0x57a6a6f1, 0x73b4b4c7, 0x97c6c651, 0xcbe8e823, 0xa1dddd7c, 0xe874749c, 0x3e1f1f21, 0x964b4bdd, 0x61bdbddc, 0x0d8b8b86, 0x0f8a8a85, 0xe0707090, 0x7c3e3e42, 0x71b5b5c4, 0xcc6666aa, 0x904848d8, 0x06030305, 0xf7f6f601, 0x1c0e0e12, 0xc26161a3, 0x6a35355f, 0xae5757f9, 0x69b9b9d0, 0x17868691, 0x99c1c158, 0x3a1d1d27, 0x279e9eb9, 0xd9e1e138, 0xebf8f813, 0x2b9898b3, 0x22111133, 0xd26969bb, 0xa9d9d970, 0x078e8e89, 0x339494a7, 0x2d9b9bb6, 0x3c1e1e22, 0x15878792, 0xc9e9e920, 0x87cece49, 0xaa5555ff, 0x50282878,
                                0xa5dfdf7a, 0x038c8c8f, 0x59a1a1f8, 0x09898980, 0x1a0d0d17, 0x65bfbfda, 0xd7e6e631, 0x844242c6, 0xd06868b8, 0x824141c3, 0x299999b0, 0x5a2d2d77, 0x1e0f0f11, 0x7bb0b0cb, 0xa85454fc, 0x6dbbbbd6, 0x2c16163a};
const uint32_t AES::Te4[256] = {0x63636363, 0x7c7c7c7c, 0x77777777, 0x7b7b7b7b, 0xf2f2f2f2, 0x6b6b6b6b, 0x6f6f6f6f, 0xc5c5c5c5, 0x30303030, 0x01010101, 0x67676767, 0x2b2b2b2b, 0xfefefefe, 0xd7d7d7d7, 0xabababab, 0x76767676, 0xcacacaca, 0x82828282, 0xc9c9c9c9, 0x7d7d7d7d, 0xfafafafa, 0x59595959, 0x47474747, 0xf0f0f0f0, 0xadadadad, 0xd4d4d4d4, 0xa2a2a2a2, 0xafafafaf, 0x9c9c9c9c, 0xa4a4a4a4, 0x72727272, 0xc0c0c0c0, 0xb7b7b7b7, 0xfdfdfdfd, 0x93939393, 0x26262626, 0x36363636, 0x3f3f3f3f, 0xf7f7f7f7, 0xcccccccc, 0x34343434, 0xa5a5a5a5, 0xe5e5e5e5, 0xf1f1f1f1, 0x71717171, 0xd8d8d8d8, 0x31313131,
                                0x15151515, 0x04040404, 0xc7c7c7c7, 0x23232323, 0xc3c3c3c3, 0x18181818, 0x96969696, 0x05050505, 0x9a9a9a9a, 0x07070707, 0x12121212, 0x80808080, 0xe2e2e2e2, 0xebebebeb, 0x27272727, 0xb2b2b2b2, 0x75757575,
                                0x09090909, 0x83838383, 0x2c2c2c2c, 0x1a1a1a1a, 0x1b1b1b1b, 0x6e6e6e6e, 0x5a5a5a5a, 0xa0a0a0a0, 0x52525252, 0x3b3b3b3b, 0xd6d6d6d6, 0xb3b3b3b3, 0x29292929, 0xe3e3e3e3, 0x2f2f2f2f, 0x84848484, 0x53535353, 0xd1d1d1d1, 0x00000000, 0xedededed, 0x20202020, 0xfcfcfcfc, 0xb1b1b1b1, 0x5b5b5b5b, 0x6a6a6a6a, 0xcbcbcbcb, 0xbebebebe, 0x39393939, 0x4a4a4a4a, 0x4c4c4c4c, 0x58585858, 0xcfcfcfcf, 0xd0d0d0d0, 0xefefefef, 0xaaaaaaaa, 0xfbfbfbfb, 0x43434343, 0x4d4d4d4d, 0x33333333, 0x85858585, 0x45454545, 0xf9f9f9f9, 0x02020202, 0x7f7f7f7f, 0x50505050, 0x3c3c3c3c, 0x9f9f9f9f,
                                0xa8a8a8a8, 0x51515151, 0xa3a3a3a3, 0x40404040, 0x8f8f8f8f, 0x92929292, 0x9d9d9d9d, 0x38383838, 0xf5f5f5f5, 0xbcbcbcbc, 0xb6b6b6b6, 0xdadadada, 0x21212121, 0x10101010, 0xffffffff, 0xf3f3f3f3, 0xd2d2d2d2,
                                0xcdcdcdcd, 0x0c0c0c0c, 0x13131313, 0xecececec, 0x5f5f5f5f, 0x97979797, 0x44444444, 0x17171717, 0xc4c4c4c4, 0xa7a7a7a7, 0x7e7e7e7e, 0x3d3d3d3d, 0x64646464, 0x5d5d5d5d, 0x19191919, 0x73737373, 0x60606060, 0x81818181, 0x4f4f4f4f, 0xdcdcdcdc, 0x22222222, 0x2a2a2a2a, 0x90909090, 0x88888888, 0x46464646, 0xeeeeeeee, 0xb8b8b8b8, 0x14141414, 0xdededede, 0x5e5e5e5e, 0x0b0b0b0b, 0xdbdbdbdb, 0xe0e0e0e0, 0x32323232, 0x3a3a3a3a, 0x0a0a0a0a, 0x49494949, 0x06060606, 0x24242424, 0x5c5c5c5c, 0xc2c2c2c2, 0xd3d3d3d3, 0xacacacac, 0x62626262, 0x91919191, 0x95959595, 0xe4e4e4e4,
                                0x79797979, 0xe7e7e7e7, 0xc8c8c8c8, 0x37373737, 0x6d6d6d6d, 0x8d8d8d8d, 0xd5d5d5d5, 0x4e4e4e4e, 0xa9a9a9a9, 0x6c6c6c6c, 0x56565656, 0xf4f4f4f4, 0xeaeaeaea, 0x65656565, 0x7a7a7a7a, 0xaeaeaeae, 0x08080808,
                                0xbabababa, 0x78787878, 0x25252525, 0x2e2e2e2e, 0x1c1c1c1c, 0xa6a6a6a6, 0xb4b4b4b4, 0xc6c6c6c6, 0xe8e8e8e8, 0xdddddddd, 0x74747474, 0x1f1f1f1f, 0x4b4b4b4b, 0xbdbdbdbd, 0x8b8b8b8b, 0x8a8a8a8a, 0x70707070, 0x3e3e3e3e, 0xb5b5b5b5, 0x66666666, 0x48484848, 0x03030303, 0xf6f6f6f6, 0x0e0e0e0e, 0x61616161, 0x35353535, 0x57575757, 0xb9b9b9b9, 0x86868686, 0xc1c1c1c1, 0x1d1d1d1d, 0x9e9e9e9e, 0xe1e1e1e1, 0xf8f8f8f8, 0x98989898, 0x11111111, 0x69696969, 0xd9d9d9d9, 0x8e8e8e8e, 0x94949494, 0x9b9b9b9b, 0x1e1e1e1e, 0x87878787, 0xe9e9e9e9, 0xcececece, 0x55555555, 0x28282828,
                                0xdfdfdfdf, 0x8c8c8c8c, 0xa1a1a1a1, 0x89898989, 0x0d0d0d0d, 0xbfbfbfbf, 0xe6e6e6e6, 0x42424242, 0x68686868, 0x41414141, 0x99999999, 0x2d2d2d2d, 0x0f0f0f0f, 0xb0b0b0b0, 0x54545454, 0xbbbbbbbb, 0x16161616};
const uint32_t AES::Td0[256] = {0x51f4a750, 0x7e416553, 0x1a17a4c3, 0x3a275e96, 0x3bab6bcb, 0x1f9d45f1, 0xacfa58ab, 0x4be30393, 0x2030fa55, 0xad766df6, 0x88cc7691, 0xf5024c25, 0x4fe5d7fc, 0xc52acbd7, 0x26354480, 0xb562a38f, 0xdeb15a49, 0x25ba1b67, 0x45ea0e98, 0x5dfec0e1, 0xc32f7502, 0x814cf012, 0x8d4697a3, 0x6bd3f9c6, 0x038f5fe7, 0x15929c95, 0xbf6d7aeb, 0x955259da, 0xd4be832d, 0x587421d3, 0x49e06929, 0x8ec9c844, 0x75c2896a, 0xf48e7978, 0x99583e6b, 0x27b971dd, 0xbee14fb6, 0xf088ad17, 0xc920ac66, 0x7dce3ab4, 0x63df4a18, 0xe51a3182, 0x97513360, 0x62537f45, 0xb16477e0, 0xbb6bae84, 0xfe81a01c,
                                0xf9082b94, 0x70486858, 0x8f45fd19, 0x94de6c87, 0x527bf8b7, 0xab73d323, 0x724b02e2, 0xe31f8f57, 0x6655ab2a, 0xb2eb2807, 0x2fb5c203, 0x86c57b9a, 0xd33708a5, 0x302887f2, 0x23bfa5b2, 0x02036aba, 0xed16825c,
                                0x8acf1c2b, 0xa779b492, 0xf307f2f0, 0x4e69e2a1, 0x65daf4cd, 0x0605bed5, 0xd134621f, 0xc4a6fe8a, 0x342e539d, 0xa2f355a0, 0x058ae132, 0xa4f6eb75, 0x0b83ec39, 0x4060efaa, 0x5e719f06, 0xbd6e1051, 0x3e218af9, 0x96dd063d, 0xdd3e05ae, 0x4de6bd46, 0x91548db5, 0x71c45d05, 0x0406d46f, 0x605015ff, 0x1998fb24, 0xd6bde997, 0x894043cc, 0x67d99e77, 0xb0e842bd, 0x07898b88, 0xe7195b38, 0x79c8eedb, 0xa17c0a47, 0x7c420fe9, 0xf8841ec9, 0x00000000, 0x09808683, 0x322bed48, 0x1e1170ac, 0x6c5a724e, 0xfd0efffb, 0x0f853856, 0x3daed51e, 0x362d3927, 0x0a0fd964, 0x685ca621, 0x9b5b54d1,
                                0x24362e3a, 0x0c0a67b1, 0x9357e70f, 0xb4ee96d2, 0x1b9b919e, 0x80c0c54f, 0x61dc20a2, 0x5a774b69, 0x1c121a16, 0xe293ba0a, 0xc0a02ae5, 0x3c22e043, 0x121b171d, 0x0e090d0b, 0xf28bc7ad, 0x2db6a8b9, 0x141ea9c8,
                                0x57f11985, 0xaf75074c, 0xee99ddbb, 0xa37f60fd, 0xf701269f, 0x5c72f5bc, 0x44663bc5, 0x5bfb7e34, 0x8b432976, 0xcb23c6dc, 0xb6edfc68, 0xb8e4f163, 0xd731dcca, 0x42638510, 0x13972240, 0x84c61120, 0x854a247d, 0xd2bb3df8, 0xaef93211, 0xc729a16d, 0x1d9e2f4b, 0xdcb230f3, 0x0d8652ec, 0x77c1e3d0, 0x2bb3166c, 0xa970b999, 0x119448fa, 0x47e96422, 0xa8fc8cc4, 0xa0f03f1a, 0x567d2cd8, 0x223390ef, 0x87494ec7, 0xd938d1c1, 0x8ccaa2fe, 0x98d40b36, 0xa6f581cf, 0xa57ade28, 0xdab78e26, 0x3fadbfa4, 0x2c3a9de4, 0x5078920d, 0x6a5fcc9b, 0x547e4662, 0xf68d13c2, 0x90d8b8e8, 0x2e39f75e,
                                0x82c3aff5, 0x9f5d80be, 0x69d0937c, 0x6fd52da9, 0xcf2512b3, 0xc8ac993b, 0x10187da7, 0xe89c636e, 0xdb3bbb7b, 0xcd267809, 0x6e5918f4, 0xec9ab701, 0x834f9aa8, 0xe6956e65, 0xaaffe67e, 0x21bccf08, 0xef15e8e6,
                                0xbae79bd9, 0x4a6f36ce, 0xea9f09d4, 0x29b07cd6, 0x31a4b2af, 0x2a3f2331, 0xc6a59430, 0x35a266c0, 0x744ebc37, 0xfc82caa6, 0xe090d0b0, 0x33a7d815, 0xf104984a, 0x41ecdaf7, 0x7fcd500e, 0x1791f62f, 0x764dd68d, 0x43efb04d, 0xccaa4d54, 0xe49604df, 0x9ed1b5e3, 0x4c6a881b, 0xc12c1fb8, 0x4665517f, 0x9d5eea04, 0x018c355d, 0xfa877473, 0xfb0b412e, 0xb3671d5a, 0x92dbd252, 0xe9105633, 0x6dd64713, 0x9ad7618c, 0x37a10c7a, 0x59f8148e, 0xeb133c89, 0xcea927ee, 0xb761c935, 0xe11ce5ed, 0x7a47b13c, 0x9cd2df59, 0x55f2733f, 0x1814ce79, 0x73c737bf, 0x53f7cdea, 0x5ffdaa5b, 0xdf3d6f14,
                                0x7844db86, 0xcaaff381, 0xb968c43e, 0x3824342c, 0xc2a3405f, 0x161dc372, 0xbce2250c, 0x283c498b, 0xff0d9541, 0x39a80171, 0x080cb3de, 0xd8b4e49c, 0x6456c190, 0x7bcb8461, 0xd532b670, 0x486c5c74, 0xd0b85742};
const uint8_t AES::Td4[256] = {0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb, 0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb, 0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e, 0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25, 0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92, 0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d,
                               0x84, 0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06, 0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
                               0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73, 0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e, 0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b, 0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4, 0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f, 0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c,
                               0xef, 0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61, 0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d};
const uint32_t AES::rcon[15] = {0x01000000, 0x02000000, 0x04000000, 0x08000000, 0x10000000, 0x20000000, 0x40000000, 0x80000000, 0x1B000000, 0x36000000, 0x6c000000, 0xd8000000, 0xab000000, 0x4d000000, 0x9a000000};

void AES::_initSW(const uint8_t key[32]) noexcept
{
	uint32_t *rk = _k.sw.ek;

	rk[0] = Utils::loadBigEndian< uint32_t >(key);
	rk[1] = Utils::loadBigEndian< uint32_t >(key + 4);
	rk[2] = Utils::loadBigEndian< uint32_t >(key + 8);
	rk[3] = Utils::loadBigEndian< uint32_t >(key + 12);
	rk[4] = Utils::loadBigEndian< uint32_t >(key + 16);
	rk[5] = Utils::loadBigEndian< uint32_t >(key + 20);
	rk[6] = Utils::loadBigEndian< uint32_t >(key + 24);
	rk[7] = Utils::loadBigEndian< uint32_t >(key + 28);
	for (int i = 0;;) {
		uint32_t temp = rk[7];
		rk[8] = rk[0] ^ (Te2_r((temp >> 16U) & 0xffU) & 0xff000000U) ^ (Te3_r((temp >> 8U) & 0xffU) & 0x00ff0000U) ^ (Te0[(temp) & 0xffU] & 0x0000ff00U) ^ (Te1_r(temp >> 24U) & 0x000000ffU) ^ rcon[i];
		rk[9] = rk[1] ^ rk[8];
		rk[10] = rk[2] ^ rk[9];
		rk[11] = rk[3] ^ rk[10];
		if (++i == 7)
			break;
		temp = rk[11];
		rk[12] = rk[4] ^ (Te2_r(temp >> 24U) & 0xff000000U) ^ (Te3_r((temp >> 16U) & 0xffU) & 0x00ff0000U) ^ (Te0[(temp >> 8U) & 0xffU] & 0x0000ff00U) ^ (Te1_r((temp) & 0xffU) & 0x000000ffU);
		rk[13] = rk[5] ^ rk[12];
		rk[14] = rk[6] ^ rk[13];
		rk[15] = rk[7] ^ rk[14];
		rk += 8;
	}

	_encryptSW((const uint8_t *)Utils::ZERO256, (uint8_t *)_k.sw.h);
	_k.sw.h[0] = Utils::ntoh(_k.sw.h[0]);
	_k.sw.h[1] = Utils::ntoh(_k.sw.h[1]);

	for (int i = 0; i < 60; ++i)
		_k.sw.dk[i] = _k.sw.ek[i];
	rk = _k.sw.dk;

	for (int i = 0, j = 56; i < j; i += 4, j -= 4) {
		uint32_t temp = rk[i];
		rk[i] = rk[j];
		rk[j] = temp;
		temp = rk[i + 1];
		rk[i + 1] = rk[j + 1];
		rk[j + 1] = temp;
		temp = rk[i + 2];
		rk[i + 2] = rk[j + 2];
		rk[j + 2] = temp;
		temp = rk[i + 3];
		rk[i + 3] = rk[j + 3];
		rk[j + 3] = temp;
	}
	for (int i = 1; i < 14; ++i) {
		rk += 4;
		rk[0] = Td0[Te4[(rk[0] >> 24U)] & 0xffU] ^ Td1_r(Te4[(rk[0] >> 16U) & 0xffU] & 0xffU) ^ Td2_r(Te4[(rk[0] >> 8U) & 0xffU] & 0xffU) ^ Td3_r(Te4[(rk[0]) & 0xffU] & 0xffU);
		rk[1] = Td0[Te4[(rk[1] >> 24U)] & 0xffU] ^ Td1_r(Te4[(rk[1] >> 16U) & 0xffU] & 0xffU) ^ Td2_r(Te4[(rk[1] >> 8U) & 0xffU] & 0xffU) ^ Td3_r(Te4[(rk[1]) & 0xffU] & 0xffU);
		rk[2] = Td0[Te4[(rk[2] >> 24U)] & 0xffU] ^ Td1_r(Te4[(rk[2] >> 16U) & 0xffU] & 0xffU) ^ Td2_r(Te4[(rk[2] >> 8U) & 0xffU] & 0xffU) ^ Td3_r(Te4[(rk[2]) & 0xffU] & 0xffU);
		rk[3] = Td0[Te4[(rk[3] >> 24U)] & 0xffU] ^ Td1_r(Te4[(rk[3] >> 16U) & 0xffU] & 0xffU) ^ Td2_r(Te4[(rk[3] >> 8U) & 0xffU] & 0xffU) ^ Td3_r(Te4[(rk[3]) & 0xffU] & 0xffU);
	}
}

void AES::_encryptSW(const uint8_t in[16], uint8_t out[16]) const noexcept
{
	const uint32_t *const restrict rk = _k.sw.ek;
	const uint32_t m8 = 0xff;
	const uint32_t m8_24 = 0xff000000;
	const uint32_t m8_16 = 0x00ff0000;
	const uint32_t m8_8 = 0x0000ff00;
	uint32_t s0, s1, s2, s3;
	s0 = Utils::loadBigEndian< uint32_t >(in) ^ rk[0];
	s1 = Utils::loadBigEndian< uint32_t >(in + 4) ^ rk[1];
	s2 = Utils::loadBigEndian< uint32_t >(in + 8) ^ rk[2];
	s3 = Utils::loadBigEndian< uint32_t >(in + 12) ^ rk[3];
	uint32_t t0, t1, t2, t3;
	t0 = Te0[s0 >> 24U] ^ Te1_r((s1 >> 16U) & m8) ^ Te2_r((s2 >> 8U) & m8) ^ Te3_r(s3 & m8) ^ rk[4];
	t1 = Te0[s1 >> 24U] ^ Te1_r((s2 >> 16U) & m8) ^ Te2_r((s3 >> 8U) & m8) ^ Te3_r(s0 & m8) ^ rk[5];
	t2 = Te0[s2 >> 24U] ^ Te1_r((s3 >> 16U) & m8) ^ Te2_r((s0 >> 8U) & m8) ^ Te3_r(s1 & m8) ^ rk[6];
	t3 = Te0[s3 >> 24U] ^ Te1_r((s0 >> 16U) & m8) ^ Te2_r((s1 >> 8U) & m8) ^ Te3_r(s2 & m8) ^ rk[7];
	s0 = Te0[t0 >> 24U] ^ Te1_r((t1 >> 16U) & m8) ^ Te2_r((t2 >> 8U) & m8) ^ Te3_r(t3 & m8) ^ rk[8];
	s1 = Te0[t1 >> 24U] ^ Te1_r((t2 >> 16U) & m8) ^ Te2_r((t3 >> 8U) & m8) ^ Te3_r(t0 & m8) ^ rk[9];
	s2 = Te0[t2 >> 24U] ^ Te1_r((t3 >> 16U) & m8) ^ Te2_r((t0 >> 8U) & m8) ^ Te3_r(t1 & m8) ^ rk[10];
	s3 = Te0[t3 >> 24U] ^ Te1_r((t0 >> 16U) & m8) ^ Te2_r((t1 >> 8U) & m8) ^ Te3_r(t2 & m8) ^ rk[11];
	t0 = Te0[s0 >> 24U] ^ Te1_r((s1 >> 16U) & m8) ^ Te2_r((s2 >> 8U) & m8) ^ Te3_r(s3 & m8) ^ rk[12];
	t1 = Te0[s1 >> 24U] ^ Te1_r((s2 >> 16U) & m8) ^ Te2_r((s3 >> 8U) & m8) ^ Te3_r(s0 & m8) ^ rk[13];
	t2 = Te0[s2 >> 24U] ^ Te1_r((s3 >> 16U) & m8) ^ Te2_r((s0 >> 8U) & m8) ^ Te3_r(s1 & m8) ^ rk[14];
	t3 = Te0[s3 >> 24U] ^ Te1_r((s0 >> 16U) & m8) ^ Te2_r((s1 >> 8U) & m8) ^ Te3_r(s2 & m8) ^ rk[15];
	s0 = Te0[t0 >> 24U] ^ Te1_r((t1 >> 16U) & m8) ^ Te2_r((t2 >> 8U) & m8) ^ Te3_r(t3 & m8) ^ rk[16];
	s1 = Te0[t1 >> 24U] ^ Te1_r((t2 >> 16U) & m8) ^ Te2_r((t3 >> 8U) & m8) ^ Te3_r(t0 & m8) ^ rk[17];
	s2 = Te0[t2 >> 24U] ^ Te1_r((t3 >> 16U) & m8) ^ Te2_r((t0 >> 8U) & m8) ^ Te3_r(t1 & m8) ^ rk[18];
	s3 = Te0[t3 >> 24U] ^ Te1_r((t0 >> 16U) & m8) ^ Te2_r((t1 >> 8U) & m8) ^ Te3_r(t2 & m8) ^ rk[19];
	t0 = Te0[s0 >> 24U] ^ Te1_r((s1 >> 16U) & m8) ^ Te2_r((s2 >> 8U) & m8) ^ Te3_r(s3 & m8) ^ rk[20];
	t1 = Te0[s1 >> 24U] ^ Te1_r((s2 >> 16U) & m8) ^ Te2_r((s3 >> 8U) & m8) ^ Te3_r(s0 & m8) ^ rk[21];
	t2 = Te0[s2 >> 24U] ^ Te1_r((s3 >> 16U) & m8) ^ Te2_r((s0 >> 8U) & m8) ^ Te3_r(s1 & m8) ^ rk[22];
	t3 = Te0[s3 >> 24U] ^ Te1_r((s0 >> 16U) & m8) ^ Te2_r((s1 >> 8U) & m8) ^ Te3_r(s2 & m8) ^ rk[23];
	s0 = Te0[t0 >> 24U] ^ Te1_r((t1 >> 16U) & m8) ^ Te2_r((t2 >> 8U) & m8) ^ Te3_r(t3 & m8) ^ rk[24];
	s1 = Te0[t1 >> 24U] ^ Te1_r((t2 >> 16U) & m8) ^ Te2_r((t3 >> 8U) & m8) ^ Te3_r(t0 & m8) ^ rk[25];
	s2 = Te0[t2 >> 24U] ^ Te1_r((t3 >> 16U) & m8) ^ Te2_r((t0 >> 8U) & m8) ^ Te3_r(t1 & m8) ^ rk[26];
	s3 = Te0[t3 >> 24U] ^ Te1_r((t0 >> 16U) & m8) ^ Te2_r((t1 >> 8U) & m8) ^ Te3_r(t2 & m8) ^ rk[27];
	t0 = Te0[s0 >> 24U] ^ Te1_r((s1 >> 16U) & m8) ^ Te2_r((s2 >> 8U) & m8) ^ Te3_r(s3 & m8) ^ rk[28];
	t1 = Te0[s1 >> 24U] ^ Te1_r((s2 >> 16U) & m8) ^ Te2_r((s3 >> 8U) & m8) ^ Te3_r(s0 & m8) ^ rk[29];
	t2 = Te0[s2 >> 24U] ^ Te1_r((s3 >> 16U) & m8) ^ Te2_r((s0 >> 8U) & m8) ^ Te3_r(s1 & m8) ^ rk[30];
	t3 = Te0[s3 >> 24U] ^ Te1_r((s0 >> 16U) & m8) ^ Te2_r((s1 >> 8U) & m8) ^ Te3_r(s2 & m8) ^ rk[31];
	s0 = Te0[t0 >> 24U] ^ Te1_r((t1 >> 16U) & m8) ^ Te2_r((t2 >> 8U) & m8) ^ Te3_r(t3 & m8) ^ rk[32];
	s1 = Te0[t1 >> 24U] ^ Te1_r((t2 >> 16U) & m8) ^ Te2_r((t3 >> 8U) & m8) ^ Te3_r(t0 & m8) ^ rk[33];
	s2 = Te0[t2 >> 24U] ^ Te1_r((t3 >> 16U) & m8) ^ Te2_r((t0 >> 8U) & m8) ^ Te3_r(t1 & m8) ^ rk[34];
	s3 = Te0[t3 >> 24U] ^ Te1_r((t0 >> 16U) & m8) ^ Te2_r((t1 >> 8U) & m8) ^ Te3_r(t2 & m8) ^ rk[35];
	t0 = Te0[s0 >> 24U] ^ Te1_r((s1 >> 16U) & m8) ^ Te2_r((s2 >> 8U) & m8) ^ Te3_r(s3 & m8) ^ rk[36];
	t1 = Te0[s1 >> 24U] ^ Te1_r((s2 >> 16U) & m8) ^ Te2_r((s3 >> 8U) & m8) ^ Te3_r(s0 & m8) ^ rk[37];
	t2 = Te0[s2 >> 24U] ^ Te1_r((s3 >> 16U) & m8) ^ Te2_r((s0 >> 8U) & m8) ^ Te3_r(s1 & m8) ^ rk[38];
	t3 = Te0[s3 >> 24U] ^ Te1_r((s0 >> 16U) & m8) ^ Te2_r((s1 >> 8U) & m8) ^ Te3_r(s2 & m8) ^ rk[39];
	s0 = Te0[t0 >> 24U] ^ Te1_r((t1 >> 16U) & m8) ^ Te2_r((t2 >> 8U) & m8) ^ Te3_r(t3 & m8) ^ rk[40];
	s1 = Te0[t1 >> 24U] ^ Te1_r((t2 >> 16U) & m8) ^ Te2_r((t3 >> 8U) & m8) ^ Te3_r(t0 & m8) ^ rk[41];
	s2 = Te0[t2 >> 24U] ^ Te1_r((t3 >> 16U) & m8) ^ Te2_r((t0 >> 8U) & m8) ^ Te3_r(t1 & m8) ^ rk[42];
	s3 = Te0[t3 >> 24U] ^ Te1_r((t0 >> 16U) & m8) ^ Te2_r((t1 >> 8U) & m8) ^ Te3_r(t2 & m8) ^ rk[43];
	t0 = Te0[s0 >> 24U] ^ Te1_r((s1 >> 16U) & m8) ^ Te2_r((s2 >> 8U) & m8) ^ Te3_r(s3 & m8) ^ rk[44];
	t1 = Te0[s1 >> 24U] ^ Te1_r((s2 >> 16U) & m8) ^ Te2_r((s3 >> 8U) & m8) ^ Te3_r(s0 & m8) ^ rk[45];
	t2 = Te0[s2 >> 24U] ^ Te1_r((s3 >> 16U) & m8) ^ Te2_r((s0 >> 8U) & m8) ^ Te3_r(s1 & m8) ^ rk[46];
	t3 = Te0[s3 >> 24U] ^ Te1_r((s0 >> 16U) & m8) ^ Te2_r((s1 >> 8U) & m8) ^ Te3_r(s2 & m8) ^ rk[47];
	s0 = Te0[t0 >> 24U] ^ Te1_r((t1 >> 16U) & m8) ^ Te2_r((t2 >> 8U) & m8) ^ Te3_r(t3 & m8) ^ rk[48];
	s1 = Te0[t1 >> 24U] ^ Te1_r((t2 >> 16U) & m8) ^ Te2_r((t3 >> 8U) & m8) ^ Te3_r(t0 & m8) ^ rk[49];
	s2 = Te0[t2 >> 24U] ^ Te1_r((t3 >> 16U) & m8) ^ Te2_r((t0 >> 8U) & m8) ^ Te3_r(t1 & m8) ^ rk[50];
	s3 = Te0[t3 >> 24U] ^ Te1_r((t0 >> 16U) & m8) ^ Te2_r((t1 >> 8U) & m8) ^ Te3_r(t2 & m8) ^ rk[51];
	t0 = Te0[s0 >> 24U] ^ Te1_r((s1 >> 16U) & m8) ^ Te2_r((s2 >> 8U) & m8) ^ Te3_r(s3 & m8) ^ rk[52];
	t1 = Te0[s1 >> 24U] ^ Te1_r((s2 >> 16U) & m8) ^ Te2_r((s3 >> 8U) & m8) ^ Te3_r(s0 & m8) ^ rk[53];
	t2 = Te0[s2 >> 24U] ^ Te1_r((s3 >> 16U) & m8) ^ Te2_r((s0 >> 8U) & m8) ^ Te3_r(s1 & m8) ^ rk[54];
	t3 = Te0[s3 >> 24U] ^ Te1_r((s0 >> 16U) & m8) ^ Te2_r((s1 >> 8U) & m8) ^ Te3_r(s2 & m8) ^ rk[55];
	s0 = (Te2_r(t0 >> 24U) & m8_24) ^ (Te3_r((t1 >> 16U) & m8) & m8_16) ^ (Te0[(t2 >> 8U) & m8] & m8_8) ^ (Te1_r(t3 & m8) & m8) ^ rk[56];
	s1 = (Te2_r(t1 >> 24U) & m8_24) ^ (Te3_r((t2 >> 16U) & m8) & m8_16) ^ (Te0[(t3 >> 8U) & m8] & m8_8) ^ (Te1_r(t0 & m8) & m8) ^ rk[57];
	s2 = (Te2_r(t2 >> 24U) & m8_24) ^ (Te3_r((t3 >> 16U) & m8) & m8_16) ^ (Te0[(t0 >> 8U) & m8] & m8_8) ^ (Te1_r(t1 & m8) & m8) ^ rk[58];
	s3 = (Te2_r(t3 >> 24U) & m8_24) ^ (Te3_r((t0 >> 16U) & m8) & m8_16) ^ (Te0[(t1 >> 8U) & m8] & m8_8) ^ (Te1_r(t2 & m8) & m8) ^ rk[59];
	Utils::storeBigEndian< uint32_t >(out, s0);
	Utils::storeBigEndian< uint32_t >(out + 4, s1);
	Utils::storeBigEndian< uint32_t >(out + 8, s2);
	Utils::storeBigEndian< uint32_t >(out + 12, s3);
}

void AES::_decryptSW(const uint8_t in[16], uint8_t out[16]) const noexcept
{
	const uint32_t *restrict rk = _k.sw.dk;
	uint32_t s0, s1, s2, s3, t0, t1, t2, t3;
	const uint32_t m8 = 0xff;
	s0 = Utils::loadBigEndian< uint32_t >(in) ^ rk[0];
	s1 = Utils::loadBigEndian< uint32_t >(in + 4) ^ rk[1];
	s2 = Utils::loadBigEndian< uint32_t >(in + 8) ^ rk[2];
	s3 = Utils::loadBigEndian< uint32_t >(in + 12) ^ rk[3];
	t0 = Td0[s0 >> 24U] ^ Td1_r((s3 >> 16U) & m8) ^ Td2_r((s2 >> 8U) & m8) ^ Td3_r(s1 & m8) ^ rk[4];
	t1 = Td0[s1 >> 24U] ^ Td1_r((s0 >> 16U) & m8) ^ Td2_r((s3 >> 8U) & m8) ^ Td3_r(s2 & m8) ^ rk[5];
	t2 = Td0[s2 >> 24U] ^ Td1_r((s1 >> 16U) & m8) ^ Td2_r((s0 >> 8U) & m8) ^ Td3_r(s3 & m8) ^ rk[6];
	t3 = Td0[s3 >> 24U] ^ Td1_r((s2 >> 16U) & m8) ^ Td2_r((s1 >> 8U) & m8) ^ Td3_r(s0 & m8) ^ rk[7];
	s0 = Td0[t0 >> 24U] ^ Td1_r((t3 >> 16U) & m8) ^ Td2_r((t2 >> 8U) & m8) ^ Td3_r(t1 & m8) ^ rk[8];
	s1 = Td0[t1 >> 24U] ^ Td1_r((t0 >> 16U) & m8) ^ Td2_r((t3 >> 8U) & m8) ^ Td3_r(t2 & m8) ^ rk[9];
	s2 = Td0[t2 >> 24U] ^ Td1_r((t1 >> 16U) & m8) ^ Td2_r((t0 >> 8U) & m8) ^ Td3_r(t3 & m8) ^ rk[10];
	s3 = Td0[t3 >> 24U] ^ Td1_r((t2 >> 16U) & m8) ^ Td2_r((t1 >> 8U) & m8) ^ Td3_r(t0 & m8) ^ rk[11];
	t0 = Td0[s0 >> 24U] ^ Td1_r((s3 >> 16U) & m8) ^ Td2_r((s2 >> 8U) & m8) ^ Td3_r(s1 & m8) ^ rk[12];
	t1 = Td0[s1 >> 24U] ^ Td1_r((s0 >> 16U) & m8) ^ Td2_r((s3 >> 8U) & m8) ^ Td3_r(s2 & m8) ^ rk[13];
	t2 = Td0[s2 >> 24U] ^ Td1_r((s1 >> 16U) & m8) ^ Td2_r((s0 >> 8U) & m8) ^ Td3_r(s3 & m8) ^ rk[14];
	t3 = Td0[s3 >> 24U] ^ Td1_r((s2 >> 16U) & m8) ^ Td2_r((s1 >> 8U) & m8) ^ Td3_r(s0 & m8) ^ rk[15];
	s0 = Td0[t0 >> 24U] ^ Td1_r((t3 >> 16U) & m8) ^ Td2_r((t2 >> 8U) & m8) ^ Td3_r(t1 & m8) ^ rk[16];
	s1 = Td0[t1 >> 24U] ^ Td1_r((t0 >> 16U) & m8) ^ Td2_r((t3 >> 8U) & m8) ^ Td3_r(t2 & m8) ^ rk[17];
	s2 = Td0[t2 >> 24U] ^ Td1_r((t1 >> 16U) & m8) ^ Td2_r((t0 >> 8U) & m8) ^ Td3_r(t3 & m8) ^ rk[18];
	s3 = Td0[t3 >> 24U] ^ Td1_r((t2 >> 16U) & m8) ^ Td2_r((t1 >> 8U) & m8) ^ Td3_r(t0 & m8) ^ rk[19];
	t0 = Td0[s0 >> 24U] ^ Td1_r((s3 >> 16U) & m8) ^ Td2_r((s2 >> 8U) & m8) ^ Td3_r(s1 & m8) ^ rk[20];
	t1 = Td0[s1 >> 24U] ^ Td1_r((s0 >> 16U) & m8) ^ Td2_r((s3 >> 8U) & m8) ^ Td3_r(s2 & m8) ^ rk[21];
	t2 = Td0[s2 >> 24U] ^ Td1_r((s1 >> 16U) & m8) ^ Td2_r((s0 >> 8U) & m8) ^ Td3_r(s3 & m8) ^ rk[22];
	t3 = Td0[s3 >> 24U] ^ Td1_r((s2 >> 16U) & m8) ^ Td2_r((s1 >> 8U) & m8) ^ Td3_r(s0 & m8) ^ rk[23];
	s0 = Td0[t0 >> 24U] ^ Td1_r((t3 >> 16U) & m8) ^ Td2_r((t2 >> 8U) & m8) ^ Td3_r(t1 & m8) ^ rk[24];
	s1 = Td0[t1 >> 24U] ^ Td1_r((t0 >> 16U) & m8) ^ Td2_r((t3 >> 8U) & m8) ^ Td3_r(t2 & m8) ^ rk[25];
	s2 = Td0[t2 >> 24U] ^ Td1_r((t1 >> 16U) & m8) ^ Td2_r((t0 >> 8U) & m8) ^ Td3_r(t3 & m8) ^ rk[26];
	s3 = Td0[t3 >> 24U] ^ Td1_r((t2 >> 16U) & m8) ^ Td2_r((t1 >> 8U) & m8) ^ Td3_r(t0 & m8) ^ rk[27];
	t0 = Td0[s0 >> 24U] ^ Td1_r((s3 >> 16U) & m8) ^ Td2_r((s2 >> 8U) & m8) ^ Td3_r(s1 & m8) ^ rk[28];
	t1 = Td0[s1 >> 24U] ^ Td1_r((s0 >> 16U) & m8) ^ Td2_r((s3 >> 8U) & m8) ^ Td3_r(s2 & m8) ^ rk[29];
	t2 = Td0[s2 >> 24U] ^ Td1_r((s1 >> 16U) & m8) ^ Td2_r((s0 >> 8U) & m8) ^ Td3_r(s3 & m8) ^ rk[30];
	t3 = Td0[s3 >> 24U] ^ Td1_r((s2 >> 16U) & m8) ^ Td2_r((s1 >> 8U) & m8) ^ Td3_r(s0 & m8) ^ rk[31];
	s0 = Td0[t0 >> 24U] ^ Td1_r((t3 >> 16U) & m8) ^ Td2_r((t2 >> 8U) & m8) ^ Td3_r(t1 & m8) ^ rk[32];
	s1 = Td0[t1 >> 24U] ^ Td1_r((t0 >> 16U) & m8) ^ Td2_r((t3 >> 8U) & m8) ^ Td3_r(t2 & m8) ^ rk[33];
	s2 = Td0[t2 >> 24U] ^ Td1_r((t1 >> 16U) & m8) ^ Td2_r((t0 >> 8U) & m8) ^ Td3_r(t3 & m8) ^ rk[34];
	s3 = Td0[t3 >> 24U] ^ Td1_r((t2 >> 16U) & m8) ^ Td2_r((t1 >> 8U) & m8) ^ Td3_r(t0 & m8) ^ rk[35];
	t0 = Td0[s0 >> 24U] ^ Td1_r((s3 >> 16U) & m8) ^ Td2_r((s2 >> 8U) & m8) ^ Td3_r(s1 & m8) ^ rk[36];
	t1 = Td0[s1 >> 24U] ^ Td1_r((s0 >> 16U) & m8) ^ Td2_r((s3 >> 8U) & m8) ^ Td3_r(s2 & m8) ^ rk[37];
	t2 = Td0[s2 >> 24U] ^ Td1_r((s1 >> 16U) & m8) ^ Td2_r((s0 >> 8U) & m8) ^ Td3_r(s3 & m8) ^ rk[38];
	t3 = Td0[s3 >> 24U] ^ Td1_r((s2 >> 16U) & m8) ^ Td2_r((s1 >> 8U) & m8) ^ Td3_r(s0 & m8) ^ rk[39];
	s0 = Td0[t0 >> 24U] ^ Td1_r((t3 >> 16U) & m8) ^ Td2_r((t2 >> 8U) & m8) ^ Td3_r(t1 & m8) ^ rk[40];
	s1 = Td0[t1 >> 24U] ^ Td1_r((t0 >> 16U) & m8) ^ Td2_r((t3 >> 8U) & m8) ^ Td3_r(t2 & m8) ^ rk[41];
	s2 = Td0[t2 >> 24U] ^ Td1_r((t1 >> 16U) & m8) ^ Td2_r((t0 >> 8U) & m8) ^ Td3_r(t3 & m8) ^ rk[42];
	s3 = Td0[t3 >> 24U] ^ Td1_r((t2 >> 16U) & m8) ^ Td2_r((t1 >> 8U) & m8) ^ Td3_r(t0 & m8) ^ rk[43];
	t0 = Td0[s0 >> 24U] ^ Td1_r((s3 >> 16U) & m8) ^ Td2_r((s2 >> 8U) & m8) ^ Td3_r(s1 & m8) ^ rk[44];
	t1 = Td0[s1 >> 24U] ^ Td1_r((s0 >> 16U) & m8) ^ Td2_r((s3 >> 8U) & m8) ^ Td3_r(s2 & m8) ^ rk[45];
	t2 = Td0[s2 >> 24U] ^ Td1_r((s1 >> 16U) & m8) ^ Td2_r((s0 >> 8U) & m8) ^ Td3_r(s3 & m8) ^ rk[46];
	t3 = Td0[s3 >> 24U] ^ Td1_r((s2 >> 16U) & m8) ^ Td2_r((s1 >> 8U) & m8) ^ Td3_r(s0 & m8) ^ rk[47];
	s0 = Td0[t0 >> 24U] ^ Td1_r((t3 >> 16U) & m8) ^ Td2_r((t2 >> 8U) & m8) ^ Td3_r(t1 & m8) ^ rk[48];
	s1 = Td0[t1 >> 24U] ^ Td1_r((t0 >> 16U) & m8) ^ Td2_r((t3 >> 8U) & m8) ^ Td3_r(t2 & m8) ^ rk[49];
	s2 = Td0[t2 >> 24U] ^ Td1_r((t1 >> 16U) & m8) ^ Td2_r((t0 >> 8U) & m8) ^ Td3_r(t3 & m8) ^ rk[50];
	s3 = Td0[t3 >> 24U] ^ Td1_r((t2 >> 16U) & m8) ^ Td2_r((t1 >> 8U) & m8) ^ Td3_r(t0 & m8) ^ rk[51];
	t0 = Td0[s0 >> 24U] ^ Td1_r((s3 >> 16U) & m8) ^ Td2_r((s2 >> 8U) & m8) ^ Td3_r(s1 & m8) ^ rk[52];
	t1 = Td0[s1 >> 24U] ^ Td1_r((s0 >> 16U) & m8) ^ Td2_r((s3 >> 8U) & m8) ^ Td3_r(s2 & m8) ^ rk[53];
	t2 = Td0[s2 >> 24U] ^ Td1_r((s1 >> 16U) & m8) ^ Td2_r((s0 >> 8U) & m8) ^ Td3_r(s3 & m8) ^ rk[54];
	t3 = Td0[s3 >> 24U] ^ Td1_r((s2 >> 16U) & m8) ^ Td2_r((s1 >> 8U) & m8) ^ Td3_r(s0 & m8) ^ rk[55];
	rk += 56;
	Utils::storeBigEndian< uint32_t >(out, (Td4[(t0 >> 24U)] << 24U) ^ (Td4[(t3 >> 16U) & m8] << 16U) ^ (Td4[(t2 >> 8U) & m8] << 8U) ^ (Td4[(t1) & m8]) ^ rk[0]);
	Utils::storeBigEndian< uint32_t >(out + 4, (Td4[(t1 >> 24U)] << 24U) ^ (Td4[(t0 >> 16U) & m8] << 16U) ^ (Td4[(t3 >> 8U) & m8] << 8U) ^ (Td4[(t2) & m8]) ^ rk[1]);
	Utils::storeBigEndian< uint32_t >(out + 8, (Td4[(t2 >> 24U)] << 24U) ^ (Td4[(t1 >> 16U) & m8] << 16U) ^ (Td4[(t0 >> 8U) & m8] << 8U) ^ (Td4[(t3) & m8]) ^ rk[2]);
	Utils::storeBigEndian< uint32_t >(out + 12, (Td4[(t3 >> 24U)] << 24U) ^ (Td4[(t2 >> 16U) & m8] << 16U) ^ (Td4[(t1 >> 8U) & m8] << 8U) ^ (Td4[(t0) & m8]) ^ rk[3]);
}

#ifdef ZT_AES_AESNI

static __m128i _init256_1_aesni(__m128i a, __m128i b) noexcept
{
	__m128i x, y;
	b = _mm_shuffle_epi32(b, 0xff);
	y = _mm_slli_si128(a, 0x04);
	x = _mm_xor_si128(a, y);
	y = _mm_slli_si128(y, 0x04);
	x = _mm_xor_si128(x, y);
	y = _mm_slli_si128(y, 0x04);
	x = _mm_xor_si128(x, y);
	x = _mm_xor_si128(x, b);
	return x;
}

static __m128i _init256_2_aesni(__m128i a, __m128i b) noexcept
{
	__m128i x, y, z;
	y = _mm_aeskeygenassist_si128(a, 0x00);
	z = _mm_shuffle_epi32(y, 0xaa);
	y = _mm_slli_si128(b, 0x04);
	x = _mm_xor_si128(b, y);
	y = _mm_slli_si128(y, 0x04);
	x = _mm_xor_si128(x, y);
	y = _mm_slli_si128(y, 0x04);
	x = _mm_xor_si128(x, y);
	x = _mm_xor_si128(x, z);
	return x;
}

void AES::_init_aesni(const uint8_t key[32]) noexcept
{
	__m128i t1, t2, k1, k2, k3, k4, k5, k6, k7, k8, k9, k10, k11, k12, k13;
	_k.ni.k[0] = t1 = _mm_loadu_si128((const __m128i *)key);
	_k.ni.k[1] = k1 = t2 = _mm_loadu_si128((const __m128i *)(key + 16));
	_k.ni.k[2] = k2 = t1 = _init256_1_aesni(t1, _mm_aeskeygenassist_si128(t2, 0x01));
	_k.ni.k[3] = k3 = t2 = _init256_2_aesni(t1, t2);
	_k.ni.k[4] = k4 = t1 = _init256_1_aesni(t1, _mm_aeskeygenassist_si128(t2, 0x02));
	_k.ni.k[5] = k5 = t2 = _init256_2_aesni(t1, t2);
	_k.ni.k[6] = k6 = t1 = _init256_1_aesni(t1, _mm_aeskeygenassist_si128(t2, 0x04));
	_k.ni.k[7] = k7 = t2 = _init256_2_aesni(t1, t2);
	_k.ni.k[8] = k8 = t1 = _init256_1_aesni(t1, _mm_aeskeygenassist_si128(t2, 0x08));
	_k.ni.k[9] = k9 = t2 = _init256_2_aesni(t1, t2);
	_k.ni.k[10] = k10 = t1 = _init256_1_aesni(t1, _mm_aeskeygenassist_si128(t2, 0x10));
	_k.ni.k[11] = k11 = t2 = _init256_2_aesni(t1, t2);
	_k.ni.k[12] = k12 = t1 = _init256_1_aesni(t1, _mm_aeskeygenassist_si128(t2, 0x20));
	_k.ni.k[13] = k13 = t2 = _init256_2_aesni(t1, t2);
	_k.ni.k[14] = _init256_1_aesni(t1, _mm_aeskeygenassist_si128(t2, 0x40));
	_k.ni.k[15] = _mm_aesimc_si128(k13);
	_k.ni.k[16] = _mm_aesimc_si128(k12);
	_k.ni.k[17] = _mm_aesimc_si128(k11);
	_k.ni.k[18] = _mm_aesimc_si128(k10);
	_k.ni.k[19] = _mm_aesimc_si128(k9);
	_k.ni.k[20] = _mm_aesimc_si128(k8);
	_k.ni.k[21] = _mm_aesimc_si128(k7);
	_k.ni.k[22] = _mm_aesimc_si128(k6);
	_k.ni.k[23] = _mm_aesimc_si128(k5);
	_k.ni.k[24] = _mm_aesimc_si128(k4);
	_k.ni.k[25] = _mm_aesimc_si128(k3);
	_k.ni.k[26] = _mm_aesimc_si128(k2);
	_k.ni.k[27] = _mm_aesimc_si128(k1);

	__m128i h = _k.ni.k[0]; // _mm_xor_si128(_mm_setzero_si128(),_k.ni.k[0]);
	h = _mm_aesenc_si128(h, k1);
	h = _mm_aesenc_si128(h, k2);
	h = _mm_aesenc_si128(h, k3);
	h = _mm_aesenc_si128(h, k4);
	h = _mm_aesenc_si128(h, k5);
	h = _mm_aesenc_si128(h, k6);
	h = _mm_aesenc_si128(h, k7);
	h = _mm_aesenc_si128(h, k8);
	h = _mm_aesenc_si128(h, k9);
	h = _mm_aesenc_si128(h, k10);
	h = _mm_aesenc_si128(h, k11);
	h = _mm_aesenc_si128(h, k12);
	h = _mm_aesenc_si128(h, k13);
	h = _mm_aesenclast_si128(h, _k.ni.k[14]);
	__m128i hswap = _mm_shuffle_epi8(h, s_sseSwapBytes);
	__m128i hh = p_gmacPCLMUL128(hswap, h);
	__m128i hhh = p_gmacPCLMUL128(hswap, hh);
	__m128i hhhh = p_gmacPCLMUL128(hswap, hhh);
	_k.ni.h[0] = hswap;
	_k.ni.h[1] = hh = _mm_shuffle_epi8(hh, s_sseSwapBytes);
	_k.ni.h[2] = hhh = _mm_shuffle_epi8(hhh, s_sseSwapBytes);
	_k.ni.h[3] = hhhh = _mm_shuffle_epi8(hhhh, s_sseSwapBytes);
	_k.ni.h2[0] = _mm_xor_si128(_mm_shuffle_epi32(hswap, 78), hswap);
	_k.ni.h2[1] = _mm_xor_si128(_mm_shuffle_epi32(hh, 78), hh);
	_k.ni.h2[2] = _mm_xor_si128(_mm_shuffle_epi32(hhh, 78), hhh);
	_k.ni.h2[3] = _mm_xor_si128(_mm_shuffle_epi32(hhhh, 78), hhhh);
}

void AES::_encrypt_aesni(const void *const in, void *const out) const noexcept
{
	__m128i tmp = _mm_loadu_si128((const __m128i *)in);
	tmp = _mm_xor_si128(tmp, _k.ni.k[0]);
	tmp = _mm_aesenc_si128(tmp, _k.ni.k[1]);
	tmp = _mm_aesenc_si128(tmp, _k.ni.k[2]);
	tmp = _mm_aesenc_si128(tmp, _k.ni.k[3]);
	tmp = _mm_aesenc_si128(tmp, _k.ni.k[4]);
	tmp = _mm_aesenc_si128(tmp, _k.ni.k[5]);
	tmp = _mm_aesenc_si128(tmp, _k.ni.k[6]);
	tmp = _mm_aesenc_si128(tmp, _k.ni.k[7]);
	tmp = _mm_aesenc_si128(tmp, _k.ni.k[8]);
	tmp = _mm_aesenc_si128(tmp, _k.ni.k[9]);
	tmp = _mm_aesenc_si128(tmp, _k.ni.k[10]);
	tmp = _mm_aesenc_si128(tmp, _k.ni.k[11]);
	tmp = _mm_aesenc_si128(tmp, _k.ni.k[12]);
	tmp = _mm_aesenc_si128(tmp, _k.ni.k[13]);
	_mm_storeu_si128((__m128i *)out, _mm_aesenclast_si128(tmp, _k.ni.k[14]));
}

void AES::_decrypt_aesni(const void *in, void *out) const noexcept
{
	__m128i tmp = _mm_loadu_si128((const __m128i *)in);
	tmp = _mm_xor_si128(tmp, _k.ni.k[14]);
	tmp = _mm_aesdec_si128(tmp, _k.ni.k[15]);
	tmp = _mm_aesdec_si128(tmp, _k.ni.k[16]);
	tmp = _mm_aesdec_si128(tmp, _k.ni.k[17]);
	tmp = _mm_aesdec_si128(tmp, _k.ni.k[18]);
	tmp = _mm_aesdec_si128(tmp, _k.ni.k[19]);
	tmp = _mm_aesdec_si128(tmp, _k.ni.k[20]);
	tmp = _mm_aesdec_si128(tmp, _k.ni.k[21]);
	tmp = _mm_aesdec_si128(tmp, _k.ni.k[22]);
	tmp = _mm_aesdec_si128(tmp, _k.ni.k[23]);
	tmp = _mm_aesdec_si128(tmp, _k.ni.k[24]);
	tmp = _mm_aesdec_si128(tmp, _k.ni.k[25]);
	tmp = _mm_aesdec_si128(tmp, _k.ni.k[26]);
	tmp = _mm_aesdec_si128(tmp, _k.ni.k[27]);
	_mm_storeu_si128((__m128i *)out, _mm_aesdeclast_si128(tmp, _k.ni.k[0]));
}

#endif // ZT_AES_AESNI

#ifdef ZT_AES_NEON

#define ZT_INIT_ARMNEON_CRYPTO_SUBWORD(w) ((uint32_t)s_sbox[w & 0xffU] + ((uint32_t)s_sbox[(w >> 8U) & 0xffU] << 8U) + ((uint32_t)s_sbox[(w >> 16U) & 0xffU] << 16U) + ((uint32_t)s_sbox[(w >> 24U) & 0xffU] << 24U))
#define ZT_INIT_ARMNEON_CRYPTO_ROTWORD(w) (((w) << 8U) | ((w) >> 24U))
#define ZT_INIT_ARMNEON_CRYPTO_NK 8
#define ZT_INIT_ARMNEON_CRYPTO_NB 4
#define ZT_INIT_ARMNEON_CRYPTO_NR 14

void AES::_init_armneon_crypto(const uint8_t key[32]) noexcept
{
	static const uint8_t s_sbox[256] = {0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76, 0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0, 0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15, 0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75, 0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84, 0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c,
	                                    0x58, 0xcf, 0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8, 0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2, 0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73, 0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb, 0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79, 0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea,
	                                    0x65, 0x7a, 0xae, 0x08, 0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a, 0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e, 0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf, 0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16};

	uint64_t h[2];
	uint32_t *const w = reinterpret_cast<uint32_t *>(_k.neon.ek);

	for (unsigned int i=0;i<ZT_INIT_ARMNEON_CRYPTO_NK;++i) {
		const unsigned int j = i * 4;
		w[i] = ((uint32_t)key[j] << 24U) | ((uint32_t)key[j + 1] << 16U) | ((uint32_t)key[j + 2] << 8U) | (uint32_t)key[j + 3];
	}

	for (unsigned int i=ZT_INIT_ARMNEON_CRYPTO_NK;i<(ZT_INIT_ARMNEON_CRYPTO_NB * (ZT_INIT_ARMNEON_CRYPTO_NR + 1));++i) {
		uint32_t t = w[i - 1];
		const unsigned int imod = i & (ZT_INIT_ARMNEON_CRYPTO_NK - 1);
		if (imod == 0) {
			t = ZT_INIT_ARMNEON_CRYPTO_SUBWORD(ZT_INIT_ARMNEON_CRYPTO_ROTWORD(t)) ^ rcon[(i - 1) / ZT_INIT_ARMNEON_CRYPTO_NK];
		} else if (imod == 4) {
			t = ZT_INIT_ARMNEON_CRYPTO_SUBWORD(t);
		}
		w[i] = w[i - ZT_INIT_ARMNEON_CRYPTO_NK] ^ t;
	}

	for (unsigned int i=0;i<(ZT_INIT_ARMNEON_CRYPTO_NB * (ZT_INIT_ARMNEON_CRYPTO_NR + 1));++i)
		w[i] = Utils::hton(w[i]);

	_k.neon.dk[0] = _k.neon.ek[14];
	for (int i=1;i<14;++i)
		_k.neon.dk[i] = vaesimcq_u8(_k.neon.ek[14 - i]);
	_k.neon.dk[14] = _k.neon.ek[0];

	_encrypt_armneon_crypto(Utils::ZERO256, h);
	Utils::copy<16>(&(_k.neon.h), h);
	_k.neon.h = vrbitq_u8(_k.neon.h);
	_k.sw.h[0] = Utils::ntoh(h[0]);
	_k.sw.h[1] = Utils::ntoh(h[1]);
}

void AES::_encrypt_armneon_crypto(const void *const in, void *const out) const noexcept
{
	uint8x16_t tmp = vld1q_u8(reinterpret_cast<const uint8_t *>(in));
	tmp = vaesmcq_u8(vaeseq_u8(tmp, _k.neon.ek[0]));
	tmp = vaesmcq_u8(vaeseq_u8(tmp, _k.neon.ek[1]));
	tmp = vaesmcq_u8(vaeseq_u8(tmp, _k.neon.ek[2]));
	tmp = vaesmcq_u8(vaeseq_u8(tmp, _k.neon.ek[3]));
	tmp = vaesmcq_u8(vaeseq_u8(tmp, _k.neon.ek[4]));
	tmp = vaesmcq_u8(vaeseq_u8(tmp, _k.neon.ek[5]));
	tmp = vaesmcq_u8(vaeseq_u8(tmp, _k.neon.ek[6]));
	tmp = vaesmcq_u8(vaeseq_u8(tmp, _k.neon.ek[7]));
	tmp = vaesmcq_u8(vaeseq_u8(tmp, _k.neon.ek[8]));
	tmp = vaesmcq_u8(vaeseq_u8(tmp, _k.neon.ek[9]));
	tmp = vaesmcq_u8(vaeseq_u8(tmp, _k.neon.ek[10]));
	tmp = vaesmcq_u8(vaeseq_u8(tmp, _k.neon.ek[11]));
	tmp = vaesmcq_u8(vaeseq_u8(tmp, _k.neon.ek[12]));
	tmp = veorq_u8(vaeseq_u8(tmp, _k.neon.ek[13]), _k.neon.ek[14]);
	vst1q_u8(reinterpret_cast<uint8_t *>(out), tmp);
}

void AES::_decrypt_armneon_crypto(const void *const in, void *const out) const noexcept
{
	uint8x16_t tmp = vld1q_u8(reinterpret_cast<const uint8_t *>(in));
	tmp = vaesimcq_u8(vaesdq_u8(tmp, _k.neon.dk[0]));
	tmp = vaesimcq_u8(vaesdq_u8(tmp, _k.neon.dk[1]));
	tmp = vaesimcq_u8(vaesdq_u8(tmp, _k.neon.dk[2]));
	tmp = vaesimcq_u8(vaesdq_u8(tmp, _k.neon.dk[3]));
	tmp = vaesimcq_u8(vaesdq_u8(tmp, _k.neon.dk[4]));
	tmp = vaesimcq_u8(vaesdq_u8(tmp, _k.neon.dk[5]));
	tmp = vaesimcq_u8(vaesdq_u8(tmp, _k.neon.dk[6]));
	tmp = vaesimcq_u8(vaesdq_u8(tmp, _k.neon.dk[7]));
	tmp = vaesimcq_u8(vaesdq_u8(tmp, _k.neon.dk[8]));
	tmp = vaesimcq_u8(vaesdq_u8(tmp, _k.neon.dk[9]));
	tmp = vaesimcq_u8(vaesdq_u8(tmp, _k.neon.dk[10]));
	tmp = vaesimcq_u8(vaesdq_u8(tmp, _k.neon.dk[11]));
	tmp = vaesimcq_u8(vaesdq_u8(tmp, _k.neon.dk[12]));
	tmp = veorq_u8(vaesdq_u8(tmp, _k.neon.dk[13]), _k.neon.dk[14]);
	vst1q_u8(reinterpret_cast<uint8_t *>(out), tmp);
}

#endif // ZT_AES_NEON

} // namespace ZeroTier
