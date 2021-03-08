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

#ifndef ZT_AES_HPP
#define ZT_AES_HPP

#include "Constants.hpp"
#include "Utils.hpp"
#include "SHA512.hpp"

// Uncomment to disable all hardware acceleration (usually for testing)
//#define ZT_AES_NO_ACCEL

#if !defined(ZT_AES_NO_ACCEL) && defined(ZT_ARCH_X64)
#define ZT_AES_AESNI 1
#endif
#if !defined(ZT_AES_NO_ACCEL) && defined(ZT_ARCH_ARM_HAS_NEON)
#define ZT_AES_NEON 1
#endif

#ifndef ZT_INLINE
#define ZT_INLINE inline
#endif

namespace ZeroTier {

/**
 * AES-256 and pals including GMAC, CTR, etc.
 *
 * This includes hardware acceleration for certain processors. The software
 * mode is fallback and is significantly slower.
 */
class AES
{
public:
	/**
	 * @return True if this system has hardware AES acceleration
	 */
	static ZT_INLINE bool accelerated()
	{
#ifdef ZT_AES_NO_ACCEL
		return false;
#else
#ifdef ZT_AES_AESNI
		return Utils::CPUID.aes;
#endif
#ifdef ZT_AES_NEON
		return Utils::ARMCAP.aes;
#endif
#endif
	}

	/**
	 * Create an un-initialized AES instance (must call init() before use)
	 */
	ZT_INLINE AES() noexcept
	{}

	/**
	 * Create an AES instance with the given key
	 *
	 * @param key 256-bit key
	 */
	explicit ZT_INLINE AES(const void *const key) noexcept
	{ this->init(key); }

	ZT_INLINE ~AES()
	{ Utils::burn(&p_k, sizeof(p_k)); }

	/**
	 * Set (or re-set) this AES256 cipher's key
	 *
	 * @param key 256-bit / 32-byte key
	 */
	ZT_INLINE void init(const void *const key) noexcept
	{
#ifdef ZT_AES_AESNI
		if (likely(Utils::CPUID.aes)) {
			p_init_aesni(reinterpret_cast<const uint8_t *>(key));
			return;
		}
#endif
#ifdef ZT_AES_NEON
		if (Utils::ARMCAP.aes) {
			p_init_armneon_crypto(reinterpret_cast<const uint8_t *>(key));
			return;
		}
#endif
		p_initSW(reinterpret_cast<const uint8_t *>(key));
	}

	/**
	 * Encrypt a single AES block
	 *
	 * @param in Input block
	 * @param out Output block (can be same as input)
	 */
	ZT_INLINE void encrypt(const void *const in, void *const out) const noexcept
	{
#ifdef ZT_AES_AESNI
		if (likely(Utils::CPUID.aes)) {
			p_encrypt_aesni(in, out);
			return;
		}
#endif
#ifdef ZT_AES_NEON
		if (Utils::ARMCAP.aes) {
			p_encrypt_armneon_crypto(in, out);
			return;
		}
#endif
		p_encryptSW(reinterpret_cast<const uint8_t *>(in), reinterpret_cast<uint8_t *>(out));
	}

	/**
	 * Decrypt a single AES block
	 *
	 * @param in Input block
	 * @param out Output block (can be same as input)
	 */
	ZT_INLINE void decrypt(const void *const in, void *const out) const noexcept
	{
#ifdef ZT_AES_AESNI
		if (likely(Utils::CPUID.aes)) {
			p_decrypt_aesni(in, out);
			return;
		}
#endif
#ifdef ZT_AES_NEON
		if (Utils::ARMCAP.aes) {
			p_decrypt_armneon_crypto(in, out);
			return;
		}
#endif
		p_decryptSW(reinterpret_cast<const uint8_t *>(in), reinterpret_cast<uint8_t *>(out));
	}

	class GMACSIVEncryptor;
	class GMACSIVDecryptor;

	/**
	 * Streaming GMAC calculator
	 */
	class GMAC
	{
		friend class GMACSIVEncryptor;
		friend class GMACSIVDecryptor;

	public:
		/**
		 * @return True if this system has hardware GMAC acceleration
		 */
		static ZT_INLINE bool accelerated()
		{
#ifdef ZT_AES_AESNI
			return Utils::CPUID.aes;
#else
#ifdef ZT_AES_NEON
			return Utils::ARMCAP.pmull;
#else
			return false;
#endif
#endif
		}

		/**
		 * Create a new instance of GMAC (must be initialized with init() before use)
		 *
		 * @param aes Keyed AES instance to use
		 */
		ZT_INLINE GMAC(const AES &aes) : _aes(aes)
		{}

		/**
		 * Reset and initialize for a new GMAC calculation
		 *
		 * @param iv 96-bit initialization vector (pad with zeroes if actual IV is shorter)
		 */
		ZT_INLINE void init(const uint8_t iv[12]) noexcept
		{
			_rp = 0;
			_len = 0;

			// We fill the least significant 32 bits in the _iv field with 1 since in GCM mode
			// this would hold the counter, but we're not doing GCM just GMAC. That means the
			// counter always stays just 1.
#ifdef ZT_AES_AESNI // also implies an x64 processor
			*reinterpret_cast<uint64_t *>(_iv) = *reinterpret_cast<const uint64_t *>(iv);
			*reinterpret_cast<uint32_t *>(_iv + 8) = *reinterpret_cast<const uint64_t *>(iv + 8);
			*reinterpret_cast<uint32_t *>(_iv + 12) = 0x01000000; // 0x00000001 in big-endian byte order
#else
			Utils::copy<12>(_iv, iv);
			_iv[12] = 0;
			_iv[13] = 0;
			_iv[14] = 0;
			_iv[15] = 1;
#endif

			_y[0] = 0;
			_y[1] = 0;
		}

		/**
		 * Process data through GMAC
		 *
		 * @param data Bytes to process
		 * @param len Length of input
		 */
		void update(const void *data, unsigned int len) noexcept;

		/**
		 * Process any remaining cached bytes and generate tag
		 *
		 * Don't call finish() more than once or you'll get an invalid result.
		 *
		 * @param tag 128-bit GMAC tag (can be truncated)
		 */
		void finish(uint8_t tag[16]) noexcept;

	private:
#ifdef ZT_AES_AESNI
		void p_aesNIUpdate(const uint8_t *in, unsigned int len) noexcept;
		void p_aesNIFinish(uint8_t tag[16]) noexcept;
#endif
#ifdef ZT_AES_NEON
		void p_armUpdate(const uint8_t *in, unsigned int len) noexcept;
		void p_armFinish(uint8_t tag[16]) noexcept;
#endif
		const AES &_aes;
		unsigned int _rp;
		unsigned int _len;
		uint8_t _r[16]; // remainder
		uint8_t _iv[16];
		uint64_t _y[2];
	};

	/**
	 * Streaming AES-CTR encrypt/decrypt
	 *
	 * NOTE: this doesn't support overflow of the counter in the least significant 32 bits.
	 * We will never encrypt more than a tiny fraction of 2^32 blocks, so this is left out as
	 * an optimization.
	 */
	class CTR
	{
		friend class GMACSIVEncryptor;
		friend class GMACSIVDecryptor;

	public:
		ZT_INLINE CTR(const AES &aes) noexcept: _aes(aes)
		{}

		/**
		 * Initialize this CTR instance to encrypt a new stream
		 *
		 * @param iv Unique initialization vector and initial 32-bit counter (least significant 32 bits, big-endian)
		 * @param output Buffer to which to store output (MUST be large enough for total bytes processed!)
		 */
		ZT_INLINE void init(const uint8_t iv[16], void *const output) noexcept
		{
			Utils::copy< 16 >(_ctr, iv);
			_out = reinterpret_cast<uint8_t *>(output);
			_len = 0;
		}

		/**
		 * Initialize this CTR instance to encrypt a new stream
		 *
		 * @param iv Unique initialization vector
		 * @param ic Initial counter (must be in big-endian byte order!)
		 * @param output Buffer to which to store output (MUST be large enough for total bytes processed!)
		 */
		ZT_INLINE void init(const uint8_t iv[12], const uint32_t ic, void *const output) noexcept
		{
			Utils::copy< 12 >(_ctr, iv);
			reinterpret_cast<uint32_t *>(_ctr)[3] = ic;
			_out = reinterpret_cast<uint8_t *>(output);
			_len = 0;
		}

		/**
		 * Encrypt or decrypt data, writing result to the output provided to init()
		 *
		 * @param input Input data
		 * @param len Length of input
		 */
		void crypt(const void *input, unsigned int len) noexcept;

		/**
		 * Finish any remaining bytes if total bytes processed wasn't a multiple of 16
		 *
		 * Don't call more than once for a given stream or data may be corrupted.
		 */
		void finish() noexcept;

	private:
#ifdef ZT_AES_AESNI
		void p_aesNICrypt(const uint8_t *in, uint8_t *out, unsigned int len) noexcept;
#endif
#ifdef ZT_AES_NEON
		void p_armCrypt(const uint8_t *in, uint8_t *out, unsigned int len) noexcept;
#endif
		const AES &_aes;
		uint64_t _ctr[2];
		uint8_t *_out;
		unsigned int _len;
	};

	/**
	 * Encryptor for AES-GMAC-SIV.
	 *
	 * Encryption requires two passes. The first pass starts after init
	 * with aad (if any) followed by update1() and finish1(). Then the
	 * update2() and finish2() methods must be used over the same data
	 * (but NOT AAD) again.
	 *
	 * This supports encryption of a maximum of 2^31 bytes of data per
	 * call to init().
	 */
	class GMACSIVEncryptor
	{
	public:
		/**
		 * Create a new AES-GMAC-SIV encryptor keyed with the provided AES instances
		 *
		 * @param k0 First of two AES instances keyed with K0
		 * @param k1 Second of two AES instances keyed with K1
		 */
		ZT_INLINE GMACSIVEncryptor(const AES &k0, const AES &k1) noexcept :
			_gmac(k0),
			_ctr(k1)
		{}

		/**
		 * Initialize AES-GMAC-SIV
		 *
		 * @param iv IV in network byte order (byte order in which it will appear on the wire)
		 * @param output Pointer to buffer to receive ciphertext, must be large enough for all to-be-processed data!
		 */
		ZT_INLINE void init(const uint64_t iv, void *const output) noexcept
		{
			// Output buffer to receive the result of AES-CTR encryption.
			_output = output;

			// Initialize GMAC with 64-bit IV (and remaining 32 bits padded to zero).
			_tag[0] = iv;
			_tag[1] = 0;
			_gmac.init(reinterpret_cast<const uint8_t *>(_tag));
		}

		/**
		 * Process AAD (additional authenticated data) that is not being encrypted.
		 *
		 * This MUST be called before update1() and finish1() if there is AAD to
		 * be included. This also MUST NOT be called more than once as the current
		 * code only supports one chunk of AAD.
		 *
		 * @param aad Additional authenticated data
		 * @param len Length of AAD in bytes
		 */
		ZT_INLINE void aad(const void *const aad, unsigned int len) noexcept
		{
			// Feed ADD into GMAC first
			_gmac.update(aad, len);

			// End of AAD is padded to a multiple of 16 bytes to ensure unique encoding.
			len &= 0xfU;
			if (len != 0)
				_gmac.update(Utils::ZERO256, 16U - len);
		}

		/**
		 * First pass plaintext input function
		 *
		 * @param input Plaintext chunk
		 * @param len Length of plaintext chunk
		 */
		ZT_INLINE void update1(const void *const input, const unsigned int len) noexcept
		{ _gmac.update(input, len); }

		/**
		 * Finish first pass, compute CTR IV, initialize second pass.
		 */
		ZT_INLINE void finish1() noexcept
		{
			// Compute 128-bit GMAC tag.
			uint64_t tmp[2];
			_gmac.finish(reinterpret_cast<uint8_t *>(tmp));

			// Shorten to 64 bits, concatenate with message IV, and encrypt with AES to
			// yield the CTR IV and opaque IV/MAC blob. In ZeroTier's use of GMAC-SIV
			// this get split into the packet ID (64 bits) and the MAC (64 bits) in each
			// packet and then recombined on receipt for legacy reasons (but with no
			// cryptographic or performance impact).
			_tag[1] = tmp[0] ^ tmp[1]; // NOTE: _tag[0] already contains message IV, see init()
			_ctr._aes.encrypt(_tag, _tag);

			// Initialize CTR with 96-bit CTR nonce and 32-bit counter. The counter
			// incorporates 31 more bits of entropy which should raise our security margin
			// a bit, but this is not included in the worst case analysis of GMAC-SIV.
			// The most significant bit of the counter is masked to zero to allow up to
			// 2^31 bytes to be encrypted before the counter loops. Some CTR implementations
			// increment the whole big-endian 128-bit integer in which case this could be
			// used for more than 2^31 bytes, but ours does not for performance reasons
			// and so 2^31 should be considered the input limit.
			tmp[0] = _tag[0];
			tmp[1] = _tag[1] & ZT_CONST_TO_BE_UINT64(0xffffffff7fffffffULL);
			_ctr.init(reinterpret_cast<const uint8_t *>(tmp), _output);
		}

		/**
		 * Second pass plaintext input function
		 *
		 * The same plaintext must be fed in the second time. Chunk boundaries
		 * (between calls to update2()) do not have to be the same, just the order
		 * of the bytes.
		 *
		 * @param input Plaintext chunk
		 * @param len Length of plaintext chunk
		 */
		ZT_INLINE void update2(const void *const input, const unsigned int len) noexcept
		{ _ctr.crypt(input, len); }

		/**
		 * Finish second pass and return a pointer to the opaque 128-bit IV+MAC block
		 *
		 * The returned pointer remains valid as long as this object exists and init()
		 * is not called again.
		 *
		 * @return Pointer to 128-bit opaque IV+MAC (packed into two 64-bit integers)
		 */
		ZT_INLINE const uint64_t *finish2()
		{
			_ctr.finish();
			return _tag;
		}

	private:
		void *_output;
		uint64_t _tag[2];
		AES::GMAC _gmac;
		AES::CTR _ctr;
	};

	/**
	 * Decryptor for AES-GMAC-SIV.
	 *
	 * GMAC-SIV decryption is single-pass. AAD (if any) must be processed first.
	 */
	class GMACSIVDecryptor
	{
	public:
		ZT_INLINE GMACSIVDecryptor(const AES &k0, const AES &k1) noexcept:
			_ctr(k1),
			_gmac(k0)
		{}

		/**
		 * Initialize decryptor for a new message
		 *
		 * @param tag 128-bit combined IV/MAC originally created by GMAC-SIV encryption
		 * @param output Buffer in which to write output plaintext (must be large enough!)
		 */
		ZT_INLINE void init(const uint64_t tag[2], void *const output) noexcept
		{
			uint64_t tmp[2];
			tmp[0] = tag[0];
			tmp[1] = tag[1] & ZT_CONST_TO_BE_UINT64(0xffffffff7fffffffULL);
			_ctr.init(reinterpret_cast<const uint8_t *>(tmp), output);

			_ctr._aes.decrypt(tag, _ivMac);

			tmp[0] = _ivMac[0];
			tmp[1] = 0;
			_gmac.init(reinterpret_cast<const uint8_t *>(tmp));

			_output = output;
			_decryptedLen = 0;
		}

		/**
		 * Process AAD (additional authenticated data) that wasn't encrypted
		 *
		 * @param aad Additional authenticated data
		 * @param len Length of AAD in bytes
		 */
		ZT_INLINE void aad(const void *const aad, unsigned int len) noexcept
		{
			_gmac.update(aad, len);
			len &= 0xfU;
			if (len != 0)
				_gmac.update(Utils::ZERO256, 16 - len);
		}

		/**
		 * Feed ciphertext into the decryptor
		 *
		 * Unlike encryption, GMAC-SIV decryption requires only one pass.
		 *
		 * @param input Input ciphertext
		 * @param len Length of ciphertext
		 */
		ZT_INLINE void update(const void *const input, const unsigned int len) noexcept
		{
			_ctr.crypt(input, len);
			_decryptedLen += len;
		}

		/**
		 * Flush decryption, compute MAC, and verify
		 *
		 * @return True if resulting plaintext (and AAD) pass message authentication check
		 */
		ZT_INLINE bool finish() noexcept
		{
			_ctr.finish();

			uint64_t gmacTag[2];
			_gmac.update(_output, _decryptedLen);
			_gmac.finish(reinterpret_cast<uint8_t *>(gmacTag));
			return (gmacTag[0] ^ gmacTag[1]) == _ivMac[1];
		}

	private:
		uint64_t _ivMac[2];
		AES::CTR _ctr;
		AES::GMAC _gmac;
		void *_output;
		unsigned int _decryptedLen;
	};

private:
	static const uint32_t Te0[256];
	static const uint32_t Te4[256];
	static const uint32_t Td0[256];
	static const uint8_t Td4[256];
	static const uint32_t rcon[15];

	void p_initSW(const uint8_t *key) noexcept;
	void p_encryptSW(const uint8_t *in, uint8_t *out) const noexcept;
	void p_decryptSW(const uint8_t *in, uint8_t *out) const noexcept;

	union
	{
#ifdef ZT_AES_AESNI
		struct
		{
			__m128i k[28];
			__m128i h[4]; // h, hh, hhh, hhhh
			__m128i h2[4]; // _mm_xor_si128(_mm_shuffle_epi32(h, 78), h), etc.
		} ni;
#endif

#ifdef ZT_AES_NEON
		struct
		{
			uint64_t hsw[2]; // in case it has AES but not PMULL, not sure if that ever happens
			uint8x16_t ek[15];
			uint8x16_t dk[15];
			uint8x16_t h;
		} neon;
#endif

		struct
		{
			uint64_t h[2];
			uint32_t ek[60];
			uint32_t dk[60];
		} sw;
	} p_k;

#ifdef ZT_AES_AESNI
	void p_init_aesni(const uint8_t *key) noexcept;
	void p_encrypt_aesni(const void *in, void *out) const noexcept;
	void p_decrypt_aesni(const void *in, void *out) const noexcept;
#endif

#ifdef ZT_AES_NEON
	void p_init_armneon_crypto(const uint8_t *key) noexcept;
	void p_encrypt_armneon_crypto(const void *in, void *out) const noexcept;
	void p_decrypt_armneon_crypto(const void *in, void *out) const noexcept;
#endif
};

} // namespace ZeroTier

#endif
