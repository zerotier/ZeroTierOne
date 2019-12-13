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

#ifndef ZT_EPHEMERALKEY_HPP
#define ZT_EPHEMERALKEY_HPP

#include "Constants.hpp"
#include "C25519.hpp"
#include "ECC384.hpp"
#include "SHA512.hpp"
#include "Buffer.hpp"
#include "Utils.hpp"

namespace ZeroTier {

#define ZT_EPHEMERAL_KEY_TYPE_1_PUBLIC_SIZE (ZT_C25519_PUBLIC_KEY_LEN + ZT_ECC384_PUBLIC_KEY_SIZE)
#define ZT_EPHEMERAL_KEY_TYPE_1_PRIVATE_SIZE (ZT_C25519_PRIVATE_KEY_LEN + ZT_ECC384_PRIVATE_KEY_SIZE)

/**
 * An ephemeral key exchanged to implement forward secrecy
 *
 * This key includes both C25519 and ECC384 keys and key agreement executes
 * ECDH for both and hashes the results together. This should be able to be
 * FIPS compliant (if the C25519 portion is just considered a nonce) while
 * simultaneously being more secure than either curve alone.
 *
 * Serialization includes only the public portion since ephemeral private
 * keys are never shared or stored anywhere.
 */
class EphemeralKey
{
public:
	enum Type
	{
		NONE = 0,
		C25519ECC384 = 1
	};

	inline EphemeralKey() : _priv(nullptr),_type(NONE) {}

	inline ~EphemeralKey()
	{
		if (_priv) {
			Utils::burn(_priv,ZT_EPHEMERAL_KEY_TYPE_1_PRIVATE_SIZE);
			delete [] _priv;
		}
	}

	inline Type type() const { return (Type)_type; }
	inline bool hasPrivate() const { return (_priv != nullptr); }

	inline void generate()
	{
		if (!_priv)
			_priv = new uint8_t[ZT_EPHEMERAL_KEY_TYPE_1_PRIVATE_SIZE];
		C25519::generate(_pub,_priv);
		ECC384GenerateKey(_pub + ZT_C25519_PUBLIC_KEY_LEN,_priv + ZT_C25519_PRIVATE_KEY_LEN);
		_type = C25519ECC384;
	}

	inline bool agree(const EphemeralKey &theirs,uint8_t key[ZT_PEER_SECRET_KEY_LENGTH]) const
	{
		if ((_priv)&&(_type == 1)) {
			uint8_t rawkey[128],h[48];
			C25519::agree(_priv,theirs._pub,rawkey);
			ECC384ECDH(theirs._pub + ZT_C25519_PUBLIC_KEY_LEN,_priv + ZT_C25519_PRIVATE_KEY_LEN,rawkey + ZT_C25519_SHARED_KEY_LEN);
			SHA384(h,rawkey,ZT_C25519_SHARED_KEY_LEN + ZT_ECC384_SHARED_SECRET_SIZE);
			memcpy(key,h,ZT_PEER_SECRET_KEY_LENGTH);
			return true;
		}
		return false;
	}

	template<unsigned int C>
	inline void serialize(Buffer<C> &b) const
	{
		b.append(_type);
		if (_type == C25519ECC384)
			b.append(_pub,ZT_EPHEMERAL_KEY_TYPE_1_PUBLIC_SIZE);
	}

	template<unsigned int C>
	inline unsigned int deserialize(const Buffer<C> &b,unsigned int startAt = 0)
	{
		unsigned int p = startAt;
		delete [] _priv;
		_priv = nullptr;
		switch(b[p++]) {
			case C25519ECC384:
				memcpy(_pub,b.field(p,ZT_EPHEMERAL_KEY_TYPE_1_PUBLIC_SIZE),ZT_EPHEMERAL_KEY_TYPE_1_PUBLIC_SIZE);
				p += ZT_EPHEMERAL_KEY_TYPE_1_PUBLIC_SIZE;
				break;
			default:
				_type = NONE;
				break;
		}
		return (p - startAt);
	}

private:
	uint8_t *_priv;
	uint8_t _pub[ZT_EPHEMERAL_KEY_TYPE_1_PUBLIC_SIZE];
	uint8_t _type;
};

} // namespace ZeroTier

#endif
