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
		Utils::burn(&_k,sizeof(_k));
	}

	inline void init(const uint8_t key[32])
	{
#ifdef ZT_AES_AESNI
		if (HW_ACCEL) {
			_init_aesni(key);
			return;
		}
#endif
		_initSW(key);
	}

	inline void encrypt(const uint8_t in[16],uint8_t out[16]) const
	{
#ifdef ZT_AES_AESNI
		if (HW_ACCEL) {
			_encrypt_aesni(in,out);
			return;
		}
#endif
		_encryptSW(in,out);
	}

	inline void gcmEncrypt(const uint8_t iv[12],const void *in,unsigned int inlen,const void *assoc,unsigned int assoclen,void *out,uint8_t *tag,unsigned int taglen)
	{
#ifdef ZT_AES_AESNI
		if (HW_ACCEL) {
			_encrypt_gcm256_aesni(inlen,(const uint8_t *)in,(uint8_t *)out,iv,assoclen,(const uint8_t *)assoc,tag,taglen);
			return;
		}
#endif
		abort(); // TODO: software
	}

	inline bool gcmDecrypt(const uint8_t iv[12],const void *in,unsigned int inlen,const void *assoc,unsigned int assoclen,void *out,const uint8_t *tag,unsigned int taglen)
	{
#ifdef ZT_AES_AESNI
		if (HW_ACCEL) {
			uint8_t tagbuf[16];
			_decrypt_gcm256_aesni(inlen,(const uint8_t *)in,(uint8_t *)out,iv,assoclen,(const uint8_t *)assoc,tagbuf,taglen);
			return Utils::secureEq(tagbuf,tag,taglen);
		}
#endif
		abort(); // TODO: software
		return false;
	}

private:
	void _initSW(const uint8_t key[32]);
	void _encryptSW(const uint8_t in[16],uint8_t out[16]) const;

	union {
#ifdef ZT_AES_AESNI
		struct {
			__m128i k[15];
			__m128i h,hh,hhh,hhhh;
		} ni;
#endif
		struct {
			uint32_t k[60];
		} sw;
	} _k;

#ifdef ZT_AES_AESNI /********************************************************/
	static inline __m128i _init256_1_aesni(__m128i a,__m128i b)
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
	static inline __m128i _init256_2_aesni(__m128i a,__m128i b)
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
		/* Init AES itself */
		__m128i t1,t2;
		_k.ni.k[0] = t1 = _mm_loadu_si128((const __m128i *)key);
		_k.ni.k[1] = t2 = _mm_loadu_si128((const __m128i *)(key+16));
		_k.ni.k[2] = t1 = _init256_1_aesni(t1,_mm_aeskeygenassist_si128(t2,0x01));
		_k.ni.k[3] = t2 = _init256_2_aesni(t1,t2);
		_k.ni.k[4] = t1 = _init256_1_aesni(t1,_mm_aeskeygenassist_si128(t2,0x02));
		_k.ni.k[5] = t2 = _init256_2_aesni(t1,t2);
		_k.ni.k[6] = t1 = _init256_1_aesni(t1,_mm_aeskeygenassist_si128(t2,0x04));
		_k.ni.k[7] = t2 = _init256_2_aesni(t1,t2);
		_k.ni.k[8] = t1 = _init256_1_aesni(t1,_mm_aeskeygenassist_si128(t2,0x08));
		_k.ni.k[9] = t2 = _init256_2_aesni(t1,t2);
		_k.ni.k[10] = t1 = _init256_1_aesni(t1,_mm_aeskeygenassist_si128(t2,0x10));
		_k.ni.k[11] = t2 = _init256_2_aesni(t1,t2);
		_k.ni.k[12] = t1 = _init256_1_aesni(t1,_mm_aeskeygenassist_si128(t2,0x20));
		_k.ni.k[13] = t2 = _init256_2_aesni(t1,t2);
		_k.ni.k[14] = _init256_1_aesni(t1,_mm_aeskeygenassist_si128(t2,0x40));

		/* Init GCM / GHASH */
		__m128i h = _mm_xor_si128(_mm_setzero_si128(),_k.ni.k[0]);
		h = _mm_aesenc_si128(h,_k.ni.k[1]);
		h = _mm_aesenc_si128(h,_k.ni.k[2]);
		h = _mm_aesenc_si128(h,_k.ni.k[3]);
		h = _mm_aesenc_si128(h,_k.ni.k[4]);
		h = _mm_aesenc_si128(h,_k.ni.k[5]);
		h = _mm_aesenc_si128(h,_k.ni.k[6]);
		h = _mm_aesenc_si128(h,_k.ni.k[7]);
		h = _mm_aesenc_si128(h,_k.ni.k[8]);
		h = _mm_aesenc_si128(h,_k.ni.k[9]);
		h = _mm_aesenc_si128(h,_k.ni.k[10]);
		h = _mm_aesenc_si128(h,_k.ni.k[11]);
		h = _mm_aesenc_si128(h,_k.ni.k[12]);
		h = _mm_aesenc_si128(h,_k.ni.k[13]);
		h = _mm_aesenclast_si128(h,_k.ni.k[14]);
		__m128i hswap = _swap128_aesni(h);
		__m128i hh = _mult_block_aesni(hswap,h);
		__m128i hhh = _mult_block_aesni(hswap,hh);
		__m128i hhhh = _mult_block_aesni(hswap,hhh);
		_k.ni.h = hswap;
		_k.ni.hh = _swap128_aesni(hh);
		_k.ni.hhh = _swap128_aesni(hhh);
		_k.ni.hhhh = _swap128_aesni(hhhh);
		/*
		this->h = h;
		h = swap128(h);
		this->hh = mult_block(h, this->h);
		this->hhh = mult_block(h, this->hh);
		this->hhhh = mult_block(h, this->hhh);
		this->h = swap128(this->h);
		this->hh = swap128(this->hh);
		this->hhh = swap128(this->hhh);
		this->hhhh = swap128(this->hhhh);
		*/
	}

	inline void _encrypt_aesni(const void *in,void *out) const
	{
		__m128i tmp;
		tmp = _mm_loadu_si128((const __m128i *)in);
		tmp = _mm_xor_si128(tmp,_k.ni.k[0]);
		tmp = _mm_aesenc_si128(tmp,_k.ni.k[1]);
		tmp = _mm_aesenc_si128(tmp,_k.ni.k[2]);
		tmp = _mm_aesenc_si128(tmp,_k.ni.k[3]);
		tmp = _mm_aesenc_si128(tmp,_k.ni.k[4]);
		tmp = _mm_aesenc_si128(tmp,_k.ni.k[5]);
		tmp = _mm_aesenc_si128(tmp,_k.ni.k[6]);
		tmp = _mm_aesenc_si128(tmp,_k.ni.k[7]);
		tmp = _mm_aesenc_si128(tmp,_k.ni.k[8]);
		tmp = _mm_aesenc_si128(tmp,_k.ni.k[9]);
		tmp = _mm_aesenc_si128(tmp,_k.ni.k[10]);
		tmp = _mm_aesenc_si128(tmp,_k.ni.k[11]);
		tmp = _mm_aesenc_si128(tmp,_k.ni.k[12]);
		tmp = _mm_aesenc_si128(tmp,_k.ni.k[13]);
		_mm_storeu_si128((__m128i *)out,_mm_aesenclast_si128(tmp,_k.ni.k[14]));
	}

	static inline __m128i _swap128_aesni(__m128i x) { return _mm_shuffle_epi8(x,_mm_set_epi8(0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15)); }
	static inline __m128i _mult_block_aesni(__m128i h,__m128i y)
	{
		__m128i t1,t2,t3,t4,t5,t6;
		y = _swap128_aesni(y);
		t1 = _mm_clmulepi64_si128(h,y,0x00);
		t2 = _mm_clmulepi64_si128(h,y,0x01);
		t3 = _mm_clmulepi64_si128(h,y,0x10);
		t4 = _mm_clmulepi64_si128(h,y,0x11);
		t2 = _mm_xor_si128(t2,t3);
		t3 = _mm_slli_si128(t2,8);
		t2 = _mm_srli_si128(t2,8);
		t1 = _mm_xor_si128(t1,t3);
		t4 = _mm_xor_si128(t4,t2);
		t5 = _mm_srli_epi32(t1,31);
		t1 = _mm_slli_epi32(t1,1);
		t6 = _mm_srli_epi32(t4,31);
		t4 = _mm_slli_epi32(t4,1);
		t3 = _mm_srli_si128(t5,12);
		t6 = _mm_slli_si128(t6,4);
		t5 = _mm_slli_si128(t5,4);
		t1 = _mm_or_si128(t1,t5);
		t4 = _mm_or_si128(t4,t6);
		t4 = _mm_or_si128(t4,t3);
		t5 = _mm_slli_epi32(t1,31);
		t6 = _mm_slli_epi32(t1,30);
		t3 = _mm_slli_epi32(t1,25);
		t5 = _mm_xor_si128(t5,t6);
		t5 = _mm_xor_si128(t5,t3);
		t6 = _mm_srli_si128(t5,4);
		t4 = _mm_xor_si128(t4,t6);
		t5 = _mm_slli_si128(t5,12);
		t1 = _mm_xor_si128(t1,t5);
		t4 = _mm_xor_si128(t4,t1);
		t5 = _mm_srli_epi32(t1,1);
		t2 = _mm_srli_epi32(t1,2);
		t3 = _mm_srli_epi32(t1,7);
		t4 = _mm_xor_si128(t4,t2);
		t4 = _mm_xor_si128(t4,t3);
		t4 = _mm_xor_si128(t4,t5);
		return _swap128_aesni(t4);
	}
	static inline __m128i _mult4xor_aesni(__m128i h1,__m128i h2,__m128i h3,__m128i h4,__m128i d1,__m128i d2,__m128i d3,__m128i d4)
	{
		__m128i t0,t1,t2,t3,t4,t5,t6,t7,t8,t9;
		d1 = _swap128_aesni(d1);
		d2 = _swap128_aesni(d2);
		d3 = _swap128_aesni(d3);
		d4 = _swap128_aesni(d4);
		t0 = _mm_clmulepi64_si128(h1,d1,0x00);
		t1 = _mm_clmulepi64_si128(h2,d2,0x00);
		t2 = _mm_clmulepi64_si128(h3,d3,0x00);
		t3 = _mm_clmulepi64_si128(h4,d4,0x00);
		t8 = _mm_xor_si128(t0,t1);
		t8 = _mm_xor_si128(t8,t2);
		t8 = _mm_xor_si128(t8,t3);
		t4 = _mm_clmulepi64_si128(h1,d1,0x11);
		t5 = _mm_clmulepi64_si128(h2,d2,0x11);
		t6 = _mm_clmulepi64_si128(h3,d3,0x11);
		t7 = _mm_clmulepi64_si128(h4,d4,0x11);
		t9 = _mm_xor_si128(t4,t5);
		t9 = _mm_xor_si128(t9,t6);
		t9 = _mm_xor_si128(t9,t7);
		t0 = _mm_shuffle_epi32(h1,78);
		t4 = _mm_shuffle_epi32(d1,78);
		t0 = _mm_xor_si128(t0,h1);
		t4 = _mm_xor_si128(t4,d1);
		t1 = _mm_shuffle_epi32(h2,78);
		t5 = _mm_shuffle_epi32(d2,78);
		t1 = _mm_xor_si128(t1,h2);
		t5 = _mm_xor_si128(t5,d2);
		t2 = _mm_shuffle_epi32(h3,78);
		t6 = _mm_shuffle_epi32(d3,78);
		t2 = _mm_xor_si128(t2,h3);
		t6 = _mm_xor_si128(t6,d3);
		t3 = _mm_shuffle_epi32(h4,78);
		t7 = _mm_shuffle_epi32(d4,78);
		t3 = _mm_xor_si128(t3,h4);
		t7 = _mm_xor_si128(t7,d4);
		t0 = _mm_clmulepi64_si128(t0,t4,0x00);
		t1 = _mm_clmulepi64_si128(t1,t5,0x00);
		t2 = _mm_clmulepi64_si128(t2,t6,0x00);
		t3 = _mm_clmulepi64_si128(t3,t7,0x00);
		t0 = _mm_xor_si128(t0,t8);
		t0 = _mm_xor_si128(t0,t9);
		t0 = _mm_xor_si128(t1,t0);
		t0 = _mm_xor_si128(t2,t0);
		t0 = _mm_xor_si128(t3,t0);
		t4 = _mm_slli_si128(t0,8);
		t0 = _mm_srli_si128(t0,8);
		t3 = _mm_xor_si128(t4,t8);
		t6 = _mm_xor_si128(t0,t9);
		t7 = _mm_srli_epi32(t3,31);
		t8 = _mm_srli_epi32(t6,31);
		t3 = _mm_slli_epi32(t3,1);
		t6 = _mm_slli_epi32(t6,1);
		t9 = _mm_srli_si128(t7,12);
		t8 = _mm_slli_si128(t8,4);
		t7 = _mm_slli_si128(t7,4);
		t3 = _mm_or_si128(t3,t7);
		t6 = _mm_or_si128(t6,t8);
		t6 = _mm_or_si128(t6,t9);
		t7 = _mm_slli_epi32(t3,31);
		t8 = _mm_slli_epi32(t3,30);
		t9 = _mm_slli_epi32(t3,25);
		t7 = _mm_xor_si128(t7,t8);
		t7 = _mm_xor_si128(t7,t9);
		t8 = _mm_srli_si128(t7,4);
		t7 = _mm_slli_si128(t7,12);
		t3 = _mm_xor_si128(t3,t7);
		t2 = _mm_srli_epi32(t3,1);
		t4 = _mm_srli_epi32(t3,2);
		t5 = _mm_srli_epi32(t3,7);
		t2 = _mm_xor_si128(t2,t4);
		t2 = _mm_xor_si128(t2,t5);
		t2 = _mm_xor_si128(t2,t8);
		t3 = _mm_xor_si128(t3,t2);
		t6 = _mm_xor_si128(t6,t3);
		return _swap128_aesni(t6);
	}
	static inline __m128i _ghash_aesni(__m128i h,__m128i y,__m128i x) { return _mult_block_aesni(h,_mm_xor_si128(y,x)); }
	static inline __m128i _increment_be_aesni(__m128i x)
	{
		x = _swap128_aesni(x);
		x = _mm_add_epi64(x, _mm_set_epi32(0, 0, 0, 1));
		x = _swap128_aesni(x);
		return x;
	}
	static inline void _htoun64_aesni(void *network,const uint64_t host) { *((uint64_t *)network) = Utils::hton(host); }

	inline __m128i _create_j_aesni(const uint8_t *iv) const
	{
		uint8_t j[16];
		*((uint64_t *)j) = *((const uint64_t *)iv);
		*((uint32_t *)(j+8)) = *((const uint32_t *)(iv+8));
		j[12] = 0;
		j[13] = 0;
		j[14] = 0;
		j[15] = 1;
		return _mm_loadu_si128((__m128i *)j);
	}
	inline __m128i _icv_header_aesni(const void *assoc,unsigned int alen) const
	{
		unsigned int blocks,pblocks,rem,i;
		__m128i h1,h2,h3,h4,d1,d2,d3,d4;
		__m128i y,last;
		const __m128i *ab;
		h1 = _k.ni.hhhh;
		h2 = _k.ni.hhh;
		h3 = _k.ni.hh;
		h4 = _k.ni.h;
		y = _mm_setzero_si128();
		ab = (const __m128i *)assoc;
		blocks = alen / 16;
		pblocks = blocks - (blocks % 4);
		rem = alen % 16;
		for (i=0;i<pblocks;i+=4) {
			d1 = _mm_loadu_si128(ab + i + 0);
			d2 = _mm_loadu_si128(ab + i + 1);
			d3 = _mm_loadu_si128(ab + i + 2);
			d4 = _mm_loadu_si128(ab + i + 3);
			y = _mm_xor_si128(y, d1);
			y = _mult4xor_aesni(h1,h2,h3,h4,y,d2,d3,d4);
		}
		for (i = pblocks; i < blocks; i++)
			y = _ghash_aesni(_k.ni.h,y,_mm_loadu_si128(ab + i));
		if (rem) {
			last = _mm_setzero_si128();
			memcpy(&last,ab + blocks,rem);
			y = _ghash_aesni(_k.ni.h,y,last);
		}
		return y;
	}
	inline __m128i _icv_tailer_aesni(__m128i y,size_t alen,size_t dlen) const
	{
		__m128i b;
		_htoun64_aesni(&b, alen * 8);
		_htoun64_aesni((uint8_t *)&b + sizeof(uint64_t), dlen * 8);
		return _ghash_aesni(_k.ni.h, y, b);
	}
	inline void _icv_crypt_aesni(__m128i y,__m128i j,uint8_t *icv,unsigned int icvsize) const
	{
		__m128i t,b;
		t = _mm_xor_si128(j,_k.ni.k[0]);
		t = _mm_aesenc_si128(t,_k.ni.k[1]);
		t = _mm_aesenc_si128(t,_k.ni.k[2]);
		t = _mm_aesenc_si128(t,_k.ni.k[3]);
		t = _mm_aesenc_si128(t,_k.ni.k[4]);
		t = _mm_aesenc_si128(t,_k.ni.k[5]);
		t = _mm_aesenc_si128(t,_k.ni.k[6]);
		t = _mm_aesenc_si128(t,_k.ni.k[7]);
		t = _mm_aesenc_si128(t,_k.ni.k[8]);
		t = _mm_aesenc_si128(t,_k.ni.k[9]);
		t = _mm_aesenc_si128(t,_k.ni.k[10]);
		t = _mm_aesenc_si128(t,_k.ni.k[11]);
		t = _mm_aesenc_si128(t,_k.ni.k[12]);
		t = _mm_aesenc_si128(t,_k.ni.k[13]);
		t = _mm_aesenclast_si128(t,_k.ni.k[14]);
		t = _mm_xor_si128(y, t);
		_mm_storeu_si128(&b, t);
		memcpy(icv,&b,icvsize);
	}

	inline __m128i _encrypt_gcm_rem_aesni(unsigned int rem,const void *in,void *out,__m128i cb,__m128i y) const
	{
		__m128i t,b;
		memset(&b,0,sizeof(b));
		memcpy(&b,in,rem);
		t = _mm_xor_si128(cb,_k.ni.k[0]);
		t = _mm_aesenc_si128(t,_k.ni.k[1]);
		t = _mm_aesenc_si128(t,_k.ni.k[2]);
		t = _mm_aesenc_si128(t,_k.ni.k[3]);
		t = _mm_aesenc_si128(t,_k.ni.k[4]);
		t = _mm_aesenc_si128(t,_k.ni.k[5]);
		t = _mm_aesenc_si128(t,_k.ni.k[6]);
		t = _mm_aesenc_si128(t,_k.ni.k[7]);
		t = _mm_aesenc_si128(t,_k.ni.k[8]);
		t = _mm_aesenc_si128(t,_k.ni.k[9]);
		t = _mm_aesenc_si128(t,_k.ni.k[10]);
		t = _mm_aesenc_si128(t,_k.ni.k[11]);
		t = _mm_aesenc_si128(t,_k.ni.k[12]);
		t = _mm_aesenc_si128(t,_k.ni.k[13]);
		t = _mm_aesenclast_si128(t,_k.ni.k[14]);
		b = _mm_xor_si128(t,b);
		memcpy(out,&b,rem);
		memset((u_char*)&b + rem,0,16 - rem);
		return _ghash_aesni(_k.ni.h,y,b);
	}
	inline void _encrypt_gcm256_aesni(unsigned int len,const uint8_t *in,uint8_t *out,const uint8_t *iv,unsigned int alen,const uint8_t *assoc,uint8_t *icv,unsigned int icvsize) const
	{
		__m128i d1,d2,d3,d4,t1,t2,t3,t4,k;
		__m128i y,j,cb,*bi,*bo;

		j = _create_j_aesni(iv);
		cb = _increment_be_aesni(j);
		y = _icv_header_aesni(assoc,alen);
		unsigned int blocks = len / 16;
		unsigned int pblocks = blocks - (blocks % 4);
		unsigned int rem = len % 16;
		bi = (__m128i *)in;
		bo = (__m128i *)out;

		unsigned int i;
		for (i=0;i<pblocks;i+=4) {
			d1 = _mm_loadu_si128(bi + i + 0);
			d2 = _mm_loadu_si128(bi + i + 1);
			d3 = _mm_loadu_si128(bi + i + 2);
			d4 = _mm_loadu_si128(bi + i + 3);
			t1 = _mm_xor_si128(cb,k = _k.ni.k[0]);
			cb = _increment_be_aesni(cb);
			t2 = _mm_xor_si128(cb,k);
			cb = _increment_be_aesni(cb);
			t3 = _mm_xor_si128(cb,k);
			cb = _increment_be_aesni(cb);
			t4 = _mm_xor_si128(cb,k);
			cb = _increment_be_aesni(cb);
			t1 = _mm_aesenc_si128(t1,k = _k.ni.k[1]);
			t2 = _mm_aesenc_si128(t2,k);
			t3 = _mm_aesenc_si128(t3,k);
			t4 = _mm_aesenc_si128(t4,k);
			t1 = _mm_aesenc_si128(t1,k = _k.ni.k[2]);
			t2 = _mm_aesenc_si128(t2,k);
			t3 = _mm_aesenc_si128(t3,k);
			t4 = _mm_aesenc_si128(t4,k);
			t1 = _mm_aesenc_si128(t1,k = _k.ni.k[3]);
			t2 = _mm_aesenc_si128(t2,k);
			t3 = _mm_aesenc_si128(t3,k);
			t4 = _mm_aesenc_si128(t4,k);
			t1 = _mm_aesenc_si128(t1,k = _k.ni.k[4]);
			t2 = _mm_aesenc_si128(t2,k);
			t3 = _mm_aesenc_si128(t3,k);
			t4 = _mm_aesenc_si128(t4,k);
			t1 = _mm_aesenc_si128(t1,k = _k.ni.k[5]);
			t2 = _mm_aesenc_si128(t2,k);
			t3 = _mm_aesenc_si128(t3,k);
			t4 = _mm_aesenc_si128(t4,k);
			t1 = _mm_aesenc_si128(t1,k = _k.ni.k[6]);
			t2 = _mm_aesenc_si128(t2,k);
			t3 = _mm_aesenc_si128(t3,k);
			t4 = _mm_aesenc_si128(t4,k);
			t1 = _mm_aesenc_si128(t1,k = _k.ni.k[7]);
			t2 = _mm_aesenc_si128(t2,k);
			t3 = _mm_aesenc_si128(t3,k);
			t4 = _mm_aesenc_si128(t4,k);
			t1 = _mm_aesenc_si128(t1,k = _k.ni.k[8]);
			t2 = _mm_aesenc_si128(t2,k);
			t3 = _mm_aesenc_si128(t3,k);
			t4 = _mm_aesenc_si128(t4,k);
			t1 = _mm_aesenc_si128(t1,k = _k.ni.k[9]);
			t2 = _mm_aesenc_si128(t2,k);
			t3 = _mm_aesenc_si128(t3,k);
			t4 = _mm_aesenc_si128(t4,k);
			t1 = _mm_aesenc_si128(t1,k = _k.ni.k[10]);
			t2 = _mm_aesenc_si128(t2,k);
			t3 = _mm_aesenc_si128(t3,k);
			t4 = _mm_aesenc_si128(t4,k);
			t1 = _mm_aesenc_si128(t1,k = _k.ni.k[11]);
			t2 = _mm_aesenc_si128(t2,k);
			t3 = _mm_aesenc_si128(t3,k);
			t4 = _mm_aesenc_si128(t4,k);
			t1 = _mm_aesenc_si128(t1,k = _k.ni.k[12]);
			t2 = _mm_aesenc_si128(t2,k);
			t3 = _mm_aesenc_si128(t3,k);
			t4 = _mm_aesenc_si128(t4,k);
			t1 = _mm_aesenc_si128(t1,k = _k.ni.k[13]);
			t2 = _mm_aesenc_si128(t2,k);
			t3 = _mm_aesenc_si128(t3,k);
			t4 = _mm_aesenc_si128(t4,k);
			t1 = _mm_aesenclast_si128(t1,k = _k.ni.k[14]);
			t2 = _mm_aesenclast_si128(t2,k);
			t3 = _mm_aesenclast_si128(t3,k);
			t4 = _mm_aesenclast_si128(t4,k);
			t1 = _mm_xor_si128(t1,d1);
			t2 = _mm_xor_si128(t2,d2);
			t3 = _mm_xor_si128(t3,d3);
			t4 = _mm_xor_si128(t4,d4);
			y = _mm_xor_si128(y,t1);
			y = _mult4xor_aesni(_k.ni.hhhh,_k.ni.hhh,_k.ni.hh,_k.ni.h,y,t2,t3,t4);
			_mm_storeu_si128(bo + i + 0,t1);
			_mm_storeu_si128(bo + i + 1,t2);
			_mm_storeu_si128(bo + i + 2,t3);
			_mm_storeu_si128(bo + i + 3,t4);
		}

		for (i=pblocks;i<blocks;++i) {
			d1 = _mm_loadu_si128(bi + i);
			t1 = _mm_xor_si128(cb,_k.ni.k[0]);
			t1 = _mm_aesenc_si128(t1,_k.ni.k[1]);
			t1 = _mm_aesenc_si128(t1,_k.ni.k[2]);
			t1 = _mm_aesenc_si128(t1,_k.ni.k[3]);
			t1 = _mm_aesenc_si128(t1,_k.ni.k[4]);
			t1 = _mm_aesenc_si128(t1,_k.ni.k[5]);
			t1 = _mm_aesenc_si128(t1,_k.ni.k[6]);
			t1 = _mm_aesenc_si128(t1,_k.ni.k[7]);
			t1 = _mm_aesenc_si128(t1,_k.ni.k[8]);
			t1 = _mm_aesenc_si128(t1,_k.ni.k[9]);
			t1 = _mm_aesenc_si128(t1,_k.ni.k[10]);
			t1 = _mm_aesenc_si128(t1,_k.ni.k[11]);
			t1 = _mm_aesenc_si128(t1,_k.ni.k[12]);
			t1 = _mm_aesenc_si128(t1,_k.ni.k[13]);
			t1 = _mm_aesenclast_si128(t1,_k.ni.k[14]);
			t1 = _mm_xor_si128(t1,d1);
			_mm_storeu_si128(bo + i,t1);
			y = _ghash_aesni(_k.ni.h,y,t1);
			cb = _increment_be_aesni(cb);
		}

		if (rem)
			y = _encrypt_gcm_rem_aesni(rem,bi + blocks,bo + blocks,cb,y);
		y = _icv_tailer_aesni(y,alen,len);
		_icv_crypt_aesni(y,j,icv,icvsize);
	}
	inline __m128i _decrypt_gcm_rem_aesni(unsigned int rem,const void *in,void *out,__m128i cb,__m128i y)
	{
		__m128i t,b;
		memset(&b,0,sizeof(b));
		memcpy(&b,in,rem);
		y = _ghash_aesni(_k.ni.h,y,b);
		t = _mm_xor_si128(cb,_k.ni.k[0]);
		t = _mm_aesenc_si128(t,_k.ni.k[1]);
		t = _mm_aesenc_si128(t,_k.ni.k[2]);
		t = _mm_aesenc_si128(t,_k.ni.k[3]);
		t = _mm_aesenc_si128(t,_k.ni.k[4]);
		t = _mm_aesenc_si128(t,_k.ni.k[5]);
		t = _mm_aesenc_si128(t,_k.ni.k[6]);
		t = _mm_aesenc_si128(t,_k.ni.k[7]);
		t = _mm_aesenc_si128(t,_k.ni.k[8]);
		t = _mm_aesenc_si128(t,_k.ni.k[9]);
		t = _mm_aesenc_si128(t,_k.ni.k[10]);
		t = _mm_aesenc_si128(t,_k.ni.k[11]);
		t = _mm_aesenc_si128(t,_k.ni.k[12]);
		t = _mm_aesenc_si128(t,_k.ni.k[13]);
		t = _mm_aesenclast_si128(t,_k.ni.k[14]);
		b = _mm_xor_si128(t,b);
		memcpy(out,&b,rem);
		return y;
	}
	inline void _decrypt_gcm256_aesni(unsigned int len,const uint8_t *in,uint8_t *out,const uint8_t *iv,unsigned int alen,const uint8_t *assoc,uint8_t *icv,unsigned int icvsize)
	{
		__m128i d1,d2,d3,d4,t1,t2,t3,t4,k;
		__m128i y,j,cb,*bi,*bo;
		unsigned int blocks,pblocks,rem;

		j = _create_j_aesni(iv);
		cb = _increment_be_aesni(j);
		y = _icv_header_aesni(assoc,alen);
		blocks = len / 16;
		pblocks = blocks - (blocks % 4);
		rem = len % 16;
		bi = (__m128i *)in;
		bo = (__m128i *)out;

		unsigned int i;
		for (i=0;i<pblocks;i+=4) {
			d1 = _mm_loadu_si128(bi + i + 0);
			d2 = _mm_loadu_si128(bi + i + 1);
			d3 = _mm_loadu_si128(bi + i + 2);
			d4 = _mm_loadu_si128(bi + i + 3);
			y = _mm_xor_si128(y,d1);
			y = _mult4xor_aesni(_k.ni.hhhh,_k.ni.hhh,_k.ni.hh,_k.ni.h,y,d2,d3,d4);
			t1 = _mm_xor_si128(cb,k = _k.ni.k[0]);
			cb = _increment_be_aesni(cb);
			t2 = _mm_xor_si128(cb,k);
			cb = _increment_be_aesni(cb);
			t3 = _mm_xor_si128(cb,k);
			cb = _increment_be_aesni(cb);
			t4 = _mm_xor_si128(cb,k);
			cb = _increment_be_aesni(cb);
			t1 = _mm_aesenc_si128(t1,k = _k.ni.k[1]);
			t2 = _mm_aesenc_si128(t2,k);
			t3 = _mm_aesenc_si128(t3,k);
			t4 = _mm_aesenc_si128(t4,k);
			t1 = _mm_aesenc_si128(t1,k = _k.ni.k[2]);
			t2 = _mm_aesenc_si128(t2,k);
			t3 = _mm_aesenc_si128(t3,k);
			t4 = _mm_aesenc_si128(t4,k);
			t1 = _mm_aesenc_si128(t1,k = _k.ni.k[3]);
			t2 = _mm_aesenc_si128(t2,k);
			t3 = _mm_aesenc_si128(t3,k);
			t4 = _mm_aesenc_si128(t4,k);
			t1 = _mm_aesenc_si128(t1,k = _k.ni.k[4]);
			t2 = _mm_aesenc_si128(t2,k);
			t3 = _mm_aesenc_si128(t3,k);
			t4 = _mm_aesenc_si128(t4,k);
			t1 = _mm_aesenc_si128(t1,k = _k.ni.k[5]);
			t2 = _mm_aesenc_si128(t2,k);
			t3 = _mm_aesenc_si128(t3,k);
			t4 = _mm_aesenc_si128(t4,k);
			t1 = _mm_aesenc_si128(t1,k = _k.ni.k[6]);
			t2 = _mm_aesenc_si128(t2,k);
			t3 = _mm_aesenc_si128(t3,k);
			t4 = _mm_aesenc_si128(t4,k);
			t1 = _mm_aesenc_si128(t1,k = _k.ni.k[7]);
			t2 = _mm_aesenc_si128(t2,k);
			t3 = _mm_aesenc_si128(t3,k);
			t4 = _mm_aesenc_si128(t4,k);
			t1 = _mm_aesenc_si128(t1,k = _k.ni.k[8]);
			t2 = _mm_aesenc_si128(t2,k);
			t3 = _mm_aesenc_si128(t3,k);
			t4 = _mm_aesenc_si128(t4,k);
			t1 = _mm_aesenc_si128(t1,k = _k.ni.k[9]);
			t2 = _mm_aesenc_si128(t2,k);
			t3 = _mm_aesenc_si128(t3,k);
			t4 = _mm_aesenc_si128(t4,k);
			t1 = _mm_aesenc_si128(t1,k = _k.ni.k[10]);
			t2 = _mm_aesenc_si128(t2,k);
			t3 = _mm_aesenc_si128(t3,k);
			t4 = _mm_aesenc_si128(t4,k);
			t1 = _mm_aesenc_si128(t1,k = _k.ni.k[11]);
			t2 = _mm_aesenc_si128(t2,k);
			t3 = _mm_aesenc_si128(t3,k);
			t4 = _mm_aesenc_si128(t4,k);
			t1 = _mm_aesenc_si128(t1,k = _k.ni.k[12]);
			t2 = _mm_aesenc_si128(t2,k);
			t3 = _mm_aesenc_si128(t3,k);
			t4 = _mm_aesenc_si128(t4,k);
			t1 = _mm_aesenc_si128(t1,k = _k.ni.k[13]);
			t2 = _mm_aesenc_si128(t2,k);
			t3 = _mm_aesenc_si128(t3,k);
			t4 = _mm_aesenc_si128(t4,k);
			t1 = _mm_aesenclast_si128(t1,k = _k.ni.k[14]);
			t2 = _mm_aesenclast_si128(t2,k);
			t3 = _mm_aesenclast_si128(t3,k);
			t4 = _mm_aesenclast_si128(t4,k);
			t1 = _mm_xor_si128(t1,d1);
			t2 = _mm_xor_si128(t2,d2);
			t3 = _mm_xor_si128(t3,d3);
			t4 = _mm_xor_si128(t4,d4);
			_mm_storeu_si128(bo + i + 0,t1);
			_mm_storeu_si128(bo + i + 1,t2);
			_mm_storeu_si128(bo + i + 2,t3);
			_mm_storeu_si128(bo + i + 3,t4);
		}

		for (i=pblocks;i<blocks;i++) {
			d1 = _mm_loadu_si128(bi + i);
			y = _ghash_aesni(_k.ni.h,y,d1);
			t1 = _mm_xor_si128(cb,_k.ni.k[0]);
			t1 = _mm_aesenc_si128(t1,_k.ni.k[1]);
			t1 = _mm_aesenc_si128(t1,_k.ni.k[2]);
			t1 = _mm_aesenc_si128(t1,_k.ni.k[3]);
			t1 = _mm_aesenc_si128(t1,_k.ni.k[4]);
			t1 = _mm_aesenc_si128(t1,_k.ni.k[5]);
			t1 = _mm_aesenc_si128(t1,_k.ni.k[6]);
			t1 = _mm_aesenc_si128(t1,_k.ni.k[7]);
			t1 = _mm_aesenc_si128(t1,_k.ni.k[8]);
			t1 = _mm_aesenc_si128(t1,_k.ni.k[9]);
			t1 = _mm_aesenc_si128(t1,_k.ni.k[10]);
			t1 = _mm_aesenc_si128(t1,_k.ni.k[11]);
			t1 = _mm_aesenc_si128(t1,_k.ni.k[12]);
			t1 = _mm_aesenc_si128(t1,_k.ni.k[13]);
			t1 = _mm_aesenclast_si128(t1,_k.ni.k[14]);
			t1 = _mm_xor_si128(t1,d1);
			_mm_storeu_si128(bo + i,t1);
			cb = _increment_be_aesni(cb);
		}

		if (rem)
			y = _decrypt_gcm_rem_aesni(rem,bi + blocks,bo + blocks,cb,y);
		y = _icv_tailer_aesni(y,alen,len);
		_icv_crypt_aesni(y,j,icv,icvsize);
	}
#endif /* ZT_AES_AESNI ******************************************************/
};

} // namespace ZeroTier

#endif
