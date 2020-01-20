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
#include "Buffer.hpp"
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
	 * This generates a SHA384 hash of this identity's keys.
	 *
	 * @param h Buffer to receive SHA384 of public key(s)
	 * @param includePrivate If true, hash private key(s) as well
	 */
	bool hash(uint8_t h[48],bool includePrivate = false) const;

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
	 * Serialize this identity (binary)
	 *
	 * @param b Destination buffer to append to
	 * @param includePrivate If true, include private key component (if present) (default: false)
	 */
	template<unsigned int C>
	inline void serialize(Buffer<C> &b,bool includePrivate = false) const
	{
		_address.appendTo(b);
		switch(_type) {

			case C25519:
				b.append((uint8_t)C25519);
				b.append(_pub.c25519,ZT_C25519_PUBLIC_KEY_LEN);
				if ((_hasPrivate)&&(includePrivate)) {
					b.append((uint8_t)ZT_C25519_PRIVATE_KEY_LEN);
					b.append(_priv.c25519,ZT_C25519_PRIVATE_KEY_LEN);
				} else {
					b.append((uint8_t)0);
				}
				break;

			case P384:
				b.append((uint8_t)P384);
				b.append(&_pub,ZT_C25519_PUBLIC_KEY_LEN + ZT_ECC384_PUBLIC_KEY_SIZE + ZT_C25519_SIGNATURE_LEN + ZT_ECC384_SIGNATURE_SIZE);
				if ((_hasPrivate)&&(includePrivate)) {
					b.append((uint8_t)(ZT_C25519_PRIVATE_KEY_LEN + ZT_ECC384_PRIVATE_KEY_SIZE));
					b.append(_priv.c25519,ZT_C25519_PRIVATE_KEY_LEN);
					b.append(_priv.p384,ZT_ECC384_PRIVATE_KEY_SIZE);
				} else {
					b.append((uint8_t)0);
				}
				b.append((uint8_t)0); // size of additional fields (should have included such a thing in v0!)
				break;

		}
	}

	/**
	 * Deserialize a binary serialized identity
	 *
	 * If an exception is thrown, the Identity object is left in an undefined
	 * state and should not be used.
	 *
	 * @param b Buffer containing serialized data
	 * @param startAt Index within buffer of serialized data (default: 0)
	 * @return Length of serialized data read from buffer
	 */
	template<unsigned int C>
	inline unsigned int deserialize(const Buffer<C> &b,unsigned int startAt = 0)
	{
		_hasPrivate = false;
		unsigned int p = startAt;
		unsigned int pkl;

		_address.setTo(b.field(p,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH);
		p += ZT_ADDRESS_LENGTH;

		switch((_type = (Type)b[p++])) {

			case C25519:
				memcpy(_pub.c25519,b.field(p,ZT_C25519_PUBLIC_KEY_LEN),ZT_C25519_PUBLIC_KEY_LEN);
				p += ZT_C25519_PUBLIC_KEY_LEN;
				pkl = (unsigned int)b[p++];
				if (pkl) {
					if (pkl != ZT_C25519_PRIVATE_KEY_LEN)
						throw ZT_EXCEPTION_INVALID_SERIALIZED_DATA_INVALID_CRYPTOGRAPHIC_TOKEN;
					_hasPrivate = true;
					memcpy(_priv.c25519,b.field(p,ZT_C25519_PRIVATE_KEY_LEN),ZT_C25519_PRIVATE_KEY_LEN);
					p += ZT_C25519_PRIVATE_KEY_LEN;
				} else {
					_hasPrivate = false;
				}
				break;

			case P384:
				memcpy(&_pub,b.field(p,ZT_C25519_PUBLIC_KEY_LEN + ZT_ECC384_PUBLIC_KEY_SIZE + ZT_C25519_SIGNATURE_LEN + ZT_ECC384_SIGNATURE_SIZE),ZT_C25519_PUBLIC_KEY_LEN + ZT_ECC384_PUBLIC_KEY_SIZE + ZT_C25519_SIGNATURE_LEN + ZT_ECC384_SIGNATURE_SIZE);
				p += ZT_C25519_PUBLIC_KEY_LEN + ZT_ECC384_PUBLIC_KEY_SIZE + ZT_C25519_SIGNATURE_LEN + ZT_ECC384_SIGNATURE_SIZE;
				pkl = (unsigned int)b[p++];
				if (pkl) {
					if (pkl != (ZT_C25519_PRIVATE_KEY_LEN + ZT_ECC384_PRIVATE_KEY_SIZE))
						throw ZT_EXCEPTION_INVALID_SERIALIZED_DATA_INVALID_CRYPTOGRAPHIC_TOKEN;
					_hasPrivate = true;
					memcpy(_priv.c25519,b.field(p,ZT_C25519_PRIVATE_KEY_LEN),ZT_C25519_PRIVATE_KEY_LEN);
					p += ZT_C25519_PRIVATE_KEY_LEN;
					memcpy(_priv.p384,b.field(p,ZT_ECC384_PRIVATE_KEY_SIZE),ZT_ECC384_PRIVATE_KEY_SIZE);
					p += ZT_ECC384_PRIVATE_KEY_SIZE;
				} else {
					_hasPrivate = false;
				}
				p += b.template at<uint8_t>(p) + 2;
				break;

			default:
				throw ZT_EXCEPTION_INVALID_SERIALIZED_DATA_INVALID_TYPE;

		}

		return (p - startAt);
	}

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

	// Marshal interface ///////////////////////////////////////////////////////
	static ZT_ALWAYS_INLINE int marshalSizeMax() { return ZT_IDENTITY_MARSHAL_SIZE_MAX; }
	inline int marshal(uint8_t data[ZT_IDENTITY_MARSHAL_SIZE_MAX],const bool includePrivate = false) const
	{
		_address.copyTo(data,ZT_ADDRESS_LENGTH);
		switch(_type) {

			case C25519:
				data[ZT_ADDRESS_LENGTH] = (uint8_t)C25519;
				memcpy(data + ZT_ADDRESS_LENGTH + 1,_pub.c25519,ZT_C25519_PUBLIC_KEY_LEN);
				if ((includePrivate)&&(_hasPrivate)) {
					data[ZT_ADDRESS_LENGTH + 1 + ZT_C25519_PUBLIC_KEY_LEN] = ZT_C25519_PRIVATE_KEY_LEN;
					memcpy(data + ZT_ADDRESS_LENGTH + 1 + ZT_C25519_PUBLIC_KEY_LEN + 1,_priv.c25519,ZT_C25519_PRIVATE_KEY_LEN);
					return (ZT_ADDRESS_LENGTH + 1 + ZT_C25519_PUBLIC_KEY_LEN + 1 + ZT_C25519_PRIVATE_KEY_LEN);
				}
				data[ZT_ADDRESS_LENGTH + 1 + ZT_C25519_PUBLIC_KEY_LEN] = 0;
				return (ZT_ADDRESS_LENGTH + 1 + ZT_C25519_PUBLIC_KEY_LEN + 1);

			case P384:
				data[ZT_ADDRESS_LENGTH] = (uint8_t)P384;
				memcpy(data + ZT_ADDRESS_LENGTH + 1,&_pub,ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE);
				if ((includePrivate)&&(_hasPrivate)) {
					data[ZT_ADDRESS_LENGTH + 1 + ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE] = ZT_C25519_PRIVATE_KEY_LEN + ZT_ECC384_PRIVATE_KEY_SIZE;
					memcpy(data + ZT_ADDRESS_LENGTH + 1 + ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE + 1,&_priv,ZT_IDENTITY_P384_COMPOUND_PRIVATE_KEY_SIZE);
					data[ZT_ADDRESS_LENGTH + 1 + ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE + 1 + ZT_IDENTITY_P384_COMPOUND_PRIVATE_KEY_SIZE] = 0;
					return (ZT_ADDRESS_LENGTH + 1 + ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE + 1 + ZT_IDENTITY_P384_COMPOUND_PRIVATE_KEY_SIZE + 1);
				}
				data[ZT_ADDRESS_LENGTH + 1 + ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE] = 0;
				data[ZT_ADDRESS_LENGTH + 1 + ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE + 1] = 0;
				return (ZT_ADDRESS_LENGTH + 1 + ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE + 2);

		}
		return -1;
	}
	inline int unmarshal(const uint8_t *restrict data,const int len)
	{
		if (len < (ZT_ADDRESS_LENGTH + 1))
			return -1;
		unsigned int privlen;
		switch((_type = (Type)data[ZT_ADDRESS_LENGTH])) {

			case C25519:
				if (len < (ZT_ADDRESS_LENGTH + 1 + ZT_C25519_PUBLIC_KEY_LEN + 1))
					return -1;
				memcpy(_pub.c25519,data + ZT_ADDRESS_LENGTH + 1,ZT_C25519_PUBLIC_KEY_LEN);
				privlen = data[ZT_ADDRESS_LENGTH + 1 + ZT_C25519_PUBLIC_KEY_LEN];
				if (privlen == ZT_C25519_PRIVATE_KEY_LEN) {
					if (len < (ZT_ADDRESS_LENGTH + 1 + ZT_C25519_PUBLIC_KEY_LEN + 1 + ZT_C25519_PRIVATE_KEY_LEN))
						return -1;
					_hasPrivate = true;
					memcpy(_priv.c25519,data + ZT_ADDRESS_LENGTH + 1 + ZT_C25519_PUBLIC_KEY_LEN + 1,ZT_C25519_PRIVATE_KEY_LEN);
					return (ZT_ADDRESS_LENGTH + 1 + ZT_C25519_PUBLIC_KEY_LEN + 1 + ZT_C25519_PRIVATE_KEY_LEN);
				} else if (privlen == 0) {
					_hasPrivate = false;
					return (ZT_ADDRESS_LENGTH + 1 + ZT_C25519_PUBLIC_KEY_LEN + 1);
				}
				break;

			case P384:
				if (len < (ZT_ADDRESS_LENGTH + 1 + ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE + 2))
					return -1;
				memcpy(&_pub,data + ZT_ADDRESS_LENGTH + 1,ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE);
				privlen = data[ZT_ADDRESS_LENGTH + 1 + ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE];
				if (privlen == ZT_IDENTITY_P384_COMPOUND_PRIVATE_KEY_SIZE) {
					if (len < (ZT_ADDRESS_LENGTH + 1 + ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE + 1 + ZT_IDENTITY_P384_COMPOUND_PRIVATE_KEY_SIZE + 1))
						return -1;
					_hasPrivate = true;
					memcpy(&_priv,data + ZT_ADDRESS_LENGTH + 1 + ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE + 1,ZT_IDENTITY_P384_COMPOUND_PRIVATE_KEY_SIZE);
					privlen = data[ZT_ADDRESS_LENGTH + 1 + ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE + 1 + ZT_IDENTITY_P384_COMPOUND_PRIVATE_KEY_SIZE];
					if (len < (privlen + (ZT_ADDRESS_LENGTH + 1 + ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE + 1 + ZT_IDENTITY_P384_COMPOUND_PRIVATE_KEY_SIZE + 1)))
						return -1;
					return (int)(privlen + (unsigned int)(ZT_ADDRESS_LENGTH + 1 + ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE + 1 + ZT_IDENTITY_P384_COMPOUND_PRIVATE_KEY_SIZE + 1));
				} else if (privlen == 0) {
					_hasPrivate = false;
					return (ZT_ADDRESS_LENGTH + 1 + ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE + 2);
				}
				break;

		}
		return -1;
	}
	////////////////////////////////////////////////////////////////////////////

private:
	Address _address;
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
