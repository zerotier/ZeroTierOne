/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2019  ZeroTier, Inc.  https://www.zerotier.com/
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

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

#define ZT_IDENTITY_STRING_BUFFER_LENGTH 384

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
	enum Type
	{
		C25519 = 0, // Curve25519 and Ed25519 (1.0 and 2.0, default)
		P384 = 1    // NIST P-384 ECDH and ECDSA (2.0+ only)
	};

	Identity() { memset(reinterpret_cast<void *>(this),0,sizeof(Identity)); }
	Identity(const Identity &id) { memcpy(reinterpret_cast<void *>(this),&id,sizeof(Identity)); }

	Identity(const char *str)
	{
		if (!fromString(str))
			throw ZT_EXCEPTION_INVALID_SERIALIZED_DATA_INVALID_TYPE;
	}

	template<unsigned int C>
	Identity(const Buffer<C> &b,unsigned int startAt = 0) { deserialize(b,startAt); }

	~Identity() { Utils::burn(reinterpret_cast<void *>(this),sizeof(Identity)); }

	inline void zero() { Utils::burn(reinterpret_cast<void *>(this),sizeof(Identity)); }

	inline Identity &operator=(const Identity &id)
	{
		memcpy(reinterpret_cast<void *>(this),&id,sizeof(Identity));
		return *this;
	}

	/**
	 * @return Identity type
	 */
	inline Type type() const { return _type; }

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
	inline bool hasPrivate() const { return _hasPrivate; }

	/**
	 * Compute the SHA512 hash of our private key (if we have one)
	 *
	 * @param sha Buffer to receive SHA512 (MUST be ZT_SHA512_DIGEST_LEN (64) bytes in length)
	 * @return True on success, false if no private key
	 */
	inline bool sha512PrivateKey(void *sha) const
	{
		if (_hasPrivate) {
			switch(_type) {
				case C25519:
					SHA512(sha,_k.t0.priv.data,ZT_C25519_PRIVATE_KEY_LEN);
					return true;
				case P384:
					SHA512(sha,_k.t1.priv,ZT_ECC384_PRIVATE_KEY_SIZE);
					return true;
			}
		}
		return false;
	}

	/**
	 * Compute the SHA512 hash of our public key
	 * 
	 * @param sha Buffer to receive hash bytes
	 * @return True on success, false if identity is empty or invalid
	 */
	inline bool sha512PublicKey(void *sha) const
	{
		if (_hasPrivate) {
			switch(_type) {
				case C25519:
					SHA512(sha,_k.t0.pub.data,ZT_C25519_PUBLIC_KEY_LEN);
					return true;
				case P384:
					SHA512(sha,_k.t1.pub,ZT_ECC384_PUBLIC_KEY_SIZE);
					return true;
			}
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
	 * @param klen Length of key in bytes
	 * @return Was agreement successful?
	 */
	bool agree(const Identity &id,void *key,unsigned int klen) const;

	/**
	 * @return This identity's address
	 */
	inline const Address &address() const { return _address; }

	/**
	 * Serialize this identity (binary)
	 *
	 * @param b Destination buffer to append to
	 * @param includePrivate If true, include private key component (if present) (default: false)
	 * @throws std::out_of_range Buffer too small
	 */
	template<unsigned int C>
	inline void serialize(Buffer<C> &b,bool includePrivate = false) const
	{
		_address.appendTo(b);
		switch(_type) {

			case C25519:
				b.append((uint8_t)C25519);
				b.append(_k.t0.pub.data,ZT_C25519_PUBLIC_KEY_LEN);
				if ((_hasPrivate)&&(includePrivate)) {
					b.append((uint8_t)ZT_C25519_PRIVATE_KEY_LEN);
					b.append(_k.t0.priv.data,ZT_C25519_PRIVATE_KEY_LEN);
				} else {
					b.append((uint8_t)0);
				}
				break;

			case P384:
				b.append((uint8_t)P384);
				b.append(_k.t1.pub,ZT_ECC384_PUBLIC_KEY_SIZE);
				if ((_hasPrivate)&&(includePrivate)) {
					b.append((uint8_t)ZT_ECC384_PRIVATE_KEY_SIZE);
					b.append(_k.t1.priv,ZT_ECC384_PRIVATE_KEY_SIZE);
				} else {
					b.append((uint8_t)0);
				}
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
	inline unsigned int deserialize(const Buffer<C> &b,unsigned int startAt = 0)
	{
		_hasPrivate = false;
		unsigned int p = startAt;
		unsigned int pkl;

		_address.setTo(b.field(p,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH);
		p += ZT_ADDRESS_LENGTH;

		_type = (Type)b[p++];
		switch(_type) {

			case C25519:
				memcpy(_k.t0.pub.data,b.field(p,ZT_C25519_PUBLIC_KEY_LEN),ZT_C25519_PUBLIC_KEY_LEN);
				p += ZT_C25519_PUBLIC_KEY_LEN;
				pkl = (unsigned int)b[p++];
				if (pkl) {
					if (pkl != ZT_C25519_PRIVATE_KEY_LEN)
						throw ZT_EXCEPTION_INVALID_SERIALIZED_DATA_INVALID_CRYPTOGRAPHIC_TOKEN;
					_hasPrivate = true;
					memcpy(_k.t0.priv.data,b.field(p,ZT_C25519_PRIVATE_KEY_LEN),ZT_C25519_PRIVATE_KEY_LEN);
					p += ZT_C25519_PRIVATE_KEY_LEN;
				} else {
					memset(_k.t0.priv.data,0,ZT_C25519_PRIVATE_KEY_LEN);
					_hasPrivate = false;
				}
				break;

			case P384:
				memcpy(_k.t0.pub.data,b.field(p,ZT_ECC384_PUBLIC_KEY_SIZE),ZT_ECC384_PUBLIC_KEY_SIZE);
				p += ZT_ECC384_PUBLIC_KEY_SIZE;
				pkl = (unsigned int)b[p++];
				if (pkl) {
					if (pkl != ZT_ECC384_PRIVATE_KEY_SIZE)
						throw ZT_EXCEPTION_INVALID_SERIALIZED_DATA_INVALID_CRYPTOGRAPHIC_TOKEN;
					_hasPrivate = true;
					memcpy(_k.t1.priv,b.field(p,ZT_ECC384_PRIVATE_KEY_SIZE),ZT_ECC384_PRIVATE_KEY_SIZE);
					p += ZT_ECC384_PRIVATE_KEY_SIZE;
				} else {
					memset(_k.t1.priv,0,ZT_ECC384_PRIVATE_KEY_SIZE);
					_hasPrivate = false;
				}
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
	inline operator bool() const { return (_address); }

	inline bool operator==(const Identity &id) const
	{
		if ((_address == id._address)&&(_type == id._type)) {
			switch(_type) {
				case C25519:
					return (memcmp(_k.t0.pub.data,id._k.t0.pub.data,ZT_C25519_PUBLIC_KEY_LEN) == 0);
				case P384:
					return (memcmp(_k.t1.pub,id._k.t1.pub,ZT_ECC384_PUBLIC_KEY_SIZE) == 0);
				default:
					return false;
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
					case C25519:
						return (memcmp(_k.t0.pub.data,id._k.t0.pub.data,ZT_C25519_PUBLIC_KEY_LEN) < 0);
					case P384:
						return (memcmp(_k.t1.pub,id._k.t1.pub,ZT_ECC384_PUBLIC_KEY_SIZE) < 0);
				}
			}
		}
		return false;
	}
	inline bool operator!=(const Identity &id) const { return !(*this == id); }
	inline bool operator>(const Identity &id) const { return (id < *this); }
	inline bool operator<=(const Identity &id) const { return !(id < *this); }
	inline bool operator>=(const Identity &id) const { return !(*this < id); }

	inline unsigned long hashCode() const { return (unsigned long)_address.toInt(); }

private:
	Address _address;
	union {
		struct {
			C25519::Public pub;
			C25519::Private priv;
		} t0;
		struct {
			uint8_t pub[ZT_ECC384_PUBLIC_KEY_SIZE];
			uint8_t priv[ZT_ECC384_PRIVATE_KEY_SIZE];
		} t1;
	} _k;
	Type _type;
	bool _hasPrivate;
};

} // namespace ZeroTier

#endif
