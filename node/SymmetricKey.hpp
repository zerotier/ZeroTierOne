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

#ifndef ZT_SYMMETRICKEY_HPP
#define ZT_SYMMETRICKEY_HPP

#include "Constants.hpp"
#include "Utils.hpp"
#include "InetAddress.hpp"

namespace ZeroTier {

#define ZT_SYMMETRICKEY_MARSHAL_SIZE_MAX 52

/**
 * Container for symmetric keys and ciphers initialized with them
 *
 * This container is responsible for tracking key TTL to maintain it
 * below our security bounds and tell us when it's time to re-key.
 *
 * Set TTL and TTLM to 0 for permanent keys. These still track uses
 * but do not signal expiration.
 *
 * @tparam C Cipher to embed (must accept key in constructor and/or init() method)
 * @tparam TTL Maximum time to live in milliseconds or 0 for a permanent key with unlimited TTL
 * @tparam TTLM Maximum time to live in messages or 0 for a permanent key with unlimited TTL
 */
template<typename C,int64_t TTL,uint64_t TTLM>
class SymmetricKey
{
public:
	/**
	 * Symmetric cipher keyed with this key
	 */
	const C cipher;

	/**
	 * Construct an uninitialized symmetric key container
	 */
	ZT_INLINE SymmetricKey() noexcept : // NOLINT(cppcoreguidelines-pro-type-member-init,hicpp-member-init,hicpp-use-equals-default,modernize-use-equals-default)
		cipher(),
		_ts(0),
		_nonceBase(0),
		_odometer(0)
	{
		Utils::memoryLock(_k,sizeof(_k));
	}

	/**
	 * Construct a new symmetric key
	 *
	 * @param ts Current time (must still be given for permanent keys even though there is no expiry checking)
	 * @param key 32-byte / 256-bit key
	 */
	explicit ZT_INLINE SymmetricKey(const int64_t ts,const void *const key) noexcept : // NOLINT(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
		cipher(key),
		_ts(ts),
		_nonceBase((uint64_t)ts << 22U), // << 22 to shift approximately the seconds since epoch into the most significant 32 bits
		_odometer(0)
	{
		Utils::memoryLock(_k,sizeof(_k));
		Utils::copy<ZT_SYMMETRIC_KEY_SIZE>(_k,key);
	}

	ZT_INLINE SymmetricKey(const SymmetricKey &k) noexcept : // NOLINT(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
		cipher(k._k),
		_ts(k.ts),
		_nonceBase(k._nonceBase),
		_odometer(k._odometer)
	{
		Utils::memoryLock(_k,sizeof(_k));
		Utils::copy<ZT_SYMMETRIC_KEY_SIZE>(_k,k._k);
	}

	ZT_INLINE ~SymmetricKey() noexcept
	{
		Utils::burn(_k,sizeof(_k));
		Utils::memoryUnlock(_k,sizeof(_k));
	}

	ZT_INLINE SymmetricKey &operator=(const SymmetricKey &k) noexcept
	{
		if (&k != this) {
			cipher.init(k._k);
			_ts = k._ts;
			_nonceBase = k._nonceBase;
			_odometer = k._odometer;
			Utils::copy<ZT_SYMMETRIC_KEY_SIZE>(_k,k._k);
		}
		return *this;
	}

	/**
	 * Initialize or change the key wrapped by this SymmetricKey object
	 *
	 * If the supplied key is identical to the current key, no change occurs and false is returned.
	 *
	 * @param ts Current time
	 * @param key 32-byte / 256-bit key
	 * @return True if the symmetric key was changed
	 */
	ZT_INLINE bool init(const int64_t ts,const void *const key) noexcept
	{
		if ((_ts > 0)&&(memcmp(_k,key,ZT_SYMMETRIC_KEY_SIZE) == 0))
			return false;
		cipher.init(key);
		_ts = ts;
		_nonceBase = (uint64_t)ts << 22U; // << 22 to shift approximately the seconds since epoch into the most significant 32 bits;
		_odometer = 0;
		Utils::copy<ZT_SYMMETRIC_KEY_SIZE>(_k,key);
		return true;
	}

	/**
	 * Clear key and set to NIL value (boolean evaluates to false)
	 */
	ZT_INLINE void clear() noexcept
	{
		_ts = 0;
		_nonceBase = 0;
		_odometer = 0;
		Utils::zero<ZT_SYMMETRIC_KEY_SIZE>(_k);
	}

	/**
	 * Check whether this symmetric key may be expiring soon
	 *
	 * @param now Current time
	 * @return True if re-keying should happen
	 */
	ZT_INLINE bool expiringSoon(const int64_t now) const noexcept
	{
		return (TTL > 0) && ( ((now - _ts) >= (TTL / 2)) || (_odometer >= (TTLM / 2)) );
	}

	/**
	 * Check whether this symmetric key is expired due to too much time or too many messages
	 *
	 * @param now Current time
	 * @return True if this symmetric key should no longer be used
	 */
	ZT_INLINE bool expired(const int64_t now) const noexcept
	{
		return (TTL > 0) && ( ((now - _ts) >= TTL) || (_odometer >= TTLM) );
	}

	/**
	 * @return True if this is a never-expiring key, such as the identity key created by identity key agreement
	 */
	constexpr bool permanent() const noexcept
	{
		return TTL == 0;
	}

	/**
	 * Get the raw key that was used to initialize the cipher.
	 *
	 * @return 32-byte / 256-bit symmetric key
	 */
	ZT_INLINE const uint8_t *key() const noexcept
	{
		return _k;
	}

	/**
	 * Advance usage counter by one and return the next unique initialization vector for a new message.
	 *
	 * @return Next unique IV for next message
	 */
	ZT_INLINE uint64_t nextMessageIv() noexcept
	{
		return _nonceBase + _odometer++;
	}

	/**
	 * @return True if this object is not NIL
	 */
	ZT_INLINE operator bool() const noexcept { return (_ts > 0); } // NOLINT(google-explicit-constructor,hicpp-explicit-conversions)

	static constexpr int marshalSizeMax() noexcept { return ZT_SYMMETRICKEY_MARSHAL_SIZE_MAX; }

	/**
	 * Marshal with encryption at rest
	 *
	 * @tparam MC Cipher type (AES in our code) to use for encryption at rest
	 * @param keyEncCipher Initialized cipher
	 * @param data Destination for marshaled key
	 * @return Bytes written or -1 on error
	 */
	template<typename MC>
	ZT_INLINE int marshal(const MC &keyEncCipher,uint8_t data[ZT_SYMMETRICKEY_MARSHAL_SIZE_MAX]) const noexcept
	{
		Utils::storeBigEndian<uint64_t>(data,(uint64_t)_ts);
		Utils::storeBigEndian<uint64_t>(data + 8,_odometer.load());
		Utils::storeBigEndian<uint32_t>(data + 16,Utils::fnv1a32(_k,sizeof(_k)));

		// Key encryption at rest is CBC using the last 32 bits of the timestamp, the odometer,
		// and the FNV1a checksum as a 128-bit IV. A duplicate IV wouldn't matter much anyway since
		// keys should be unique. Simple ECB would be fine as they also have no structure, but this
		// looks better.
		uint8_t tmp[16];
		for(int i=0;i<16;++i)
			tmp[i] = data[i + 4] ^ _k[i];
		keyEncCipher.encrypt(tmp,data + 20);
		for(int i=0;i<16;++i)
			tmp[i] = data[i + 20] ^ _k[i + 16];
		keyEncCipher.encrypt(tmp,data + 36);

		return ZT_SYMMETRICKEY_MARSHAL_SIZE_MAX;
	}

	/**
	 * Unmarshal, decrypt, and verify key checksum
	 *
	 * Key checksum verification failure results in the SymmetricKey being zeroed out to its
	 * nil value, but the bytes read are still returned. The caller must check this if it
	 * requires the key to be present and verified.
	 *
	 * @tparam MC Cipher type (AES in our code) to use for encryption at rest
	 * @param keyDecCipher Initialized cipher for decryption
	 * @param data Source to read
	 * @param len Bytes remaining at source
	 * @return Bytes read from source
	 */
	template<typename MC>
	ZT_INLINE int unmarshal(const MC &keyDecCipher,const uint8_t *restrict data,int len) noexcept
	{
		if (len < ZT_SYMMETRICKEY_MARSHAL_SIZE_MAX)
			return -1;

		_ts = (int64_t)Utils::loadBigEndian<uint64_t>(data);
		_odometer = (uint64_t)Utils::loadBigEndian<uint64_t>(data + 8);
		const uint32_t fnv = Utils::loadBigEndian<uint32_t>(data + 16); // NOLINT(hicpp-use-auto,modernize-use-auto)

		uint8_t tmp[16];
		keyDecCipher.decrypt(data + 20,tmp);
		for(int i=0;i<16;++i)
			_k[i] = data[i + 4] ^ tmp[i];
		keyDecCipher.decrypt(data + 36,tmp);
		for(int i=0;i<16;++i)
			_k[i + 16] = data[i + 20] ^ tmp[i];

		if (Utils::fnv1a32(_k,sizeof(_k)) != fnv)
			clear();

		return ZT_SYMMETRICKEY_MARSHAL_SIZE_MAX;
	}

private:
	int64_t _ts;
	uint64_t _nonceBase;
	std::atomic<uint64_t> _odometer;
	uint8_t _k[ZT_SYMMETRIC_KEY_SIZE];
};

} // namespace ZeroTier

#endif
