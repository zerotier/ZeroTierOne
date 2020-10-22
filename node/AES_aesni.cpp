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

#ifdef ZT_AES_AESNI

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif

namespace ZeroTier {

namespace {

const __m128i s_sseSwapBytes = _mm_set_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);

__attribute__((__target__("ssse3,sse4,sse4.1,sse4.2,pclmul")))
__m128i p_gmacPCLMUL128(const __m128i h, __m128i y) noexcept
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

/* Disable VAES stuff on compilers too old to compile these intrinsics,
 * and MinGW64 also seems not to support them so disable on Windows.
 * The performance gain can be significant but regular SSE is already so
 * fast it's highly unlikely to be a rate limiting factor except on massive
 * servers and network infrastructure stuff. */
#if !defined(__WINDOWS__) && ((__GNUC__ >= 8) || (__clang_major__ >= 7))

#define ZT_AES_VAES512 1

__attribute__((__target__("sse4,aes,avx,avx2,vaes,avx512f,avx512bw")))
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

__attribute__((__target__("sse4,aes,avx,avx2,vaes")))
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

__attribute__((__target__("ssse3,sse4,sse4.1,sse4.2,aes,pclmul")))
__m128i p_init256_1_aesni(__m128i a, __m128i b) noexcept
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

__attribute__((__target__("ssse3,sse4,sse4.1,sse4.2,aes,pclmul")))
__m128i p_init256_2_aesni(__m128i a, __m128i b) noexcept
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

} // anonymous namespace

__attribute__((__target__("ssse3,sse4,sse4.1,sse4.2,pclmul")))
void AES::GMAC::p_aesNIUpdate(const uint8_t *in, unsigned int len) noexcept
{
	__m128i y = _mm_loadu_si128(reinterpret_cast<const __m128i *>(_y));

	// Handle anything left over from a previous run that wasn't a multiple of 16 bytes.
	if (_rp) {
		for (;;) {
			if (!len)
				return;
			--len;
			_r[_rp++] = *(in++);
			if (_rp == 16) {
				y = p_gmacPCLMUL128(_aes.p_k.ni.h[0], _mm_xor_si128(y, _mm_loadu_si128(reinterpret_cast<__m128i *>(_r))));
				break;
			}
		}
	}

	if (likely(len >= 64)) {
		const __m128i sb = s_sseSwapBytes;
		const __m128i h = _aes.p_k.ni.h[0];
		const __m128i hh = _aes.p_k.ni.h[1];
		const __m128i hhh = _aes.p_k.ni.h[2];
		const __m128i hhhh = _aes.p_k.ni.h[3];
		const __m128i h2 = _aes.p_k.ni.h2[0];
		const __m128i hh2 = _aes.p_k.ni.h2[1];
		const __m128i hhh2 = _aes.p_k.ni.h2[2];
		const __m128i hhhh2 = _aes.p_k.ni.h2[3];
		const uint8_t *const end64 = in + (len & ~((unsigned int)63));
		len &= 63U;
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
		y = p_gmacPCLMUL128(_aes.p_k.ni.h[0], _mm_xor_si128(y, _mm_loadu_si128(reinterpret_cast<const __m128i *>(in))));
		in += 16;
		len -= 16;
	}

	_mm_storeu_si128(reinterpret_cast<__m128i *>(_y), y);

	// Any overflow is cached for a later run or finish().
	for (unsigned int i = 0; i < len; ++i)
		_r[i] = in[i];
	_rp = len; // len is always less than 16 here
}

__attribute__((__target__("ssse3,sse4,sse4.1,sse4.2,pclmul,aes")))
void AES::GMAC::p_aesNIFinish(uint8_t tag[16]) noexcept
{
	__m128i y = _mm_loadu_si128(reinterpret_cast<const __m128i *>(_y));

	// Handle any remaining bytes, padding the last block with zeroes.
	if (_rp) {
		while (_rp < 16)
			_r[_rp++] = 0;
		y = p_gmacPCLMUL128(_aes.p_k.ni.h[0], _mm_xor_si128(y, _mm_loadu_si128(reinterpret_cast<__m128i *>(_r))));
	}

	// Interleave encryption of IV with the final GHASH of y XOR (length * 8).
	// Then XOR these together to get the final tag.
	const __m128i *const k = _aes.p_k.ni.k;
	const __m128i h = _aes.p_k.ni.h[0];
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
}

__attribute__((__target__("ssse3,sse4,sse4.1,sse4.2,aes")))
void AES::CTR::p_aesNICrypt(const uint8_t *in, uint8_t *out, unsigned int len) noexcept
{
	const __m128i dd = _mm_set_epi64x(0, (long long)_ctr[0]);
	uint64_t c1 = Utils::ntoh(_ctr[1]);

	const __m128i *const k = _aes.p_k.ni.k;
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
}

__attribute__((__target__("ssse3,sse4,sse4.1,sse4.2,aes,pclmul")))
void AES::p_init_aesni(const uint8_t *key) noexcept
{
	__m128i t1, t2, k1, k2, k3, k4, k5, k6, k7, k8, k9, k10, k11, k12, k13;
	p_k.ni.k[0] = t1 = _mm_loadu_si128((const __m128i *)key);
	p_k.ni.k[1] = k1 = t2 = _mm_loadu_si128((const __m128i *)(key + 16));
	p_k.ni.k[2] = k2 = t1 = p_init256_1_aesni(t1, _mm_aeskeygenassist_si128(t2, 0x01));
	p_k.ni.k[3] = k3 = t2 = p_init256_2_aesni(t1, t2);
	p_k.ni.k[4] = k4 = t1 = p_init256_1_aesni(t1, _mm_aeskeygenassist_si128(t2, 0x02));
	p_k.ni.k[5] = k5 = t2 = p_init256_2_aesni(t1, t2);
	p_k.ni.k[6] = k6 = t1 = p_init256_1_aesni(t1, _mm_aeskeygenassist_si128(t2, 0x04));
	p_k.ni.k[7] = k7 = t2 = p_init256_2_aesni(t1, t2);
	p_k.ni.k[8] = k8 = t1 = p_init256_1_aesni(t1, _mm_aeskeygenassist_si128(t2, 0x08));
	p_k.ni.k[9] = k9 = t2 = p_init256_2_aesni(t1, t2);
	p_k.ni.k[10] = k10 = t1 = p_init256_1_aesni(t1, _mm_aeskeygenassist_si128(t2, 0x10));
	p_k.ni.k[11] = k11 = t2 = p_init256_2_aesni(t1, t2);
	p_k.ni.k[12] = k12 = t1 = p_init256_1_aesni(t1, _mm_aeskeygenassist_si128(t2, 0x20));
	p_k.ni.k[13] = k13 = t2 = p_init256_2_aesni(t1, t2);
	p_k.ni.k[14] = p_init256_1_aesni(t1, _mm_aeskeygenassist_si128(t2, 0x40));
	p_k.ni.k[15] = _mm_aesimc_si128(k13);
	p_k.ni.k[16] = _mm_aesimc_si128(k12);
	p_k.ni.k[17] = _mm_aesimc_si128(k11);
	p_k.ni.k[18] = _mm_aesimc_si128(k10);
	p_k.ni.k[19] = _mm_aesimc_si128(k9);
	p_k.ni.k[20] = _mm_aesimc_si128(k8);
	p_k.ni.k[21] = _mm_aesimc_si128(k7);
	p_k.ni.k[22] = _mm_aesimc_si128(k6);
	p_k.ni.k[23] = _mm_aesimc_si128(k5);
	p_k.ni.k[24] = _mm_aesimc_si128(k4);
	p_k.ni.k[25] = _mm_aesimc_si128(k3);
	p_k.ni.k[26] = _mm_aesimc_si128(k2);
	p_k.ni.k[27] = _mm_aesimc_si128(k1);

	__m128i h = p_k.ni.k[0]; // _mm_xor_si128(_mm_setzero_si128(),_k.ni.k[0]);
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
	h = _mm_aesenclast_si128(h, p_k.ni.k[14]);
	__m128i hswap = _mm_shuffle_epi8(h, s_sseSwapBytes);
	__m128i hh = p_gmacPCLMUL128(hswap, h);
	__m128i hhh = p_gmacPCLMUL128(hswap, hh);
	__m128i hhhh = p_gmacPCLMUL128(hswap, hhh);
	p_k.ni.h[0] = hswap;
	p_k.ni.h[1] = hh = _mm_shuffle_epi8(hh, s_sseSwapBytes);
	p_k.ni.h[2] = hhh = _mm_shuffle_epi8(hhh, s_sseSwapBytes);
	p_k.ni.h[3] = hhhh = _mm_shuffle_epi8(hhhh, s_sseSwapBytes);
	p_k.ni.h2[0] = _mm_xor_si128(_mm_shuffle_epi32(hswap, 78), hswap);
	p_k.ni.h2[1] = _mm_xor_si128(_mm_shuffle_epi32(hh, 78), hh);
	p_k.ni.h2[2] = _mm_xor_si128(_mm_shuffle_epi32(hhh, 78), hhh);
	p_k.ni.h2[3] = _mm_xor_si128(_mm_shuffle_epi32(hhhh, 78), hhhh);
}

__attribute__((__target__("ssse3,sse4,sse4.1,sse4.2,aes,pclmul")))
void AES::p_encrypt_aesni(const void *const in, void *const out) const noexcept
{
	__m128i tmp = _mm_loadu_si128((const __m128i *)in);
	tmp = _mm_xor_si128(tmp, p_k.ni.k[0]);
	tmp = _mm_aesenc_si128(tmp, p_k.ni.k[1]);
	tmp = _mm_aesenc_si128(tmp, p_k.ni.k[2]);
	tmp = _mm_aesenc_si128(tmp, p_k.ni.k[3]);
	tmp = _mm_aesenc_si128(tmp, p_k.ni.k[4]);
	tmp = _mm_aesenc_si128(tmp, p_k.ni.k[5]);
	tmp = _mm_aesenc_si128(tmp, p_k.ni.k[6]);
	tmp = _mm_aesenc_si128(tmp, p_k.ni.k[7]);
	tmp = _mm_aesenc_si128(tmp, p_k.ni.k[8]);
	tmp = _mm_aesenc_si128(tmp, p_k.ni.k[9]);
	tmp = _mm_aesenc_si128(tmp, p_k.ni.k[10]);
	tmp = _mm_aesenc_si128(tmp, p_k.ni.k[11]);
	tmp = _mm_aesenc_si128(tmp, p_k.ni.k[12]);
	tmp = _mm_aesenc_si128(tmp, p_k.ni.k[13]);
	_mm_storeu_si128((__m128i *)out, _mm_aesenclast_si128(tmp, p_k.ni.k[14]));
}

__attribute__((__target__("ssse3,sse4,sse4.1,sse4.2,aes,pclmul")))
void AES::p_decrypt_aesni(const void *in, void *out) const noexcept
{
	__m128i tmp = _mm_loadu_si128((const __m128i *)in);
	tmp = _mm_xor_si128(tmp, p_k.ni.k[14]);
	tmp = _mm_aesdec_si128(tmp, p_k.ni.k[15]);
	tmp = _mm_aesdec_si128(tmp, p_k.ni.k[16]);
	tmp = _mm_aesdec_si128(tmp, p_k.ni.k[17]);
	tmp = _mm_aesdec_si128(tmp, p_k.ni.k[18]);
	tmp = _mm_aesdec_si128(tmp, p_k.ni.k[19]);
	tmp = _mm_aesdec_si128(tmp, p_k.ni.k[20]);
	tmp = _mm_aesdec_si128(tmp, p_k.ni.k[21]);
	tmp = _mm_aesdec_si128(tmp, p_k.ni.k[22]);
	tmp = _mm_aesdec_si128(tmp, p_k.ni.k[23]);
	tmp = _mm_aesdec_si128(tmp, p_k.ni.k[24]);
	tmp = _mm_aesdec_si128(tmp, p_k.ni.k[25]);
	tmp = _mm_aesdec_si128(tmp, p_k.ni.k[26]);
	tmp = _mm_aesdec_si128(tmp, p_k.ni.k[27]);
	_mm_storeu_si128((__m128i *)out, _mm_aesdeclast_si128(tmp, p_k.ni.k[0]));
}

} // namespace ZeroTier

#endif // ZT_AES_AESNI
