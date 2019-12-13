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

#ifndef ZT_IDENTITY_HPP
#define ZT_IDENTITY_HPP

#include <stdio.h>
#include <stdlib.h>

#include "Constants.hpp"
#include "Utils.hpp"
#include "Address.hpp"
#include "C25519.hpp"
#include "Buffer.hpp"
#include "SHA512.hpp"
#include "ECC384.hpp"

#define ZT_IDENTITY_STRING_BUFFER_LENGTH 1024

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

	inline Identity() { memset(reinterpret_cast<void *>(this),0,sizeof(Identity)); }
	inline ~Identity() { Utils::burn(reinterpret_cast<void *>(&this->_priv),sizeof(this->_priv)); }

	/**
	 * Construct identity from string
	 *
	 * If the identity is not basically valid (no deep checking is done) the result will
	 * be a null identity.
	 *
	 * @param str Identity in canonical string format
	 */
	inline Identity(const char *str) { fromString(str); }

	template<unsigned int C>
	inline Identity(const Buffer<C> &b,unsigned int startAt = 0) { deserialize(b,startAt); }

	/**
	 * Set identity to NIL value (all zero)
	 */
	inline void zero() { memset(reinterpret_cast<void *>(this),0,sizeof(Identity)); }

	/**
	 * @return Identity type (undefined if identity is null or invalid)
	 */
	inline Type type() const { return _type; }

	/**
	 * Generate a new identity (address, key pair)
	 *
	 * This is a time consuming operation taking up to 5-10 seconds on some slower systems.
	 *
	 * @param t Type of identity to generate
	 */
	void generate(const Type t);

	/**
	 * Check the validity of this identity's pairing of key to address
	 *
	 * @return True if validation check passes
	 */
	bool locallyValidate() const;

	/**
	 * @return True if this identity contains a private key
	 */
	inline bool hasPrivate() const { return _hasPrivate; }

	/**
	 * This generates a SHA384 hash of this identity's keys.
	 *
	 * @param h Buffer to receive SHA384 of public key(s)
	 * @param includePrivate If true, hash private key(s) as well
	 */
	inline bool hash(uint8_t h[48],const bool includePrivate = false) const
	{
		switch(_type) {

			case C25519:
				if ((_hasPrivate)&&(includePrivate))
					SHA384(h,_pub.c25519,ZT_C25519_PUBLIC_KEY_LEN,_priv.c25519,ZT_C25519_PRIVATE_KEY_LEN);
				else SHA384(h,_pub.c25519,ZT_C25519_PUBLIC_KEY_LEN);
				return true;

			case P384:
				if ((_hasPrivate)&&(includePrivate))
					SHA384(h,&_pub,sizeof(_pub),&_priv,sizeof(_priv));
				else SHA384(h,&_pub,sizeof(_pub));
				return true;

		}
		return false;
	}

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
	inline unsigned int sign(const void *data,unsigned int len,void *sig,unsigned int siglen) const
	{
		if (_hasPrivate) {
			switch(_type) {

				case C25519:
					if (siglen >= ZT_C25519_SIGNATURE_LEN) {
						C25519::sign(_priv.c25519,_pub.c25519,data,len,sig);
						return ZT_C25519_SIGNATURE_LEN;
					}

				case P384:
					if (siglen >= ZT_ECC384_SIGNATURE_SIZE) {
						// When signing with P384 we also hash the C25519 public key as an
						// extra measure to ensure that only this identity can verify.
						uint8_t h[48];
						SHA384(h,data,len,_pub.c25519,ZT_C25519_PUBLIC_KEY_LEN);
						ECC384ECDSASign(_priv.p384,h,(uint8_t *)sig);
						return ZT_ECC384_SIGNATURE_SIZE;
					}

			}
		}
		return 0;
	}

	/**
	 * Verify a message signature against this identity
	 *
	 * @param data Data to check
	 * @param len Length of data
	 * @param signature Signature bytes
	 * @param siglen Length of signature in bytes
	 * @return True if signature validates and data integrity checks
	 */
	inline bool verify(const void *data,unsigned int len,const void *sig,unsigned int siglen) const
	{
		switch(_type) {

			case C25519:
				return C25519::verify(_pub.c25519,data,len,sig,siglen);

			case P384:
				if (siglen == ZT_ECC384_SIGNATURE_SIZE) {
					uint8_t h[48];
					SHA384(h,data,len,_pub.c25519,ZT_C25519_PUBLIC_KEY_LEN);
					return ECC384ECDSAVerify(_pub.p384,h,(const uint8_t *)sig);
				}
				break;

		}
		return false;
	}

	/**
	 * Shortcut method to perform key agreement with another identity
	 *
	 * This identity must have a private key. (Check hasPrivate())
	 *
	 * @param id Identity to agree with
	 * @param key Result parameter to fill with key bytes
	 * @return Was agreement successful?
	 */
	inline bool agree(const Identity &id,uint8_t key[ZT_PEER_SECRET_KEY_LENGTH]) const
	{
		uint8_t rawkey[128];
		uint8_t h[64];
		if (_hasPrivate) {
			if (_type == C25519) {

				if ((id._type == C25519)||(id._type == P384)) {
					// If we are a C25519 key we can agree with another C25519 key or with only the
					// C25519 portion of a type 1 P-384 key.
					C25519::agree(_priv.c25519,id._pub.c25519,rawkey);
					SHA512(h,rawkey,ZT_C25519_SHARED_KEY_LEN);
					memcpy(key,h,ZT_PEER_SECRET_KEY_LENGTH);
					return true;
				}

			} else if (_type == P384) {

				if (id._type == P384) {
					C25519::agree(_priv.c25519,id._pub.c25519,rawkey);
					ECC384ECDH(id._pub.p384,_priv.p384,rawkey + ZT_C25519_SHARED_KEY_LEN);
					SHA384(h,rawkey,ZT_C25519_SHARED_KEY_LEN + ZT_ECC384_SHARED_SECRET_SIZE);
					memcpy(key,h,ZT_PEER_SECRET_KEY_LENGTH);
					return true;
				} else if (id._type == C25519) {
					// If the other identity is a C25519 identity we can agree using only that type.
					C25519::agree(_priv.c25519,id._pub.c25519,rawkey);
					SHA512(h,rawkey,ZT_C25519_SHARED_KEY_LEN);
					memcpy(key,h,ZT_PEER_SECRET_KEY_LENGTH);
					return true;
				}

			}
		}
		return false;
	}

	/**
	 * @return This identity's address
	 */
	inline const Address &address() const { return _address; }

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
				b.append((uint16_t)0); // size of additional fields (should have included such a thing in v0!)
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
				p += b.template at<uint16_t>(p) + 2;
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
	inline operator bool() const { return (_address); }

	inline bool operator==(const Identity &id) const
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
	inline bool operator<(const Identity &id) const
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
	inline bool operator!=(const Identity &id) const { return !(*this == id); }
	inline bool operator>(const Identity &id) const { return (id < *this); }
	inline bool operator<=(const Identity &id) const { return !(id < *this); }
	inline bool operator>=(const Identity &id) const { return !(*this < id); }

	inline unsigned long hashCode() const { return ((unsigned long)_address.toInt() + (unsigned long)_pub.c25519[0] + (unsigned long)_pub.c25519[1] + (unsigned long)_pub.c25519[2]); }

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
