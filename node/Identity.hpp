/*
 * Copyright (c)2019 ZeroTier, Inc.
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

#ifndef ZT_IDENTITY_HPP
#define ZT_IDENTITY_HPP

#include <stdio.h>
#include <stdlib.h>

#include "Constants.hpp"
#include "Utils.hpp"
#include "Address.hpp"
#include "ECC.hpp"
#include "Buffer.hpp"
#include "SHA512.hpp"

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
	Identity() :
		_privateKey((ECC::Private *)0)
	{
	}

	Identity(const Identity &id) :
		_address(id._address),
		_publicKey(id._publicKey),
		_privateKey((id._privateKey) ? new ECC::Private(*(id._privateKey)) : (ECC::Private *)0)
	{
	}

	Identity(const char *str) :
		_privateKey((ECC::Private *)0)
	{
		if (!fromString(str)) {
			throw ZT_EXCEPTION_INVALID_SERIALIZED_DATA_INVALID_TYPE;
		}
	}

	template<unsigned int C>
	Identity(const Buffer<C> &b,unsigned int startAt = 0) :
		_privateKey((ECC::Private *)0)
	{
		deserialize(b,startAt);
	}

	~Identity()
	{
		if (_privateKey) {
			Utils::burn(_privateKey,sizeof(ECC::Private));
			delete _privateKey;
		}
	}

	inline Identity &operator=(const Identity &id)
	{
		_address = id._address;
		_publicKey = id._publicKey;
		if (id._privateKey) {
			if (!_privateKey) {
				_privateKey = new ECC::Private();
			}
			*_privateKey = *(id._privateKey);
		} else {
			delete _privateKey;
			_privateKey = (ECC::Private *)0;
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
	inline bool hasPrivate() const { return (_privateKey != (ECC::Private *)0); }

	/**
	 * Compute a SHA384 hash of this identity's address and public key(s).
	 *
	 * @param sha384buf Buffer with 48 bytes of space to receive hash
	 */
	inline void publicKeyHash(void *sha384buf) const
	{
		uint8_t address[ZT_ADDRESS_LENGTH];
		_address.copyTo(address, ZT_ADDRESS_LENGTH);
		SHA384(sha384buf, address, ZT_ADDRESS_LENGTH, _publicKey.data, ZT_ECC_PUBLIC_KEY_SET_LEN);
	}

	/**
	 * Compute the SHA512 hash of our private key (if we have one)
	 *
	 * @param sha Buffer to receive SHA512 (MUST be ZT_SHA512_DIGEST_LEN (64) bytes in length)
	 * @return True on success, false if no private key
	 */
	inline bool sha512PrivateKey(void *sha) const
	{
		if (_privateKey) {
			SHA512(sha,_privateKey->data,ZT_ECC_PRIVATE_KEY_SET_LEN);
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
	inline ECC::Signature sign(const void *data,unsigned int len) const
	{
		if (_privateKey) {
			return ECC::sign(*_privateKey,_publicKey,data,len);
		}
		throw ZT_EXCEPTION_PRIVATE_KEY_REQUIRED;
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
		if (siglen != ZT_ECC_SIGNATURE_LEN) {
			return false;
		}
		return ECC::verify(_publicKey,data,len,signature);
	}

	/**
	 * Verify a message signature against this identity
	 *
	 * @param data Data to check
	 * @param len Length of data
	 * @param signature Signature
	 * @return True if signature validates and data integrity checks
	 */
	inline bool verify(const void *data,unsigned int len,const ECC::Signature &signature) const
	{
		return ECC::verify(_publicKey,data,len,signature);
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
	inline bool agree(const Identity &id,void *const key) const
	{
		if (_privateKey) {
			ECC::agree(*_privateKey,id._publicKey,key,ZT_SYMMETRIC_KEY_SIZE);
			return true;
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
	 * @throws std::out_of_range Buffer too small
	 */
	template<unsigned int C>
	inline void serialize(Buffer<C> &b,bool includePrivate = false) const
	{
		_address.appendTo(b);
		b.append((uint8_t)0); // C25519/Ed25519 identity type
		b.append(_publicKey.data,ZT_ECC_PUBLIC_KEY_SET_LEN);
		if ((_privateKey)&&(includePrivate)) {
			b.append((unsigned char)ZT_ECC_PRIVATE_KEY_SET_LEN);
			b.append(_privateKey->data,ZT_ECC_PRIVATE_KEY_SET_LEN);
		} else {
			b.append((unsigned char)0);
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
		delete _privateKey;
		_privateKey = (ECC::Private *)0;

		unsigned int p = startAt;

		_address.setTo(b.field(p,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH);
		p += ZT_ADDRESS_LENGTH;

		if (b[p++] != 0) {
			throw ZT_EXCEPTION_INVALID_SERIALIZED_DATA_INVALID_TYPE;
		}

		memcpy(_publicKey.data,b.field(p,ZT_ECC_PUBLIC_KEY_SET_LEN),ZT_ECC_PUBLIC_KEY_SET_LEN);
		p += ZT_ECC_PUBLIC_KEY_SET_LEN;

		unsigned int privateKeyLength = (unsigned int)b[p++];
		if (privateKeyLength) {
			if (privateKeyLength != ZT_ECC_PRIVATE_KEY_SET_LEN) {
				throw ZT_EXCEPTION_INVALID_SERIALIZED_DATA_INVALID_CRYPTOGRAPHIC_TOKEN;
			}
			_privateKey = new ECC::Private();
			memcpy(_privateKey->data,b.field(p,ZT_ECC_PRIVATE_KEY_SET_LEN),ZT_ECC_PRIVATE_KEY_SET_LEN);
			p += ZT_ECC_PRIVATE_KEY_SET_LEN;
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
	 * @return C25519 public key
	 */
	inline const ECC::Public &publicKey() const { return _publicKey; }

	/**
	 * @return C25519 key pair (only returns valid pair if private key is present in this Identity object)
	 */
	inline const ECC::Pair privateKeyPair() const
	{
		ECC::Pair pair;
		pair.pub = _publicKey;
		if (_privateKey) {
			pair.priv = *_privateKey;
		} else {
			memset(pair.priv.data,0,ZT_ECC_PRIVATE_KEY_SET_LEN);
		}
		return pair;
	}

	/**
	 * @return True if this identity contains something
	 */
	inline operator bool() const { return (_address); }

	inline bool operator==(const Identity &id) const { return ((_address == id._address)&&(memcmp(_publicKey.data,id._publicKey.data,ZT_ECC_PUBLIC_KEY_SET_LEN) == 0)); }
	inline bool operator<(const Identity &id) const { return ((_address < id._address)||((_address == id._address)&&(memcmp(_publicKey.data,id._publicKey.data,ZT_ECC_PUBLIC_KEY_SET_LEN) < 0))); }
	inline bool operator!=(const Identity &id) const { return !(*this == id); }
	inline bool operator>(const Identity &id) const { return (id < *this); }
	inline bool operator<=(const Identity &id) const { return !(id < *this); }
	inline bool operator>=(const Identity &id) const { return !(*this < id); }

private:
	Address _address;
	ECC::Public _publicKey;
	ECC::Private *_privateKey;
};

} // namespace ZeroTier

#endif
