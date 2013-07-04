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

#include "EllipticCurveKey.hpp"
#include "EllipticCurveKeyPair.hpp"
#include "Array.hpp"
#include "Utils.hpp"
#include "Address.hpp"
#include "Buffer.hpp"

/**
 * Maximum length for a serialized identity
 */
#define IDENTITY_MAX_BINARY_SERIALIZED_LENGTH ((ZT_EC_MAX_BYTES * 2) + 256)

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
 *
 * After derivation, the address must be checked against isReserved(). If the
 * address is reserved, generation is repeated until a valid address results.
 *
 * Serialization of an identity:
 *
 * <[5] address>		- 40-bit ZeroTier network address
 * <[1] type>			- Identity type ID (rest is type-dependent)
 * <[1] key length>		- Length of public key
 * <[n] public key>		- Elliptic curve public key
 * <[1] sig length>		- Length of ECDSA self-signature
 * <[n] signature>		- ECDSA signature of first four fields
 * [<[1] key length>]	- [Optional] Length of private key
 * [<[n] private key>]	- [Optional] Private key
 *
 * Local storage of an identity also requires storage of its private key.
 */
class Identity
{
public:
	/**
	 * Identity types
	 */
	enum Type
	{
		/* Elliptic curve NIST-P-521 and ECDSA signature */
		IDENTITY_TYPE_NIST_P_521 = 1
		/* We won't need another identity type until quantum computers with
		 * tens of thousands of qubits are a reality. */
	};

	Identity() :
		_keyPair((EllipticCurveKeyPair *)0)
	{
	}

	Identity(const Identity &id) :
		_keyPair((id._keyPair) ? new EllipticCurveKeyPair(*id._keyPair) : (EllipticCurveKeyPair *)0),
		_publicKey(id._publicKey),
		_address(id._address),
		_signature(id._signature)
	{
	}

	Identity(const char *str)
		throw(std::invalid_argument) :
		_keyPair((EllipticCurveKeyPair *)0)
	{
		if (!fromString(str))
			throw std::invalid_argument("invalid string-serialized identity");
	}

	Identity(const std::string &str)
		throw(std::invalid_argument) :
		_keyPair((EllipticCurveKeyPair *)0)
	{
		if (!fromString(str))
			throw std::invalid_argument("invalid string-serialized identity");
	}

	template<unsigned int C>
	Identity(const Buffer<C> &b,unsigned int startAt = 0)
		throw(std::out_of_range,std::invalid_argument) :
		_keyPair((EllipticCurveKeyPair *)0)
	{
		deserialize(b,startAt);
	}

	~Identity()
	{
		delete _keyPair;
	}

	inline Identity &operator=(const Identity &id)
	{
		_keyPair = (id._keyPair) ? new EllipticCurveKeyPair(*id._keyPair) : (EllipticCurveKeyPair *)0;
		_publicKey = id._publicKey;
		_address = id._address;
		_signature = id._signature;
		return *this;
	}

	/**
	 * Generate a new identity (address, key pair)
	 * 
	 * This is a somewhat time consuming operation by design, as the address
	 * is derived from the key using a purposefully expensive many-round
	 * hash/encrypt/hash operation. This took about two seconds on a 2.4ghz
	 * Intel Core i5 in 2013.
	 * 
	 * In the very unlikely event that a reserved address is created, generate
	 * will automatically run again.
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
	 * @return Private key pair or NULL if not included with this identity
	 */
	inline const EllipticCurveKeyPair *privateKeyPair() const throw() { return _keyPair; }

	/**
	 * @return True if this identity has its private portion
	 */
	inline bool hasPrivate() const throw() { return (_keyPair); }

	/**
	 * Encrypt a block of data to send to another identity
	 *
	 * This identity must have a secret key.
	 *
	 * The encrypted data format is:
	 *   <[8] Salsa20 initialization vector>
	 *   <[8] first 8 bytes of HMAC-SHA-256 of ciphertext>
	 *   <[...] encrypted compressed data>
	 *
	 * Keying is accomplished using agree() (KDF function is in the
	 * EllipticCurveKeyPair.cpp source) to generate 64 bytes of key. The first
	 * 32 bytes are used as the Salsa20 key, and the last 32 bytes are used
	 * as the HMAC key.
	 *
	 * @param to Identity of recipient of encrypted message
	 * @param data Data to encrypt
	 * @param len Length of data
	 * @return Encrypted data or empty string on failure
	 */
	std::string encrypt(const Identity &to,const void *data,unsigned int len) const;

	/**
	 * Decrypt a message encrypted with encrypt()
	 *
	 * This identity must have a secret key.
	 *
	 * @param from Identity of sender of encrypted message
	 * @param cdata Encrypted message
	 * @param len Length of encrypted message
	 * @return Decrypted data or empty string on failure
	 */
	std::string decrypt(const Identity &from,const void *cdata,unsigned int len) const;

	/**
	 * Shortcut method to perform key agreement with another identity
	 *
	 * This identity must have its private portion.
	 *
	 * @param id Identity to agree with
	 * @param key Result parameter to fill with key bytes
	 * @param klen Length of key in bytes
	 * @return Was agreement successful?
	 */
	inline bool agree(const Identity &id,void *key,unsigned int klen) const
	{
		if ((id)&&(_keyPair))
			return _keyPair->agree(id._publicKey,(unsigned char *)key,klen);
		return false;
	}

	/**
	 * Sign a hash with this identity's private key
	 *
	 * @param sha256 32-byte hash to sign
	 * @return ECDSA signature or empty string on failure or if identity has no private portion
	 */
	inline std::string sign(const void *sha256) const
	{
		if (_keyPair)
			return _keyPair->sign(sha256);
		return std::string();
	}

	/**
	 * Sign a block of data with this identity's private key
	 *
	 * This is a shortcut to SHA-256 hashing then signing.
	 *
	 * @param sha256 32-byte hash to sign
	 * @return ECDSA signature or empty string on failure or if identity has no private portion
	 */
	inline std::string sign(const void *data,unsigned int len) const
	{
		if (_keyPair)
			return _keyPair->sign(data,len);
		return std::string();
	}

	/**
	 * Verify something signed with this identity's public key
	 * 
	 * @param sha256 32-byte hash to verify
	 * @param sigbytes Signature bytes
	 * @param siglen Length of signature
	 * @return True if signature is valid
	 */
	inline bool verifySignature(const void *sha256,const void *sigbytes,unsigned int siglen) const
	{
		return EllipticCurveKeyPair::verify(sha256,_publicKey,sigbytes,siglen);
	}

	/**
	 * Verify something signed with this identity's public key
	 * 
	 * @param data Data to verify
	 * @param len Length of data to verify
	 * @param sigbytes Signature bytes
	 * @param siglen Length of signature
	 * @return True if signature is valid
	 */
	inline bool verifySignature(const void *data,unsigned int len,const void *sigbytes,unsigned int siglen) const
	{
		return EllipticCurveKeyPair::verify(data,len,_publicKey,sigbytes,siglen);
	}

	/**
	 * @return Public key (available in all identities)
	 */
	inline const EllipticCurveKey &publicKey() const throw() { return _publicKey; }

	/**
	 * @return Identity type
	 */
	inline Type type() const throw() { return IDENTITY_TYPE_NIST_P_521; }

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
		b.append(_address.data(),ZT_ADDRESS_LENGTH);
		b.append((unsigned char)IDENTITY_TYPE_NIST_P_521);
		b.append((unsigned char)(_publicKey.size() & 0xff));
		b.append(_publicKey.data(),_publicKey.size());
		b.append((unsigned char)(_signature.length() & 0xff));
		b.append(_signature);
		if ((includePrivate)&&(_keyPair)) {
			b.append((unsigned char)(_keyPair->priv().size() & 0xff));
			b.append(_keyPair->priv().data(),_keyPair->priv().size());
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
		delete _keyPair;
		_keyPair = (EllipticCurveKeyPair *)0;

		unsigned int p = startAt;

		_address = b.field(p,ZT_ADDRESS_LENGTH);
		p += ZT_ADDRESS_LENGTH;

		if (b[p++] != IDENTITY_TYPE_NIST_P_521)
			throw std::invalid_argument("Identity: deserialize(): unsupported identity type");

		unsigned int publicKeyLength = b[p++];
		if (!publicKeyLength)
			throw std::invalid_argument("Identity: deserialize(): no public key");
		_publicKey.set(b.field(p,publicKeyLength),publicKeyLength);
		p += publicKeyLength;

		unsigned int signatureLength = b[p++];
		if (!signatureLength)
			throw std::invalid_argument("Identity: deserialize(): no signature");
		_signature.assign((const char *)b.field(p,signatureLength),signatureLength);
		p += signatureLength;

		unsigned int privateKeyLength = b[p++];
		if (privateKeyLength) {
			_keyPair = new EllipticCurveKeyPair(_publicKey,EllipticCurveKey(b.field(p,privateKeyLength),privateKeyLength));
			p += privateKeyLength;
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
	inline operator bool() const throw() { return (_publicKey.size()); }

	inline bool operator==(const Identity &id) const
		throw()
	{
		if (_address == id._address) {
			if ((_keyPair)&&(id._keyPair))
				return (*_keyPair == *id._keyPair);
			return (_publicKey == id._publicKey);
		}
		return false;
	}
	inline bool operator<(const Identity &id) const
		throw()
	{
		if (_address < id._address)
			return true;
		else if (_address == id._address)
			return (_publicKey < id._publicKey);
		return false;
	}
	inline bool operator!=(const Identity &id) const throw() { return !(*this == id); }
	inline bool operator>(const Identity &id) const throw() { return (id < *this); }
	inline bool operator<=(const Identity &id) const throw() { return !(id < *this); }
	inline bool operator>=(const Identity &id) const throw() { return !(*this < id); }

private:
	// Compute an address from public key bytes
	static Address deriveAddress(const void *keyBytes,unsigned int keyLen);

	EllipticCurveKeyPair *_keyPair;
	EllipticCurveKey _publicKey;
	Address _address;
	std::string _signature;
};

} // namespace ZeroTier

#endif
