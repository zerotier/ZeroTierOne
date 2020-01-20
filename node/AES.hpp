/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2023-01-01
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
	ZT_ALWAYS_INLINE AES() {}
	ZT_ALWAYS_INLINE AES(const uint8_t key[32]) { this->init(key); }
	ZT_ALWAYS_INLINE ~AES() { Utils::burn(&_k,sizeof(_k)); }

	/**
	 * Set (or re-set) this AES256 cipher's key
	 */
	ZT_ALWAYS_INLINE void init(const uint8_t key[32])
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
	ZT_ALWAYS_INLINE void encrypt(const uint8_t in[16],uint8_t out[16]) const
	{
#ifdef ZT_AES_AESNI
		if (likely(Utils::CPUID.aes)) {
			_encrypt_aesni(in,out);
			return;
		}
#endif
		_encryptSW(in,out);
	}

	/**
	 * Compute GMAC-AES256 (GCM without ciphertext)
	 *
	 * @param iv 96-bit IV
	 * @param in Input data
	 * @param len Length of input
	 * @param out 128-bit authorization tag from GMAC
	 */
	ZT_ALWAYS_INLINE void gmac(const uint8_t iv[12],const void *in,const unsigned int len,uint8_t out[16]) const
	{
#ifdef ZT_AES_AESNI
		if (likely(Utils::CPUID.aes)) {
			_gmac_aesni(iv,(const uint8_t *)in,len,out);
			return;
		}
#endif
		_gmacSW(iv,(const uint8_t *)in,len,out);
	}

	/**
	 * Encrypt or decrypt (they're the same) using AES256-CTR
	 *
	 * The counter here is a 128-bit big-endian that starts at the IV. The code only
	 * increments the least significant 64 bits, making it only safe to use for a
	 * maximum of 2^64-1 bytes (much larger than we ever do).
	 *
	 * @param iv 128-bit CTR IV
	 * @param in Input plaintext or ciphertext
	 * @param len Length of input
	 * @param out Output plaintext or ciphertext
	 */
	ZT_ALWAYS_INLINE void ctr(const uint8_t iv[16],const void *in,unsigned int len,void *out) const
	{
#ifdef ZT_AES_AESNI
		if (likely(Utils::CPUID.aes)) {
			_ctr_aesni(_k.ni.k,iv,(const uint8_t *)in,len,(uint8_t *)out);
			return;
		}
#endif
		_ctrSW(iv,in,len,out);
	}

	/**
	 * Perform AES-GMAC-SIV encryption
	 *
	 * This is basically AES-CMAC-SIV but with GMAC in place of CMAC after
	 * GMAC is run through AES as a keyed hash to make it behave like a
	 * proper PRF.
	 *
	 * See: https://github.com/miscreant/meta/wiki/AES-SIV
	 *
	 * The advantage is that this can be described in terms of FIPS and NSA
	 * ceritifable primitives that are present in FIPS-compliant crypto
	 * modules.
	 *
	 * The extra AES-ECB (keyed hash) encryption of the AES-CTR IV prior
	 * to use makes the IV itself a secret. This is not strictly necessary
	 * but comes at little cost.
	 *
	 * This code is ZeroTier-specific in a few ways, like the way the IV
	 * is specified, but would not be hard to generalize.
	 *
	 * @param k1 GMAC key
	 * @param k2 GMAC auth tag keyed hash key
	 * @param k3 CTR IV keyed hash key
	 * @param k4 AES-CTR key
	 * @param iv 64-bit packet IV
	 * @param pc Packet characteristics byte
	 * @param in Message plaintext
	 * @param len Length of plaintext
	 * @param out Output buffer to receive ciphertext
	 * @param tag Output buffer to receive 64-bit authentication tag
	 */
	static inline void gmacSivEncrypt(const AES &k1,const AES &k2,const AES &k3,const AES &k4,const uint8_t iv[8],const uint8_t pc,const void *in,const unsigned int len,void *out,uint8_t tag[8])
	{
#ifdef __GNUC__
		uint8_t __attribute__ ((aligned (16))) miv[12];
		uint8_t __attribute__ ((aligned (16))) ctrIv[16];
#else
		uint8_t miv[12];
		uint8_t ctrIv[16];
#endif

		// GMAC IV is 64-bit packet IV followed by other packet attributes to extend to 96 bits
#ifndef __GNUC__
		for(unsigned int i=0;i<8;++i) miv[i] = iv[i];
#else
		*((uint64_t *)miv) = *((const uint64_t *)iv);
#endif
		miv[8] = pc;
		miv[9] = (uint8_t)(len >> 16);
		miv[10] = (uint8_t)(len >> 8);
		miv[11] = (uint8_t)len;

		// Compute auth tag: AES-ECB[k2](GMAC[k1](miv,plaintext))[0:8]
		k1.gmac(miv,in,len,ctrIv);
		k2.encrypt(ctrIv,ctrIv); // ECB mode encrypt step is because GMAC is not a PRF
#ifdef ZT_NO_TYPE_PUNNING
		for(unsigned int i=0;i<8;++i) tag[i] = ctrIv[i];
#else
		*((uint64_t *)tag) = *((uint64_t *)ctrIv);
#endif

		// Create synthetic CTR IV: AES-ECB[k3](TAG | MIV[0:4] | (MIV[4:8] XOR MIV[8:12]))
#ifndef __GNUC__
		for(unsigned int i=0;i<4;++i) ctrIv[i+8] = miv[i];
		for(unsigned int i=4;i<8;++i) ctrIv[i+8] = miv[i] ^ miv[i+4];
#else
		((uint32_t *)ctrIv)[2] = ((const uint32_t *)miv)[0];
		((uint32_t *)ctrIv)[3] = ((const uint32_t *)miv)[1] ^ ((const uint32_t *)miv)[2];
#endif
		k3.encrypt(ctrIv,ctrIv);

		// Encrypt with AES[k4]-CTR
		k4.ctr(ctrIv,in,len,out);
	}

	/**
	 * Decrypt a message encrypted with AES-GMAC-SIV and check its authenticity
	 *
	 * @param k1 GMAC key
	 * @param k2 GMAC auth tag keyed hash key
	 * @param k3 CTR IV keyed hash key
	 * @param k4 AES-CTR key
	 * @param iv 64-bit message IV
	 * @param pc Packet characteristics byte
	 * @param in Message ciphertext
	 * @param len Length of ciphertext
	 * @param out Output buffer to receive plaintext
	 * @param tag Authentication tag supplied with message
	 * @return True if authentication tags match and message appears authentic
	 */
	static inline bool gmacSivDecrypt(const AES &k1,const AES &k2,const AES &k3,const AES &k4,const uint8_t iv[8],const uint8_t pc,const void *in,const unsigned int len,void *out,const uint8_t tag[8])
	{
#ifdef __GNUC__
		uint8_t __attribute__ ((aligned (16))) miv[12];
		uint8_t __attribute__ ((aligned (16))) ctrIv[16];
		uint8_t __attribute__ ((aligned (16))) gmacOut[16];
#else
		uint8_t miv[12];
		uint8_t ctrIv[16];
		uint8_t gmacOut[16];
#endif

		// Extend packet IV to 96-bit message IV using direction byte and message length
#ifdef ZT_NO_TYPE_PUNNING
		for(unsigned int i=0;i<8;++i) miv[i] = iv[i];
#else
		*((uint64_t *)miv) = *((const uint64_t *)iv);
#endif
		miv[8] = pc;
		miv[9] = (uint8_t)(len >> 16);
		miv[10] = (uint8_t)(len >> 8);
		miv[11] = (uint8_t)len;

		// Recover synthetic and secret CTR IV from auth tag and packet IV
#ifndef __GNUC__
		for(unsigned int i=0;i<8;++i) ctrIv[i] = tag[i];
		for(unsigned int i=0;i<4;++i) ctrIv[i+8] = miv[i];
		for(unsigned int i=4;i<8;++i) ctrIv[i+8] = miv[i] ^ miv[i+4];
#else
		*((uint64_t *)ctrIv) = *((const uint64_t *)tag);
		((uint32_t *)ctrIv)[2] = ((const uint32_t *)miv)[0];
		((uint32_t *)ctrIv)[3] = ((const uint32_t *)miv)[1] ^ ((const uint32_t *)miv)[2];
#endif
		k3.encrypt(ctrIv,ctrIv);

		// Decrypt with AES[k4]-CTR
		k4.ctr(ctrIv,in,len,out);

		// Compute AES[k2](GMAC[k1](iv,plaintext))
		k1.gmac(miv,out,len,gmacOut);
		k2.encrypt(gmacOut,gmacOut);

		// Check that packet's auth tag matches first 64 bits of AES(GMAC)
#ifdef ZT_NO_TYPE_PUNNING
		return Utils::secureEq(gmacOut,tag,8);
#else
		return (*((const uint64_t *)gmacOut) == *((const uint64_t *)tag));
#endif
	}

	/**
	 * Use KBKDF with HMAC-SHA-384 to derive four sub-keys for AES-GMAC-SIV from a single master key
	 *
	 * See section 5.1 at https://nvlpubs.nist.gov/nistpubs/Legacy/SP/nistspecialpublication800-108.pdf
	 *
	 * @param masterKey Master 256-bit key
	 * @param k1 GMAC key
	 * @param k2 GMAC auth tag keyed hash key
	 * @param k3 CTR IV keyed hash key
	 * @param k4 AES-CTR key
	 */
	static inline void initGmacCtrKeys(const uint8_t masterKey[32],AES &k1,AES &k2,AES &k3,AES &k4)
	{
		uint8_t k[32];
		KBKDFHMACSHA384(masterKey,ZT_PROTO_KBKDF_LABEL_KEY_USE_AES_GMAC_SIV_K1,0,0,k);
		k1.init(k);
		KBKDFHMACSHA384(masterKey,ZT_PROTO_KBKDF_LABEL_KEY_USE_AES_GMAC_SIV_K2,0,0,k);
		k2.init(k);
		KBKDFHMACSHA384(masterKey,ZT_PROTO_KBKDF_LABEL_KEY_USE_AES_GMAC_SIV_K3,0,0,k);
		k3.init(k);
		KBKDFHMACSHA384(masterKey,ZT_PROTO_KBKDF_LABEL_KEY_USE_AES_GMAC_SIV_K4,0,0,k);
		k4.init(k);
	}

private:
	static const uint32_t Te0[256];
	static const uint32_t Te1[256];
	static const uint32_t Te2[256];
	static const uint32_t Te3[256];
	static const uint32_t rcon[10];

	void _initSW(const uint8_t key[32]);
	void _encryptSW(const uint8_t in[16],uint8_t out[16]) const;
	void _ctrSW(const uint8_t iv[16],const void *in,unsigned int len,void *out) const;
	void _gmacSW(const uint8_t iv[12],const uint8_t *in,unsigned int len,uint8_t out[16]) const;

	/**************************************************************************/
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
	/**************************************************************************/

#ifdef ZT_AES_ARMNEON /******************************************************/
	static inline void _aes_256_expAssist_armneon(uint32x4_t prev1,uint32x4_t prev2,uint32_t rcon,uint32x4_t *e1,uint32x4_t *e2)
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
	inline void _init_armneon(uint8x16_t encKey)
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

	inline void _encrypt_armneon(uint8x16_t *data) const
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
#endif /*********************************************************************/

#ifdef ZT_AES_AESNI /********************************************************/
	void _init_aesni(const uint8_t key[32]);

	ZT_ALWAYS_INLINE void _encrypt_aesni(const void *in,void *out) const
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

	void _gmac_aesni(const uint8_t iv[12],const uint8_t *in,const unsigned int len,uint8_t out[16]) const;
	static void _ctr_aesni(const __m128i key[14],const uint8_t iv[16],const uint8_t *in,unsigned int len,uint8_t *out);
#endif /* ZT_AES_AESNI ******************************************************/
};

} // namespace ZeroTier

#endif
