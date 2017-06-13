/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2017  ZeroTier, Inc.  https://www.zerotier.com/
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
#include <string>

#include "Constants.hpp"
#include "Array.hpp"
#include "Utils.hpp"
#include "Address.hpp"
#include "C25519.hpp"
#include "Buffer.hpp"
#include "SHA512.hpp"

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
	Identity() :
		_privateKey((C25519::Private *)0)
	{
	}

	Identity(const Identity &id) :
		_address(id._address),
		_publicKey(id._publicKey),
		_privateKey((id._privateKey) ? new C25519::Private(*(id._privateKey)) : (C25519::Private *)0)
	{
	}

	Identity(const char *str)
		throw(std::invalid_argument) :
		_privateKey((C25519::Private *)0)
	{
		if (!fromString(str))
			throw std::invalid_argument(std::string("invalid string-serialized identity: ") + str);
	}

	Identity(const std::string &str)
		throw(std::invalid_argument) :
		_privateKey((C25519::Private *)0)
	{
		if (!fromString(str))
			throw std::invalid_argument(std::string("invalid string-serialized identity: ") + str);
	}

	template<unsigned int C>
	Identity(const Buffer<C> &b,unsigned int startAt = 0) :
		_privateKey((C25519::Private *)0)
	{
		deserialize(b,startAt);
	}

	~Identity()
	{
		delete _privateKey;
	}

	inline Identity &operator=(const Identity &id)
	{
		_address = id._address;
		_publicKey = id._publicKey;
		if (id._privateKey) {
			if (!_privateKey)
				_privateKey = new C25519::Private();
			*_privateKey = *(id._privateKey);
		} else {
			delete _privateKey;
			_privateKey = (C25519::Private *)0;
		}
		return *this;
	}

	/**
	 * Generate a new identity (address, key pair)
	 *
	 * This is a time consuming operation.
	 */
	void generate();

	/**
	 * Check the validity of this identity's pairing of key to address
	 *
	 * @return True if validation check passes
	 */
	bool locallyValidate() const;

	/**
	 * @return True if this identity contains a private key
	 */
	inline bool hasPrivate() const throw() { return (_privateKey != (C25519::Private *)0); }

	/**
	 * Compute the SHA512 hash of our private key (if we have one)
	 *
	 * @param sha Buffer to receive SHA512 (MUST be ZT_SHA512_DIGEST_LEN (64) bytes in length)
	 * @return True on success, false if no private key
	 */
	inline bool sha512PrivateKey(void *sha) const
	{
		if (_privateKey) {
			SHA512::hash(sha,_privateKey->data,ZT_C25519_PRIVATE_KEY_LEN);
			return true;
		}
		return false;
	}

	/**
	 * Sign a message with this identity (private key required)
	 *
	 * @param data Data to sign
	 * @param len Length of data
	 */
	inline C25519::Signature sign(const void *data,unsigned int len) const
		throw(std::runtime_error)
	{
		if (_privateKey)
			return C25519::sign(*_privateKey,_publicKey,data,len);
		throw std::runtime_error("sign() requires a private key");
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
	inline bool verify(const void *data,unsigned int len,const void *signature,unsigned int siglen) const
	{
		if (siglen != ZT_C25519_SIGNATURE_LEN)
			return false;
		return C25519::verify(_publicKey,data,len,signature);
	}

	/**
	 * Verify a message signature against this identity
	 *
	 * @param data Data to check
	 * @param len Length of data
	 * @param signature Signature
	 * @return True if signature validates and data integrity checks
	 */
	inline bool verify(const void *data,unsigned int len,const C25519::Signature &signature) const
	{
		return C25519::verify(_publicKey,data,len,signature);
	}

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
	inline bool agree(const Identity &id,void *key,unsigned int klen) const
	{
		if (_privateKey) {
			C25519::agree(*_privateKey,id._publicKey,key,klen);
			return true;
		}
		return false;
	}

	/**
	 * @return This identity's address
	 */
	inline const Address &address() const throw() { return _address; }

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
		b.append((uint8_t)0); // C25519/Ed25519 identity type
		b.append(_publicKey.data,(unsigned int)_publicKey.size());
		if ((_privateKey)&&(includePrivate)) {
			b.append((unsigned char)_privateKey->size());
			b.append(_privateKey->data,(unsigned int)_privateKey->size());
		} else b.append((unsigned char)0);
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
		delete _privateKey;
		_privateKey = (C25519::Private *)0;

		unsigned int p = startAt;

		_address.setTo(b.field(p,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH);
		p += ZT_ADDRESS_LENGTH;

		if (b[p++] != 0)
			throw std::invalid_argument("unsupported identity type");

		memcpy(_publicKey.data,b.field(p,(unsigned int)_publicKey.size()),(unsigned int)_publicKey.size());
		p += (unsigned int)_publicKey.size();

		unsigned int privateKeyLength = (unsigned int)b[p++];
		if (privateKeyLength) {
			if (privateKeyLength != ZT_C25519_PRIVATE_KEY_LEN)
				throw std::invalid_argument("invalid private key");
			_privateKey = new C25519::Private();
			memcpy(_privateKey->data,b.field(p,ZT_C25519_PRIVATE_KEY_LEN),ZT_C25519_PRIVATE_KEY_LEN);
			p += ZT_C25519_PRIVATE_KEY_LEN;
		}

		return (p - startAt);
	}

	/**
	 * Serialize to a more human-friendly string
	 *
	 * @param includePrivate If true, include private key (if it exists)
	 * @return ASCII string representation of identity
	 */
	std::string toString(bool includePrivate) const;

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
	inline bool fromString(const std::string &str) { return fromString(str.c_str()); }

	/**
	 * @return C25519 public key
	 */
	inline const C25519::Public &publicKey() const { return _publicKey; }

	/**
	 * @return C25519 key pair (only returns valid pair if private key is present in this Identity object)
	 */
	inline const C25519::Pair privateKeyPair() const
	{
		C25519::Pair pair;
		pair.pub = _publicKey;
		if (_privateKey)
			pair.priv = *_privateKey;
		else memset(pair.priv.data,0,ZT_C25519_PRIVATE_KEY_LEN);
		return pair;
	}

	/**
	 * @return True if this identity contains something
	 */
	inline operator bool() const throw() { return (_address); }

	inline bool operator==(const Identity &id) const throw() { return ((_address == id._address)&&(_publicKey == id._publicKey)); }
	inline bool operator<(const Identity &id) const throw() { return ((_address < id._address)||((_address == id._address)&&(_publicKey < id._publicKey))); }
	inline bool operator!=(const Identity &id) const throw() { return !(*this == id); }
	inline bool operator>(const Identity &id) const throw() { return (id < *this); }
	inline bool operator<=(const Identity &id) const throw() { return !(id < *this); }
	inline bool operator>=(const Identity &id) const throw() { return !(*this < id); }

private:
	Address _address;
	C25519::Public _publicKey;
	C25519::Private *_privateKey;
};

} // namespace ZeroTier

#endif
