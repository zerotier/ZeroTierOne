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

#include <cstdio>
#include <cstdlib>

#include "Constants.hpp"
#include "Utils.hpp"
#include "Address.hpp"
#include "C25519.hpp"
#include "SHA512.hpp"
#include "ECC384.hpp"

#define ZT_IDENTITY_STRING_BUFFER_LENGTH 1024

#define ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE (ZT_C25519_PUBLIC_KEY_LEN + ZT_ECC384_PUBLIC_KEY_SIZE + ZT_C25519_SIGNATURE_LEN + ZT_ECC384_SIGNATURE_SIZE)
#define ZT_IDENTITY_P384_COMPOUND_PRIVATE_KEY_SIZE (ZT_C25519_PRIVATE_KEY_LEN + ZT_ECC384_PRIVATE_KEY_SIZE)

#define ZT_IDENTITY_MARSHAL_SIZE_MAX (ZT_ADDRESS_LENGTH + 4 + ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE + ZT_IDENTITY_P384_COMPOUND_PRIVATE_KEY_SIZE)

namespace ZeroTier {

/**
 * A ZeroTier identity
 *
 * An identity consists of a public key, a 40-bit ZeroTier address computed
 * from that key in a collision-resistant fashion, and a self-signature.
 *
 * The address derivation algorithm makes it computationally very expensive to
 * search for a different public key that duplicates an existing address. (See
 * code for deriveAddress() for this algorithm.)
 */
class Identity
{
public:
	/**
	 * Identity type -- numeric values of these enums are protocol constants
	 */
	enum Type
	{
		C25519 = ZT_CRYPTO_ALG_C25519, // Type 0 -- Curve25519 and Ed25519 (1.x and 2.x, default)
		P384 = ZT_CRYPTO_ALG_P384      // Type 1 -- NIST P-384 with linked Curve25519/Ed25519 secondaries (2.x+)
	};

	ZT_ALWAYS_INLINE Identity() { memset(reinterpret_cast<void *>(this),0,sizeof(Identity)); }
	ZT_ALWAYS_INLINE ~Identity() { Utils::burn(reinterpret_cast<void *>(&this->_priv),sizeof(this->_priv)); }

	/**
	 * Construct identity from string
	 *
	 * If the identity is not basically valid (no deep checking is done) the result will
	 * be a null identity.
	 *
	 * @param str Identity in canonical string format
	 */
	explicit ZT_ALWAYS_INLINE Identity(const char *str) { fromString(str); }

	/**
	 * Set identity to NIL value (all zero)
	 */
	ZT_ALWAYS_INLINE void zero() { memset(reinterpret_cast<void *>(this),0,sizeof(Identity)); }

	/**
	 * @return Identity type (undefined if identity is null or invalid)
	 */
	ZT_ALWAYS_INLINE Type type() const { return _type; }

	/**
	 * Generate a new identity (address, key pair)
	 *
	 * This is a time consuming operation taking up to 5-10 seconds on some slower systems.
	 *
	 * @param t Type of identity to generate
	 */
	void generate(Type t);

	/**
	 * Check the validity of this identity's pairing of key to address
	 *
	 * @return True if validation check passes
	 */
	bool locallyValidate() const;

	/**
	 * @return True if this identity contains a private key
	 */
	ZT_ALWAYS_INLINE bool hasPrivate() const { return _hasPrivate; }

	/**
	 * @return 384-bit/48-byte hash of this identity's public key(s)
	 */
	ZT_ALWAYS_INLINE const uint8_t *hash() const
	{
		if (_hash[0] == 0)
			const_cast<Identity *>(this)->_computeHash();
		return reinterpret_cast<const uint8_t *>(_hash);
	}

	/**
	 * Compute a hash of this identity's public and private keys
	 *
	 * @param h Buffer to store SHA384 hash
	 */
	void hashWithPrivate(uint8_t h[48]) const;

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
	bool agree(const Identity &id,uint8_t key[ZT_PEER_SECRET_KEY_LENGTH]) const;

	/**
	 * @return This identity's address
	 */
	ZT_ALWAYS_INLINE const Address &address() const { return _address; }

	/**
	 * Serialize to a more human-friendly string
	 *
	 * @param includePrivate If true, include private key (if it exists)
	 * @param buf Buffer to store string
	 * @return ASCII string representation of identity (pointer to buf)
	 */
	char *toString(bool includePrivate,char buf[ZT_IDENTITY_STRING_BUFFER_LENGTH]) const;

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
	explicit ZT_ALWAYS_INLINE operator bool() const { return (_address); }

	ZT_ALWAYS_INLINE bool operator==(const Identity &id) const
	{
		if ((_address == id._address)&&(_type == id._type)) {
			switch(_type) {
				case C25519: return (memcmp(_pub.c25519,id._pub.c25519,ZT_C25519_PUBLIC_KEY_LEN) == 0);
				// case P384:
				default: return (memcmp(&_pub,&id._pub,sizeof(_pub)) == 0);
			}
		}
		return false;
	}
	ZT_ALWAYS_INLINE bool operator<(const Identity &id) const
	{
		if (_address < id._address)
			return true;
		if (_address == id._address) {
			if ((int)_type < (int)id._type)
				return true;
			if (_type == id._type) {
				switch(_type) {
					case C25519: return (memcmp(_pub.c25519,id._pub.c25519,ZT_C25519_PUBLIC_KEY_LEN) < 0);
					// case P384:
					default: return (memcmp(&_pub,&id._pub,sizeof(_pub)) < 0);
				}
			}
		}
		return false;
	}
	ZT_ALWAYS_INLINE bool operator!=(const Identity &id) const { return !(*this == id); }
	ZT_ALWAYS_INLINE bool operator>(const Identity &id) const { return (id < *this); }
	ZT_ALWAYS_INLINE bool operator<=(const Identity &id) const { return !(id < *this); }
	ZT_ALWAYS_INLINE bool operator>=(const Identity &id) const { return !(*this < id); }

	ZT_ALWAYS_INLINE unsigned long hashCode() const { return ((unsigned long)_address.toInt() + (unsigned long)_pub.c25519[0] + (unsigned long)_pub.c25519[1] + (unsigned long)_pub.c25519[2]); }

	static ZT_ALWAYS_INLINE int marshalSizeMax() { return ZT_IDENTITY_MARSHAL_SIZE_MAX; }
	int marshal(uint8_t data[ZT_IDENTITY_MARSHAL_SIZE_MAX],bool includePrivate = false) const;
	int unmarshal(const uint8_t *data,int len);

private:
	void _computeHash(); // recompute _hash

	Address _address;
	uint64_t _hash[6]; // hash of public key memo-ized for performance, recalculated when _hash[0] == 0
	Type _type; // _type determines which fields in _priv and _pub are used
	bool _hasPrivate;
	ZT_PACKED_STRUCT(struct { // don't re-order these
		uint8_t c25519[ZT_C25519_PRIVATE_KEY_LEN];
		uint8_t p384[ZT_ECC384_PRIVATE_KEY_SIZE];
	}) _priv;
	ZT_PACKED_STRUCT(struct { // don't re-order these
		uint8_t c25519[ZT_C25519_PUBLIC_KEY_LEN]; // Curve25519 and Ed25519 public keys
		uint8_t p384[ZT_ECC384_PUBLIC_KEY_SIZE];  // NIST P-384 public key
		uint8_t c25519s[ZT_C25519_SIGNATURE_LEN]; // signature of both keys with ed25519
		uint8_t p384s[ZT_ECC384_SIGNATURE_SIZE];  // signature of both keys with p384
	}) _pub;
};

} // namespace ZeroTier

#endif
