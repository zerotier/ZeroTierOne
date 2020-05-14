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

#ifndef ZT_FINGERPRINT_HPP
#define ZT_FINGERPRINT_HPP

#include "Constants.hpp"
#include "TriviallyCopyable.hpp"
#include "Address.hpp"
#include "Utils.hpp"

#include <algorithm>

#define ZT_FINGERPRINT_STRING_BUFFER_LENGTH 96

namespace ZeroTier {

class Identity;

/**
 * Address and full hash of an identity's public keys.
 *
 * This is the same size as ZT_Fingerprint and should be cast-able back and forth.
 * This is checked in Tests.cpp.
 */
class Fingerprint : public TriviallyCopyable
{
	friend class Identity;

public:
	/**
	 * Create an empty/nil fingerprint
	 */
	ZT_INLINE Fingerprint() noexcept { memoryZero(this); }

	ZT_INLINE Address address() const noexcept { return Address(m_cfp.address); }
	ZT_INLINE const uint8_t *hash() const noexcept { return m_cfp.hash; }
	ZT_INLINE ZT_Fingerprint *apiFingerprint() noexcept { return &m_cfp; }
	ZT_INLINE const ZT_Fingerprint *apiFingerprint() const noexcept { return &m_cfp; }

	/**
	 * @return True if hash is not all zero (missing/unspecified)
	 */
	ZT_INLINE bool haveHash() const noexcept { return (!Utils::allZero(m_cfp.hash, sizeof(m_cfp.hash))); }

	/**
	 * Get a base32-encoded representation of this fingerprint
	 *
	 * @param s Base32 string
	 */
	ZT_INLINE void toString(char s[ZT_FINGERPRINT_STRING_BUFFER_LENGTH]) const noexcept
	{
		uint8_t tmp[48 + 5];
		address().copyTo(tmp);
		Utils::copy<48>(tmp + 5, m_cfp.hash);
		Utils::b32e(tmp,sizeof(tmp),s,ZT_FINGERPRINT_STRING_BUFFER_LENGTH);
		s[ZT_FINGERPRINT_STRING_BUFFER_LENGTH-1] = 0; // sanity check, ensure always zero terminated
	}

	/**
	 * Set this fingerprint to a base32-encoded string
	 *
	 * @param s String to decode
	 * @return True if string appears to be valid and of the proper length (no other checking is done)
	 */
	ZT_INLINE bool fromString(const char *s) noexcept
	{
		uint8_t tmp[48 + 5];
		if (Utils::b32d(s,tmp,sizeof(tmp)) != sizeof(tmp))
			return false;
		m_cfp.address = Address(tmp).toInt();
		Utils::copy<48>(m_cfp.hash, tmp + 5);
		return true;
	}

	ZT_INLINE void zero() noexcept { memoryZero(this); }
	ZT_INLINE unsigned long hashCode() const noexcept { return (unsigned long)m_cfp.address; }

	ZT_INLINE operator bool() const noexcept { return (m_cfp.address != 0); }

	ZT_INLINE bool operator==(const Fingerprint &h) const noexcept { return ((m_cfp.address == h.m_cfp.address) && (memcmp(m_cfp.hash, h.m_cfp.hash, ZT_FINGERPRINT_HASH_SIZE) == 0)); }
	ZT_INLINE bool operator!=(const Fingerprint &h) const noexcept { return !(*this == h); }
	ZT_INLINE bool operator<(const Fingerprint &h) const noexcept { return ((m_cfp.address < h.m_cfp.address) || ((m_cfp.address == h.m_cfp.address) && (memcmp(m_cfp.hash, h.m_cfp.hash, ZT_FINGERPRINT_HASH_SIZE) < 0))); }
	ZT_INLINE bool operator>(const Fingerprint &h) const noexcept { return (h < *this); }
	ZT_INLINE bool operator<=(const Fingerprint &h) const noexcept { return !(h < *this); }
	ZT_INLINE bool operator>=(const Fingerprint &h) const noexcept { return !(*this < h); }

private:
	ZT_Fingerprint m_cfp;
};

} // namespace ZeroTier

#endif
