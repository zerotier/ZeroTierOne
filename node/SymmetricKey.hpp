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
#include "AES.hpp"
#include "SharedPtr.hpp"
#include "Address.hpp"

namespace ZeroTier {

/**
 * Container for symmetric keys and ciphers initialized with them
 *
 * This container is responsible for tracking key TTL to maintain it
 * below our security bounds and tell us when it's time to re-key.
 */
class SymmetricKey
{
	friend class SharedPtr<SymmetricKey>;

public:
	/**
	 * Secret key
	 */
	const uint8_t secret[ZT_SYMMETRIC_KEY_SIZE];

	/**
	 * Symmetric cipher keyed with this key
	 */
	const AES cipher;

	/**
	 * Construct a new symmetric key
	 *
	 * SECURITY: we use a best effort method to construct the nonce's starting point so as
	 * to avoid nonce duplication across invocations. The most significant bits are the
	 * number of seconds since epoch but with the most significant bit masked to zero.
	 * The least significant bits are random. Key life time is limited to 2^31 messages
	 * per key as per the AES-GMAC-SIV spec, and this is a SIV mode anyway so nonce repetition
	 * is non-catastrophic.
	 * 
	 * The masking of the most significant bit is because we bisect the nonce space by
	 * which direction the message is going. If the sender's ZeroTier address is
	 * numerically greater than the receiver, this bit is flipped. This means that
	 * two sides of a conversation that have created their key instances at the same
	 * time are much less likely to duplicate nonces when sending pacekts from either
	 * end.
	 * 
	 * @param ts Current time
	 * @param key 48-bit / 384-byte key
	 * @param perm If true this is a permanent key
	 */
	explicit ZT_INLINE SymmetricKey(const int64_t ts,const void *const key,const bool perm) noexcept :
		secret(),
		cipher(key), // uses first 256 bits of 384-bit key
		m_ts(ts),
		m_nonceBase(((((uint64_t)ts / 1000ULL) << 32U) & 0x7fffffff00000000ULL) | (Utils::random() & 0x00000000ffffffffULL)),
		m_odometer(0),
		m_permanent(perm)
	{
		Utils::memoryLock(this,sizeof(SymmetricKey));
		Utils::copy<ZT_SYMMETRIC_KEY_SIZE>(const_cast<uint8_t *>(secret), key);
	}

	ZT_INLINE ~SymmetricKey() noexcept
	{
		Utils::burn(const_cast<uint8_t *>(secret),ZT_SYMMETRIC_KEY_SIZE);
		Utils::memoryUnlock(this,sizeof(SymmetricKey));
	}

	/**
	 * Check whether this symmetric key may be expiring soon
	 *
	 * @param now Current time
	 * @return True if re-keying should happen
	 */
	ZT_INLINE bool expiringSoon(const int64_t now) const noexcept
	{
		return (!m_permanent) && (((now - m_ts) >= (ZT_SYMMETRIC_KEY_TTL / 2)) || (m_odometer >= (ZT_SYMMETRIC_KEY_TTL_MESSAGES / 2)) );
	}

	/**
	 * Check whether this symmetric key is expired due to too much time or too many messages
	 *
	 * @param now Current time
	 * @return True if this symmetric key should no longer be used
	 */
	ZT_INLINE bool expired(const int64_t now) const noexcept
	{
		return (!m_permanent) && (((now - m_ts) >= ZT_SYMMETRIC_KEY_TTL) || (m_odometer >= ZT_SYMMETRIC_KEY_TTL_MESSAGES) );
	}

	/**
	 * Advance usage counter by one and return the next IV / packet ID.
	 *
	 * @param sender Sending ZeroTier address
	 * @param receiver Receiving ZeroTier address
	 * @return Next unique IV for next message
	 */
	ZT_INLINE uint64_t nextMessage(const Address sender,const Address receiver) noexcept
	{
		return (m_nonceBase + m_odometer++) ^ (((uint64_t)(sender > receiver)) << 63U);
	}

private:
	const int64_t m_ts;
	const uint64_t m_nonceBase;
	std::atomic<uint64_t> m_odometer;
	std::atomic<int> __refCount;
	const bool m_permanent;
};

} // namespace ZeroTier

#endif
