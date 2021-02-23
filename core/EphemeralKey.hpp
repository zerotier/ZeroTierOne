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

#ifndef ZT_EPHEMERALKEY_HPP
#define ZT_EPHEMERALKEY_HPP

#include "Constants.hpp"
#include "C25519.hpp"
#include "ECC384.hpp"
#include "SHA512.hpp"
#include "Utils.hpp"

#define ZT_EPHEMERALKEY_PUBLIC_SIZE (1 + ZT_C25519_ECDH_PUBLIC_KEY_SIZE + ZT_ECC384_PUBLIC_KEY_SIZE)

namespace ZeroTier {

/**
 * Container for ephemeral key pair sets used in forward secrecy negotiation.
 * 
 * The ephemeral public key consists of public key(s) prefixed by a type byte.
 * In the current version there are two keys: a Curve25519 ECDH public key and
 * a NIST P-384 public key. Both are sent, and key agreement is performed by
 * agreeing with both and then hashing the results together with the long-lived
 * identity shared secret to produce a shared symmetric ephemeral key.
 * 
 * Unlike identities the private key never leaves this class. It dies when
 * a new key pair is generated or when the node is shut down.
 * 
 * Each peer holds a copy of its current ephemeral key. This is re-generated
 * after one half ZT_SYMMETRIC_KEY_TTL or after the the symmetric key has
 * been used one half of ZT_SYMMETRIC_KEY_TTL_MESSAGES times. Half the total
 * TTL is chosen to provide plenty of margin.
 */
class EphemeralKey
{
public:
	enum Type
	{
		TYPE_NIL = 0,
		TYPE_C25519_P384 = 1
	};

	/**
	 * The ephemeral public key(s)
	 * 
	 * This is sent with HELLO or OK(HELLO) and is re-written when
	 * generate() is called. Its size is static.
	 */
	const uint8_t pub[ZT_EPHEMERALKEY_PUBLIC_SIZE];

	/**
	 * Create an uninitialized ephemeral key (must call generate())
	 */
	ZT_INLINE EphemeralKey() noexcept:
		pub()
	{
		const_cast<uint8_t *>(pub)[0] = (uint8_t) TYPE_NIL;
		Utils::memoryLock(this, sizeof(EphemeralKey));
	}

	ZT_INLINE ~EphemeralKey() noexcept
	{
		Utils::burn(m_priv, sizeof(m_priv));
		Utils::memoryUnlock(this, sizeof(EphemeralKey));
	}

	/**
	 * @return True if this ephemeral key has been initialized with generate()
	 */
	ZT_INLINE operator bool() const noexcept
	{ return pub[0] != (uint8_t) TYPE_NIL; }

	/**
	 * Generate or re-generate key pair.
	 */
	ZT_INLINE void generate() noexcept
	{
		uint8_t *const p = const_cast<uint8_t *>(pub);
		p[0] = (uint8_t) TYPE_C25519_P384;
		C25519::generateC25519(p + 1, m_priv);
		ECC384GenerateKey(p + 1 + ZT_C25519_ECDH_PUBLIC_KEY_SIZE, m_priv + ZT_C25519_ECDH_PRIVATE_KEY_SIZE);
	}

	/**
	 * Execute key agreement with another ephemeral public key set.
	 * 
	 * Final key is produced by hashing the two ECDH keys followed by
	 * the identity secret key with SHA384.
	 * 
	 * @param identityKey Raw identity key shared between this node and peer
	 * @param otherPub Other public key (prefixed by type)
	 * @param key Key buffer to fill with symmetric key
	 * @return True on success
	 */
	ZT_INLINE bool agree(const uint8_t identityKey[ZT_SYMMETRIC_KEY_SIZE], const uint8_t *otherPub, const unsigned int otherPubLength, uint8_t key[ZT_SYMMETRIC_KEY_SIZE]) const noexcept
	{
		if ((otherPubLength < ZT_EPHEMERALKEY_PUBLIC_SIZE) || (otherPub[0] != (uint8_t) TYPE_C25519_P384))
			return false;
		uint8_t tmp[ZT_C25519_ECDH_SHARED_SECRET_SIZE + ZT_ECC384_SHARED_SECRET_SIZE];
		C25519::agree(m_priv, otherPub + 1, tmp);
		if (!ECC384ECDH(otherPub + 1 + ZT_C25519_ECDH_PUBLIC_KEY_SIZE, m_priv + ZT_C25519_ECDH_PRIVATE_KEY_SIZE, tmp + ZT_C25519_ECDH_SHARED_SECRET_SIZE))
			return false;
		SHA384(key, tmp, ZT_C25519_ECDH_SHARED_SECRET_SIZE + ZT_ECC384_SHARED_SECRET_SIZE, identityKey, ZT_SYMMETRIC_KEY_SIZE);
		Utils::burn(tmp, ZT_C25519_ECDH_SHARED_SECRET_SIZE + ZT_ECC384_SHARED_SECRET_SIZE);
		return true;
	}

	/**
	 * Check and see if an acknowledgement hash returned via OK(HELLO) matches our public key.
	 * 
	 * @param ackHash Hash provided in OK(HELLO)
	 * @return True if this matches the hash of this ephemeral key
	 */
	ZT_INLINE bool acknowledged(const uint8_t ackHash[ZT_SHA384_DIGEST_SIZE]) const noexcept
	{
		uint8_t h[ZT_SHA384_DIGEST_SIZE];
		SHA384(h, pub, ZT_EPHEMERALKEY_PUBLIC_SIZE);
		return Utils::secureEq(ackHash, h, ZT_SHA384_DIGEST_SIZE);
	}

private:
	uint8_t m_priv[ZT_C25519_ECDH_PRIVATE_KEY_SIZE + ZT_ECC384_PRIVATE_KEY_SIZE];
};

} // namespace ZeroTier

#endif
