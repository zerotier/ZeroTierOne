/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2012-2013  ZeroTier Networks LLC
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
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

#ifndef _ZT_IDENTITY_HPP
#define _ZT_IDENTITY_HPP

#include <stdio.h>
#include <stdlib.h>
#include <string>

#include "Constants.hpp"
#include "Array.hpp"
#include "Utils.hpp"
#include "Address.hpp"
#include "C25519.hpp"
#include "Buffer.hpp"

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
	 * Identity types
	 */
	enum Type
	{
		IDENTITY_TYPE_NIST_P_521 = 1, // OBSOLETE -- only present in some early alpha versions
		IDENTITY_TYPE_C25519 = 2
	};

	Identity() :
		_privateKey((C25519::Private *)0)
	{
	}

	Identity(const Identity &id) :
		_address(id._address),
		_publicKey(id._publicKey),
		_signature(id._signature),
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
	Identity(const Buffer<C> &b,unsigned int startAt = 0)
		throw(std::out_of_range,std::invalid_argument) :
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
		_signature = id._signature;
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
	 * Performs local validation, with two levels available
	 * 
	 * With the parameter false, this performs self-signature verification
	 * which checks the basic integrity of the key and identity. Setting the
	 * parameter to true performs a fairly time consuming computation to
	 * check that the address was properly derived from the key. This is
	 * normally not done unless a conflicting identity is received, in
	 * which case the invalid identity is thrown out.
	 * 
	 * @param doAddressDerivationCheck If true, do the time-consuming address check
	 * @return True if validation check passes
	 */
	bool locallyValidate(bool doAddressDerivationCheck) const;

	/**
	 * @return True if this identity contains a private key
	 */
	inline bool hasPrivate() const throw() { return (_privateKey != (C25519::Private *)0); }

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
	 * @return Identity type
	 */
	inline Type type() const throw() { return IDENTITY_TYPE_C25519; }

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
		throw(std::out_of_range)
	{
		_address.appendTo(b);
		b.append((unsigned char)IDENTITY_TYPE_C25519);
		b.append(_publicKey.data,_publicKey.size());
		b.append(_signature.data,_signature.size());
		if ((_privateKey)&&(includePrivate)) {
			b.append((unsigned char)_privateKey.size());
			b.append(_privateKey.data,_privateKey.size());
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
	 * @throws std::out_of_range Buffer too small
	 * @throws std::invalid_argument Serialized data invalid
	 */
	template<unsigned int C>
	inline unsigned int deserialize(const Buffer<C> &b,unsigned int startAt = 0)
		throw(std::out_of_range,std::invalid_argument)
	{
		delete _privateKey;
		_privateKey = (C25519::Private *)0;

		unsigned int p = startAt;

		_address.setTo(b.field(p,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH);
		p += ZT_ADDRESS_LENGTH;

		if (b[p++] != IDENTITY_TYPE_C25519)
			throw std::invalid_argument("Identity: deserialize(): unsupported identity type");

		memcpy(_publicKey.data,field(p,_publicKey.size()),_publicKey.size());
		p += _publicKey.size();
		memcpy(_signature.data,field(p,_signature.size()),_signature.size());
		p += _signature.size();

		unsigned int privateKeyLength = b[p++];
		if ((privateKeyLength)&&(privateKeyLength == ZT_C25519_PRIVATE_KEY_LEN)) {
			_privateKey = new C25519::Private();
			memcpy(_privateKey->data,field(p,ZT_C25519_PRIVATE_KEY_LEN),ZT_C25519_PRIVATE_KEY_LEN);
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
	// Compute an address from public key bytes
	static Address deriveAddress(const void *keyBytes,unsigned int keyLen);

	Address _address;
	C25519::Public _publicKey;
	C25519::Signature _signature;
	C25519::Private *_privateKey;
};

} // namespace ZeroTier

#endif
