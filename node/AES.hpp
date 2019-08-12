/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2019  ZeroTier, Inc.  https://www.zerotier.com/
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

#ifndef ZT_AES_HPP
#define ZT_AES_HPP

#include "Constants.hpp"
#include "Utils.hpp"

#if (defined(__amd64) || defined(__amd64__) || defined(__x86_64) || defined(__x86_64__) || defined(__AMD64) || defined(__AMD64__) || defined(_M_X64))
#include <wmmintrin.h>
#include <emmintrin.h>
#include <smmintrin.h>
#define ZT_AES_AESNI 1
#endif

namespace ZeroTier {

/**
 * AES-256 and GCM AEAD
 * 
 * AES with 128-bit or 192-bit key sizes isn't supported here. This also only
 * supports the encrypt operation since we use AES in GCM mode. For HW acceleration
 * the code is inlined for maximum performance.
 */
class AES
{
public:
	/**
	 * This will be true if your platform's type of AES acceleration is supported on this machine
	 */
	static const bool HW_ACCEL;

	inline AES() {}
	inline AES(const uint8_t key[32]) { this->init(key); }

	inline ~AES()
	{
		Utils::burn(&_k,sizeof(_k)); // ensure that expanded key memory is zeroed on object destruction
	}

	inline void init(const uint8_t key[32])
	{
		if (HW_ACCEL) {
#ifdef ZT_AES_AESNI
			_init_aesni(key);
#endif
		} else {
			_initSW(key);
		}
	}

	inline void encrypt(const uint8_t in[16],uint8_t out[16]) const
	{
		if (HW_ACCEL) {
#ifdef ZT_AES_AESNI
			_encrypt_aesni(in,out);
#endif
		} else {
			_encryptSW(in,out);
		}
	}

	// These are public so the software mode can always be tested in self-test.
	// Normally init(), encrypt(), etc. should be used.
	void _initSW(const uint8_t key[32]);
	void _encryptSW(const uint8_t in[16],uint8_t out[16]) const;

private:
#ifdef ZT_AES_AESNI
	static inline __m128i _init256_1(__m128i a,__m128i b)
	{
		__m128i x,y;
		b = _mm_shuffle_epi32(b,0xff);
		y = _mm_slli_si128(a,0x04);
		x = _mm_xor_si128(a,y);
		y = _mm_slli_si128(y,0x04);
		x = _mm_xor_si128(x,y);
		y = _mm_slli_si128(y,0x04);
		x = _mm_xor_si128(x,y);
		x = _mm_xor_si128(x,b);
		return x;
	}
	static inline __m128i _init256_2(__m128i a,__m128i b)
	{
		__m128i x,y,z;
		y = _mm_aeskeygenassist_si128(a,0x00);
		z = _mm_shuffle_epi32(y,0xaa);
		y = _mm_slli_si128(b,0x04);
		x = _mm_xor_si128(b,y);
		y = _mm_slli_si128(y,0x04);
		x = _mm_xor_si128(x,y);
		y = _mm_slli_si128(y,0x04);
		x = _mm_xor_si128(x,y);
		x = _mm_xor_si128(x,z);
		return x;
	}
	inline void _init_aesni(const uint8_t key[32])
	{
		__m128i t1,t2;
		_k.ni[0] = t1 = _mm_loadu_si128((const __m128i *)key);
		_k.ni[1] = t2 = _mm_loadu_si128((const __m128i *)(key+16));
		_k.ni[2] = t1 = _init256_1(t1,_mm_aeskeygenassist_si128(t2,0x01));
		_k.ni[3] = t2 = _init256_2(t1,t2);
		_k.ni[4] = t1 = _init256_1(t1,_mm_aeskeygenassist_si128(t2,0x02));
		_k.ni[5] = t2 = _init256_2(t1,t2);
		_k.ni[6] = t1 = _init256_1(t1,_mm_aeskeygenassist_si128(t2,0x04));
		_k.ni[7] = t2 = _init256_2(t1,t2);
		_k.ni[8] = t1 = _init256_1(t1,_mm_aeskeygenassist_si128(t2,0x08));
		_k.ni[9] = t2 = _init256_2(t1,t2);
		_k.ni[10] = t1 = _init256_1(t1,_mm_aeskeygenassist_si128(t2,0x10));
		_k.ni[11] = t2 = _init256_2(t1,t2);
		_k.ni[12] = t1 = _init256_1(t1,_mm_aeskeygenassist_si128(t2,0x20));
		_k.ni[13] = t2 = _init256_2(t1,t2);
		_k.ni[14] = _init256_1(t1,_mm_aeskeygenassist_si128(t2,0x40));
	}
	inline void _encrypt_aesni(const void *in,void *out) const
	{
		__m128i tmp;
		tmp = _mm_loadu_si128((const __m128i *)in);
		tmp = _mm_xor_si128(tmp,_k.ni[0]);
		tmp = _mm_aesenc_si128(tmp,_k.ni[1]);
		tmp = _mm_aesenc_si128(tmp,_k.ni[2]);
		tmp = _mm_aesenc_si128(tmp,_k.ni[3]);
		tmp = _mm_aesenc_si128(tmp,_k.ni[4]);
		tmp = _mm_aesenc_si128(tmp,_k.ni[5]);
		tmp = _mm_aesenc_si128(tmp,_k.ni[6]);
		tmp = _mm_aesenc_si128(tmp,_k.ni[7]);
		tmp = _mm_aesenc_si128(tmp,_k.ni[8]);
		tmp = _mm_aesenc_si128(tmp,_k.ni[9]);
		tmp = _mm_aesenc_si128(tmp,_k.ni[10]);
		tmp = _mm_aesenc_si128(tmp,_k.ni[11]);
		tmp = _mm_aesenc_si128(tmp,_k.ni[12]);
		tmp = _mm_aesenc_si128(tmp,_k.ni[13]);
		_mm_storeu_si128((__m128i *)out,_mm_aesenclast_si128(tmp,_k.ni[14]));
	}
#endif

	union {
#ifdef ZT_AES_AESNI
		__m128i ni[15]; // AES-NI expanded key
#endif
		uint32_t sw[60]; // software mode expanded key
	} _k;
};

} // namespace ZeroTier

#endif
