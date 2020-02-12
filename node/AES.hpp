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

#ifndef ZT_AES_HPP
#define ZT_AES_HPP

#include "Constants.hpp"
#include "Utils.hpp"
#include "SHA512.hpp"

#include <cstdint>

#if (defined(__amd64) || defined(__amd64__) || defined(__x86_64) || defined(__x86_64__) || defined(__AMD64) || defined(__AMD64__) || defined(_M_X64))
#include <xmmintrin.h>
#include <wmmintrin.h>
#include <emmintrin.h>
#include <smmintrin.h>
#define ZT_AES_AESNI 1
#endif

namespace ZeroTier {

/**
 * AES-256 and pals including GMAC, CTR, etc.
 */
class AES
{
public:
	ZT_ALWAYS_INLINE AES() noexcept {}
	explicit ZT_ALWAYS_INLINE AES(const uint8_t key[32]) noexcept { this->init(key); }
	ZT_ALWAYS_INLINE ~AES() { Utils::burn(&_k,sizeof(_k)); }

	/**
	 * Set (or re-set) this AES256 cipher's key
	 */
	ZT_ALWAYS_INLINE void init(const uint8_t key[32]) noexcept
	{
#ifdef ZT_AES_AESNI
		if (likely(Utils::CPUID.aes)) {
			_init_aesni(key);
			return;
		}
#endif
		_initSW(key);
	}

	/**
	 * Encrypt a single AES block (ECB mode)
	 *
	 * @param in Input block
	 * @param out Output block (can be same as input)
	 */
	ZT_ALWAYS_INLINE void encrypt(const uint8_t in[16],uint8_t out[16]) const noexcept
	{
#ifdef ZT_AES_AESNI
		if (likely(Utils::CPUID.aes)) {
			_encrypt_aesni(in,out);
			return;
		}
#endif
		_encryptSW(in,out);
	}

private:
	static const uint32_t Te0[256];
	static const uint32_t Te1[256];
	static const uint32_t Te2[256];
	static const uint32_t Te3[256];
	static const uint32_t rcon[10];

	void _initSW(const uint8_t key[32]) noexcept;
	void _encryptSW(const uint8_t in[16],uint8_t out[16]) const noexcept;
	void _gmacSW(const uint8_t iv[12],const uint8_t *in,unsigned int len,uint8_t out[16]) const noexcept;

	union {
#ifdef ZT_AES_ARMNEON
		// ARM NEON key and GMAC parameters
		struct {
			uint32x4_t k[15];
		} neon;
#endif

#ifdef ZT_AES_AESNI
		// AES-NI key and GMAC parameters
		struct {
			__m128i k[15];
			__m128i h,hh,hhh,hhhh;
		} ni;
#endif

		// Software mode key and GMAC parameters
		struct {
			uint64_t h[2];
			uint32_t ek[60];
		} sw;
	} _k;

#ifdef ZT_AES_ARMNEON
	static inline void _aes_256_expAssist_armneon(uint32x4_t prev1,uint32x4_t prev2,uint32_t rcon,uint32x4_t *e1,uint32x4_t *e2) noexcept
	{
		uint32_t round1[4], round2[4], prv1[4], prv2[4];
		vst1q_u32(prv1, prev1);
		vst1q_u32(prv2, prev2);
		round1[0] = sub_word(rot_word(prv2[3])) ^ rcon ^ prv1[0];
		round1[1] = sub_word(rot_word(round1[0])) ^ rcon ^ prv1[1];
		round1[2] = sub_word(rot_word(round1[1])) ^ rcon ^ prv1[2];
		round1[3] = sub_word(rot_word(round1[2])) ^ rcon ^ prv1[3];
		round2[0] = sub_word(rot_word(round1[3])) ^ rcon ^ prv2[0];
		round2[1] = sub_word(rot_word(round2[0])) ^ rcon ^ prv2[1];
		round2[2] = sub_word(rot_word(round2[1])) ^ rcon ^ prv2[2];
		round2[3] = sub_word(rot_word(round2[2])) ^ rcon ^ prv2[3];
		*e1 = vld1q_u3(round1);
		*e2 = vld1q_u3(round2);
		//uint32x4_t expansion[2] = {vld1q_u3(round1), vld1q_u3(round2)};
		//return expansion;
	}

	inline void _init_armneon(uint8x16_t encKey) noexcept
	{
		uint32x4_t *schedule = _k.neon.k;
		uint32x4_t e1,e2;
		(*schedule)[0] = vld1q_u32(encKey);
		(*schedule)[1] = vld1q_u32(encKey + 16);
		_aes_256_expAssist_armneon((*schedule)[0],(*schedule)[1],0x01,&e1,&e2);
		(*schedule)[2] = e1; (*schedule)[3] = e2;
		_aes_256_expAssist_armneon((*schedule)[2],(*schedule)[3],0x01,&e1,&e2);
		(*schedule)[4] = e1; (*schedule)[5] = e2;
		_aes_256_expAssist_armneon((*schedule)[4],(*schedule)[5],0x01,&e1,&e2);
		(*schedule)[6] = e1; (*schedule)[7] = e2;
		_aes_256_expAssist_armneon((*schedule)[6],(*schedule)[7],0x01,&e1,&e2);
		(*schedule)[8] = e1; (*schedule)[9] = e2;
		_aes_256_expAssist_armneon((*schedule)[8],(*schedule)[9],0x01,&e1,&e2);
		(*schedule)[10] = e1; (*schedule)[11] = e2;
		_aes_256_expAssist_armneon((*schedule)[10],(*schedule)[11],0x01,&e1,&e2);
		(*schedule)[12] = e1; (*schedule)[13] = e2;
		_aes_256_expAssist_armneon((*schedule)[12],(*schedule)[13],0x01,&e1,&e2);
		(*schedule)[14] = e1;
		/*
		doubleRound = _aes_256_expAssist_armneon((*schedule)[0], (*schedule)[1], 0x01);
		(*schedule)[2] = doubleRound[0];
		(*schedule)[3] = doubleRound[1];
		doubleRound = _aes_256_expAssist_armneon((*schedule)[2], (*schedule)[3], 0x02);
		(*schedule)[4] = doubleRound[0];
		(*schedule)[5] = doubleRound[1];
		doubleRound = _aes_256_expAssist_armneon((*schedule)[4], (*schedule)[5], 0x04);
		(*schedule)[6] = doubleRound[0];
		(*schedule)[7] = doubleRound[1];
		doubleRound = _aes_256_expAssist_armneon((*schedule)[6], (*schedule)[7], 0x08);
		(*schedule)[8] = doubleRound[0];
		(*schedule)[9] = doubleRound[1];
		doubleRound = _aes_256_expAssist_armneon((*schedule)[8], (*schedule)[9], 0x10);
		(*schedule)[10] = doubleRound[0];
		(*schedule)[11] = doubleRound[1];
		doubleRound = _aes_256_expAssist_armneon((*schedule)[10], (*schedule)[11], 0x20);
		(*schedule)[12] = doubleRound[0];
		(*schedule)[13] = doubleRound[1];
		doubleRound = _aes_256_expAssist_armneon((*schedule)[12], (*schedule)[13], 0x40);
		(*schedule)[14] = doubleRound[0];
		*/
	}

	inline void _encrypt_armneon(uint8x16_t *data) const noexcept
	{
		*data = veorq_u8(*data, _k.neon.k[0]);
		*data = vaesmcq_u8(vaeseq_u8(*data, (uint8x16_t)_k.neon.k[1]));
		*data = vaesmcq_u8(vaeseq_u8(*data, (uint8x16_t)_k.neon.k[2]));
		*data = vaesmcq_u8(vaeseq_u8(*data, (uint8x16_t)_k.neon.k[3]));
		*data = vaesmcq_u8(vaeseq_u8(*data, (uint8x16_t)_k.neon.k[4]));
		*data = vaesmcq_u8(vaeseq_u8(*data, (uint8x16_t)_k.neon.k[5]));
		*data = vaesmcq_u8(vaeseq_u8(*data, (uint8x16_t)_k.neon.k[6]));
		*data = vaesmcq_u8(vaeseq_u8(*data, (uint8x16_t)_k.neon.k[7]));
		*data = vaesmcq_u8(vaeseq_u8(*data, (uint8x16_t)_k.neon.k[8]));
		*data = vaesmcq_u8(vaeseq_u8(*data, (uint8x16_t)_k.neon.k[9]));
		*data = vaesmcq_u8(vaeseq_u8(*data, (uint8x16_t)_k.neon.k[10]));
		*data = vaesmcq_u8(vaeseq_u8(*data, (uint8x16_t)_k.neon.k[11]));
		*data = vaesmcq_u8(vaeseq_u8(*data, (uint8x16_t)_k.neon.k[12]));
		*data = vaesmcq_u8(vaeseq_u8(*data, (uint8x16_t)_k.neon.k[13]));
		*data = vaeseq_u8(*data, _k.neon.k[14]);
	}
#endif

#ifdef ZT_AES_AESNI
	void _init_aesni(const uint8_t key[32]) noexcept;

	ZT_ALWAYS_INLINE void _encrypt_aesni(const void *const in,void *const out) const noexcept
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

	void _gmac_aesni(const uint8_t iv[12],const uint8_t *in,unsigned int len,uint8_t out[16]) const noexcept;
#endif
};

} // namespace ZeroTier

#endif
