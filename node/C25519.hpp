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

// Note that the actual code in C25519.cpp is in the public domain as per
// its original license.

#ifndef ZT_C25519_HPP
#define ZT_C25519_HPP

#include "Constants.hpp"
#include "Utils.hpp"

namespace ZeroTier {

#define ZT_C25519_ECDH_PUBLIC_KEY_SIZE 32
#define ZT_C25519_ECDH_PRIVATE_KEY_SIZE 32
#define ZT_C25519_COMBINED_PUBLIC_KEY_SIZE 64
#define ZT_C25519_COMBINED_PRIVATE_KEY_SIZE 64
#define ZT_C25519_SIGNATURE_LEN 96
#define ZT_C25519_ECDH_SHARED_SECRET_SIZE 32

/**
 * A combined Curve25519 ECDH and Ed25519 signature engine
 */
class C25519
{
public:
	/**
	 * Generate a set of two 25519 keys: a C25519 ECDH key pair and an Ed25519 EDDSA key pair.
	 */
	static void generateCombined(uint8_t pub[ZT_C25519_COMBINED_PUBLIC_KEY_SIZE],uint8_t priv[ZT_C25519_COMBINED_PRIVATE_KEY_SIZE]);

	/**
	 * Generate a C25519 ECDH key pair only.
	 */
	static void generateC25519(uint8_t pub[ZT_C25519_ECDH_PUBLIC_KEY_SIZE],uint8_t priv[ZT_C25519_ECDH_PRIVATE_KEY_SIZE]);

	/**
	 * Generate a key pair satisfying a condition
	 *
	 * This begins with a random keypair from a random secret key and then
	 * iteratively increments the random secret until cond(kp) returns true.
	 * This is used to compute key pairs in which the public key, its hash
	 * or some other aspect of it satisfies some condition, such as for a
	 * hashcash criteria.
	 *
	 * @param cond Condition function or function object
	 * @return Key pair where cond(kp) returns true
	 * @tparam F Type of 'cond'
	 */
	template<typename F>
	static ZT_INLINE void generateSatisfying(F cond,uint8_t pub[ZT_C25519_COMBINED_PUBLIC_KEY_SIZE],uint8_t priv[ZT_C25519_COMBINED_PRIVATE_KEY_SIZE])
	{
		Utils::getSecureRandom(priv,ZT_C25519_COMBINED_PRIVATE_KEY_SIZE);
		_calcPubED(pub,priv); // do Ed25519 key -- bytes 32-63 of pub and priv
		do {
			++(((uint64_t *)priv)[1]);
			--(((uint64_t *)priv)[2]);
			_calcPubDH(pub,priv); // keep regenerating bytes 0-31 until satisfied
		} while (!cond(pub));
	}

	/**
	 * Perform C25519 ECC key agreement
	 *
	 * Actual key bytes are generated from one or more SHA-512 digests of
	 * the raw result of key agreement.
	 *
	 * @param mine My private key
	 * @param their Their public key
	 * @param rawkey Buffer to receive raw (not hashed) agreed upon key
	 */
	static void agree(const uint8_t mine[ZT_C25519_COMBINED_PRIVATE_KEY_SIZE],const uint8_t their[ZT_C25519_COMBINED_PUBLIC_KEY_SIZE],uint8_t rawkey[ZT_C25519_ECDH_SHARED_SECRET_SIZE]);

	/**
	 * Sign a message with a sender's key pair
	 *
	 * LEGACY: ZeroTier's ed25519 signatures contain an extra 32 bytes which are the first
	 * 32 bytes of SHA512(msg). These exist because an early version of the ZeroTier multicast
	 * algorithm did a lot of signature verification and we wanted a way to skip the more
	 * expensive ed25519 verification if the signature was obviously wrong.
	 *
	 * This verify() function will accept a 64 or 96 bit signature, checking the last 32
	 * bytes only if present.
	 *
	 * @param myPrivate My private key
	 * @param myPublic My public key
	 * @param msg Message to sign
	 * @param len Length of message in bytes
	 * @param signature Buffer to fill with signature -- MUST be 96 bytes in length
	 */
	static void sign(const uint8_t myPrivate[ZT_C25519_COMBINED_PRIVATE_KEY_SIZE],const uint8_t myPublic[ZT_C25519_COMBINED_PUBLIC_KEY_SIZE],const void *msg,unsigned int len,void *signature);

	/**
	 * Verify a message's signature
	 *
	 * @param their Public key to verify against
	 * @param msg Message to verify signature integrity against
	 * @param len Length of message in bytes
	 * @param signature Signature bytes
	 * @param siglen Length of signature in bytes
	 * @return True if signature is valid and the message is authentic and unmodified
	 */
	static bool verify(const uint8_t their[ZT_C25519_COMBINED_PUBLIC_KEY_SIZE],const void *msg,unsigned int len,const void *signature,unsigned int siglen);

private:
	// derive first 32 bytes of kp.pub from first 32 bytes of kp.priv
	// this is the ECDH key
	static void _calcPubDH(uint8_t *pub,const uint8_t *priv);

	// derive 2nd 32 bytes of kp.pub from 2nd 32 bytes of kp.priv
	// this is the Ed25519 sign/verify key
	static void _calcPubED(uint8_t pub[ZT_C25519_COMBINED_PUBLIC_KEY_SIZE],const uint8_t priv[ZT_C25519_COMBINED_PRIVATE_KEY_SIZE]);
};

} // namespace ZeroTier

#endif
