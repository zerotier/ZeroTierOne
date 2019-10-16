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

	ZT_ALWAYS_INLINE Identity() { memset(reinterpret_cast<void *>(this),0,sizeof(Identity)); }
	ZT_ALWAYS_INLINE Identity(const char *str)
	{
		if (!fromString(str))
			throw ZT_EXCEPTION_INVALID_SERIALIZED_DATA_INVALID_TYPE;
	}
	template<unsigned int C>
	ZT_ALWAYS_INLINE Identity(const Buffer<C> &b,unsigned int startAt = 0) { deserialize(b,startAt); }

	ZT_ALWAYS_INLINE ~Identity() { Utils::burn(reinterpret_cast<void *>(this),sizeof(Identity)); }

	/**
	 * Set identity to NIL value (all zero)
	 */
	ZT_ALWAYS_INLINE void zero() { memset(reinterpret_cast<void *>(this),0,sizeof(Identity)); }

	/**
	 * @return Identity type
	 */
	ZT_ALWAYS_INLINE Type type() const { return _type; }

	/**
	 * Generate a new identity (address, key pair)
	 *
	 * This is a time consuming operation.
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
	ZT_ALWAYS_INLINE bool hasPrivate() const { return _hasPrivate; }

	/**
	 * Compute the SHA512 hash of our private key (if we have one)
	 *
	 * @param sha Buffer to receive SHA512 (MUST be ZT_SHA512_DIGEST_LEN (64) bytes in length)
	 * @return True on success, false if no private key
	 */
	ZT_ALWAYS_INLINE bool sha512PrivateKey(void *const sha) const
	{
		if (_hasPrivate) {
			switch(_type) {
				case C25519:
					SHA512(sha,_priv.c25519,ZT_C25519_PRIVATE_KEY_LEN);
					return true;
				case P384:
					SHA512(sha,&_priv,sizeof(_priv));
					return true;
			}
		}
		return false;
	}

	/**
	 * @param h Buffer to receive SHA384 of public key(s)
	 */
	ZT_ALWAYS_INLINE bool hash(uint8_t h[48]) const
	{
		switch(_type) {
			case C25519:
				SHA384(h,_pub.c25519,ZT_C25519_PUBLIC_KEY_LEN);
				return true;
			case P384:
				SHA384(h,&_pub,sizeof(_pub));
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
	ZT_ALWAYS_INLINE unsigned int sign(const void *data,unsigned int len,void *sig,unsigned int siglen) const
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
						// Signature hash includes the C25519/Ed25519 public key after the message.
						// This is an added guard against divorcing these two bound keys.
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
	ZT_ALWAYS_INLINE bool verify(const void *data,unsigned int len,const void *sig,unsigned int siglen) const
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
	ZT_ALWAYS_INLINE bool agree(const Identity &id,uint8_t key[ZT_PEER_SECRET_KEY_LENGTH]) const
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
					// Perform key agreement over both curves for the same reason that C25519 public
					// keys are included in P-384 signature inputs: to bind the keys together so
					// that a type 1 identity with the same C25519 public key (and therefore address)
					// but a different P-384 key will not work.
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
	ZT_ALWAYS_INLINE const Address &address() const { return _address; }

	/**
	 * Attempt to generate an older type identity from a newer type
	 *
	 * If this identity has its private key this is not transferred to
	 * the downgraded identity.
	 *
	 * @param dest Destination to fill with downgraded identity
	 * @param toType Desired identity type
	 */
	ZT_ALWAYS_INLINE bool downgrade(Identity &dest,const Type toType)
	{
		if ((_type == P384)&&(toType == C25519)) {
			dest._address = _address;
			dest._type = C25519;
			dest._hasPrivate = false;
			memcpy(dest._pub.c25519,_pub.c25519,ZT_C25519_PUBLIC_KEY_LEN);
			return true;
		}
		return false;
	}

	/**
	 * Serialize this identity (binary)
	 *
	 * @param b Destination buffer to append to
	 * @param includePrivate If true, include private key component (if present) (default: false)
	 * @throws std::out_of_range Buffer too small
	 */
	template<unsigned int C>
	ZT_ALWAYS_INLINE void serialize(Buffer<C> &b,bool includePrivate = false) const
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
				b.append(_pub.c25519,ZT_C25519_PUBLIC_KEY_LEN);
				b.append(_pub.p384,ZT_ECC384_PUBLIC_KEY_SIZE);
				b.append(_pub.p384s,ZT_C25519_SIGNATURE_LEN);
				if ((_hasPrivate)&&(includePrivate)) {
					b.append((uint8_t)(ZT_C25519_PRIVATE_KEY_LEN + ZT_ECC384_PRIVATE_KEY_SIZE));
					b.append(_priv.c25519,ZT_C25519_PRIVATE_KEY_LEN);
					b.append(_priv.p384,ZT_ECC384_PRIVATE_KEY_SIZE);
				} else {
					b.append((uint8_t)0);
				}
				b.append((uint16_t)0); // size of additional fields
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
	 * @throws std::out_of_range Serialized data invalid
	 * @throws std::invalid_argument Serialized data invalid
	 */
	template<unsigned int C>
	ZT_ALWAYS_INLINE unsigned int deserialize(const Buffer<C> &b,unsigned int startAt = 0)
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
				memcpy(_pub.c25519,b.field(p,ZT_C25519_PUBLIC_KEY_LEN),ZT_C25519_PUBLIC_KEY_LEN);
				p += ZT_C25519_PUBLIC_KEY_LEN;
				memcpy(_pub.p384,b.field(p,ZT_ECC384_PUBLIC_KEY_SIZE),ZT_ECC384_PUBLIC_KEY_SIZE);
				p += ZT_ECC384_PUBLIC_KEY_SIZE;
				memcpy(_pub.p384s,b.field(p,ZT_C25519_SIGNATURE_LEN),ZT_C25519_SIGNATURE_LEN);
				p += ZT_ECC384_SIGNATURE_SIZE;
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
	 * @return ASCII string representation of identity
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
	ZT_ALWAYS_INLINE operator bool() const { return (_address); }

	ZT_ALWAYS_INLINE bool operator==(const Identity &id) const
	{
		if ((_address == id._address)&&(_type == id._type)) {
			switch(_type) {
				case C25519:
					return (memcmp(_pub.c25519,id._pub.c25519,ZT_C25519_PUBLIC_KEY_LEN) == 0);
				case P384:
					return (memcmp(&_pub,&id._pub,sizeof(_pub)) == 0);
				default:
					return false;
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
					case C25519:
						return (memcmp(_pub.c25519,id._pub.c25519,ZT_C25519_PUBLIC_KEY_LEN) < 0);
					case P384:
						return (memcmp(&_pub,&id._pub,sizeof(_pub)) < 0);
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

private:
	Address _address;
	Type _type;
	bool _hasPrivate;
	ZT_PACKED_STRUCT(struct { // don't re-order these
		uint8_t c25519[ZT_C25519_PRIVATE_KEY_LEN];
		uint8_t p384[ZT_ECC384_PRIVATE_KEY_SIZE];
	}) _priv;
	ZT_PACKED_STRUCT(struct { // don't re-order these
		uint8_t c25519[ZT_C25519_PUBLIC_KEY_LEN];
		uint8_t p384[ZT_ECC384_PUBLIC_KEY_SIZE];
		uint8_t p384s[ZT_C25519_SIGNATURE_LEN]; // signature of both keys with ed25519 to confirm type 0 extension to type 1
	}) _pub;
};

} // namespace ZeroTier

#endif
