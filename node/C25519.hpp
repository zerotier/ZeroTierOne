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

#define ZT_C25519_PUBLIC_KEY_LEN 64
#define ZT_C25519_PRIVATE_KEY_LEN 64
#define ZT_C25519_SIGNATURE_LEN 96
#define ZT_C25519_SHARED_KEY_LEN 32

/**
 * A combined Curve25519 ECDH and Ed25519 signature engine
 */
class C25519
{
public:
	/**
	 * Generate a C25519 elliptic curve key pair
	 */
	static void generate(uint8_t pub[ZT_C25519_PUBLIC_KEY_LEN],uint8_t priv[ZT_C25519_PRIVATE_KEY_LEN]);

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
	static ZT_ALWAYS_INLINE void generateSatisfying(F cond,uint8_t pub[ZT_C25519_PUBLIC_KEY_LEN],uint8_t priv[ZT_C25519_PRIVATE_KEY_LEN])
	{
		Utils::getSecureRandom(priv,ZT_C25519_PRIVATE_KEY_LEN);
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
	static void agree(const uint8_t mine[ZT_C25519_PRIVATE_KEY_LEN],const uint8_t their[ZT_C25519_PUBLIC_KEY_LEN],uint8_t rawkey[ZT_C25519_SHARED_KEY_LEN]);

	/**
	 * Sign a message with a sender's key pair
	 *
	 * This takes the SHA-521 of msg[] and then signs the first 32 bytes of this
	 * digest, returning it and the 64-byte ed25519 signature in signature[].
	 * This results in a signature that verifies both the signer's authenticity
	 * and the integrity of the message.
	 *
	 * This is based on the original ed25519 code from NaCl and the SUPERCOP
	 * cipher benchmark suite, but with the modification that it always
	 * produces a signature of fixed 96-byte length based on the hash of an
	 * arbitrary-length message.
	 *
	 * @param myPrivate My private key
	 * @param myPublic My public key
	 * @param msg Message to sign
	 * @param len Length of message in bytes
	 * @param signature Buffer to fill with signature -- MUST be 96 bytes in length
	 */
	static void sign(const uint8_t myPrivate[ZT_C25519_PRIVATE_KEY_LEN],const uint8_t myPublic[ZT_C25519_PUBLIC_KEY_LEN],const void *msg,unsigned int len,void *signature);

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
	static bool verify(const uint8_t their[ZT_C25519_PUBLIC_KEY_LEN],const void *msg,unsigned int len,const void *signature,const unsigned int siglen);

private:
	// derive first 32 bytes of kp.pub from first 32 bytes of kp.priv
	// this is the ECDH key
	static void _calcPubDH(uint8_t pub[ZT_C25519_PUBLIC_KEY_LEN],const uint8_t priv[ZT_C25519_PRIVATE_KEY_LEN]);

	// derive 2nd 32 bytes of kp.pub from 2nd 32 bytes of kp.priv
	// this is the Ed25519 sign/verify key
	static void _calcPubED(uint8_t pub[ZT_C25519_PUBLIC_KEY_LEN],const uint8_t priv[ZT_C25519_PRIVATE_KEY_LEN]);
};

} // namespace ZeroTier

#endif
