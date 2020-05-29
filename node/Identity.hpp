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

#ifndef ZT_IDENTITY_HPP
#define ZT_IDENTITY_HPP

#include "Constants.hpp"
#include "Utils.hpp"
#include "Address.hpp"
#include "C25519.hpp"
#include "SHA512.hpp"
#include "ECC384.hpp"
#include "TriviallyCopyable.hpp"
#include "Fingerprint.hpp"
#include "Containers.hpp"

#define ZT_IDENTITY_STRING_BUFFER_LENGTH           1024
#define ZT_IDENTITY_C25519_PUBLIC_KEY_SIZE         ZT_C25519_COMBINED_PUBLIC_KEY_SIZE
#define ZT_IDENTITY_C25519_PRIVATE_KEY_SIZE        ZT_C25519_COMBINED_PRIVATE_KEY_SIZE
#define ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE  (1 + ZT_C25519_COMBINED_PUBLIC_KEY_SIZE + ZT_ECC384_PUBLIC_KEY_SIZE)
#define ZT_IDENTITY_P384_COMPOUND_PRIVATE_KEY_SIZE (ZT_C25519_COMBINED_PRIVATE_KEY_SIZE + ZT_ECC384_PRIVATE_KEY_SIZE)
#define ZT_IDENTITY_MARSHAL_SIZE_MAX               (ZT_ADDRESS_LENGTH + 4 + ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE + ZT_IDENTITY_P384_COMPOUND_PRIVATE_KEY_SIZE)

namespace ZeroTier {

/**
 * A ZeroTier identity
 *
 * Identities currently come in two types: type 0 identities based on just Curve25519
 * and Ed25519 and type 1 identities that include both a 25519 key pair and a NIST P-384
 * key pair. Type 1 identities use P-384 for signatures but use both key pairs at once
 * (hashing both keys together) for key agreement with other type 1 identities, and can
 * agree with type 0 identities using only Curve25519.
 *
 * Type 1 identities are better in many ways but type 0 will remain the default until
 * 1.x nodes are pretty much dead in the wild.
 */
class Identity : public TriviallyCopyable
{
public:
	/**
	 * Identity type -- numeric values of these enums are protocol constants
	 */
	enum Type
	{
		C25519 = ZT_IDENTITY_TYPE_C25519, // Type 0 -- Curve25519 and Ed25519 (1.x and 2.x, default)
		P384 = ZT_IDENTITY_TYPE_P384      // Type 1 -- NIST P-384 with linked Curve25519/Ed25519 secondaries (2.x+)
	};

	/**
	 * A nil/empty identity instance
	 */
	static const Identity NIL;

	ZT_INLINE Identity() noexcept
	{
		Utils::memoryLock(this,sizeof(Identity));
		memoryZero(this);
	}

	/**
	 * Construct identity from string
	 *
	 * If the identity is not basically valid (no deep checking is done) the result will
	 * be a null identity.
	 *
	 * @param str Identity in canonical string format
	 */
	explicit ZT_INLINE Identity(const char *str)
	{
		Utils::memoryLock(this,sizeof(Identity));
		fromString(str);
	}

	ZT_INLINE ~Identity()
	{
		Utils::memoryUnlock(this,sizeof(Identity));
		Utils::burn(reinterpret_cast<void *>(&this->m_priv), sizeof(this->m_priv));
	}

	/**
	 * Set identity to NIL value (all zero)
	 */
	ZT_INLINE void zero() noexcept { memoryZero(this); }

	/**
	 * @return Identity type (undefined if identity is null or invalid)
	 */
	ZT_INLINE Type type() const noexcept { return m_type; }

	/**
	 * Generate a new identity (address, key pair)
	 *
	 * This is a time consuming operation taking up to 5-10 seconds on some slower systems.
	 *
	 * @param t Type of identity to generate
	 * @return False if there was an error such as type being an invalid value
	 */
	bool generate(Type t);

	/**
	 * Check the validity of this identity's address
	 *
	 * For type 0 identities this is slightly time consuming. For type 1 identities it's
	 * instantaneous. It should be done when a new identity is accepted for the very first
	 * time.
	 *
	 * @return True if validation check passes
	 */
	bool locallyValidate() const noexcept;

	/**
	 * @return True if this identity contains a private key
	 */
	ZT_INLINE bool hasPrivate() const noexcept { return m_hasPrivate; }

	/**
	 * @return This identity's address
	 */
	ZT_INLINE Address address() const noexcept { return Address(m_fp.address); }

	/**
	 * @return Full fingerprint of this identity (address plus SHA384 of keys)
	 */
	ZT_INLINE const Fingerprint &fingerprint() const noexcept { return m_fp; }

	/**
	 * Compute a hash of this identity's public and private keys.
	 *
	 * If there is no private key or the identity is NIL the buffer is filled with zero.
	 *
	 * @param h Buffer to store SHA384 hash
	 */
	void hashWithPrivate(uint8_t h[ZT_FINGERPRINT_HASH_SIZE]) const;

	/**
	 * Sign a message with this identity (private key required)
	 *
	 * The signature buffer should be large enough for the largest
	 * signature, which is currently 96 bytes.
	 *
	 * @param data Data to sign
	 * @param len Length of data
	 * @param sig Buffer to receive signature
	 * @param siglen Length of buffer
	 * @return Number of bytes actually written to sig or 0 on error
	 */
	unsigned int sign(const void *data,unsigned int len,void *sig,unsigned int siglen) const;

	/**
	 * Verify a message signature against this identity
	 *
	 * @param data Data to check
	 * @param len Length of data
	 * @param signature Signature bytes
	 * @param siglen Length of signature in bytes
	 * @return True if signature validates and data integrity checks
	 */
	bool verify(const void *data,unsigned int len,const void *sig,unsigned int siglen) const;

	/**
	 * Shortcut method to perform key agreement with another identity
	 *
	 * This identity must have a private key. (Check hasPrivate())
	 *
	 * @param id Identity to agree with
	 * @param key Result parameter to fill with key bytes
	 * @return Was agreement successful?
	 */
	bool agree(const Identity &id,uint8_t key[ZT_SYMMETRIC_KEY_SIZE]) const;

	/**
	 * Serialize to a more human-friendly string
	 *
	 * @param includePrivate If true, include private key (if it exists)
	 * @param buf Buffer to store string
	 * @return ASCII string representation of identity (pointer to buf)
	 */
	char *toString(bool includePrivate,char buf[ZT_IDENTITY_STRING_BUFFER_LENGTH]) const;
	ZT_INLINE String toString(const bool includePrivate = false) const { char buf[ZT_IDENTITY_STRING_BUFFER_LENGTH]; toString(includePrivate,buf); return String(buf); }

	/**
	 * Deserialize a human-friendly string
	 *
	 * Note: validation is for the format only. The locallyValidate() method
	 * must be used to check signature and address/key correspondence.
	 *
	 * @param str String to deserialize
	 * @return True if deserialization appears successful
	 */
	bool fromString(const char *str);

	/**
	 * @return True if this identity contains something
	 */
	explicit ZT_INLINE operator bool() const noexcept { return (m_fp); }

	ZT_INLINE unsigned long hashCode() const noexcept { return m_fp.hashCode(); }

	ZT_INLINE bool operator==(const Identity &id) const noexcept { return (m_fp == id.m_fp); }
	ZT_INLINE bool operator!=(const Identity &id) const noexcept { return !(*this == id); }
	ZT_INLINE bool operator<(const Identity &id) const noexcept { return (m_fp < id.m_fp); }
	ZT_INLINE bool operator>(const Identity &id) const noexcept { return (id < *this); }
	ZT_INLINE bool operator<=(const Identity &id) const noexcept { return !(id < *this); }
	ZT_INLINE bool operator>=(const Identity &id) const noexcept { return !(*this < id); }

	static constexpr int marshalSizeMax() noexcept { return ZT_IDENTITY_MARSHAL_SIZE_MAX; }
	int marshal(uint8_t data[ZT_IDENTITY_MARSHAL_SIZE_MAX],bool includePrivate = false) const noexcept;
	int unmarshal(const uint8_t *data,int len) noexcept;

private:
	void m_computeHash();

	Fingerprint m_fp;
	uint8_t m_priv[ZT_IDENTITY_P384_COMPOUND_PRIVATE_KEY_SIZE];
	uint8_t m_pub[ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE];
	Type m_type; // _type determines which fields in _priv and _pub are used
	bool m_hasPrivate;
};

} // namespace ZeroTier

#endif
