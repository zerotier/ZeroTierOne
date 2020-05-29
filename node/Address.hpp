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

#ifndef ZT_ADDRESS_HPP
#define ZT_ADDRESS_HPP

#include "Constants.hpp"
#include "Utils.hpp"
#include "TriviallyCopyable.hpp"
#include "Containers.hpp"

#define ZT_ADDRESS_STRING_SIZE_MAX (ZT_ADDRESS_LENGTH_HEX + 1)

namespace ZeroTier {

/**
 * A ZeroTier address
 *
 * This is merely a 40-bit short address packed into a uint64_t and wrapped with methods.
 */
class Address : public TriviallyCopyable
{
public:
	ZT_INLINE Address() noexcept : _a(0) {}
	explicit ZT_INLINE Address(const uint64_t a) noexcept : _a(a) {}
	explicit ZT_INLINE Address(const uint8_t b[5]) noexcept : _a(((uint64_t)b[0] << 32U) | ((uint64_t)b[1] << 24U) | ((uint64_t)b[2] << 16U) | ((uint64_t)b[3] << 8U) | (uint64_t)b[4]) {}

	ZT_INLINE Address &operator=(const uint64_t a) noexcept { _a = a; return *this; }

	/**
	 * @param bits Raw address -- 5 bytes, big-endian byte order
	 * @param len Length of array
	 */
	ZT_INLINE void setTo(const uint8_t b[5]) noexcept
	{
		_a = ((uint64_t)b[0] << 32U) | ((uint64_t)b[1] << 24U) | ((uint64_t)b[2] << 16U) | ((uint64_t)b[3] << 8U) | (uint64_t)b[4];
	}

	/**
	 * @param bits Buffer to hold 5-byte address in big-endian byte order
	 * @param len Length of array
	 */
	ZT_INLINE void copyTo(uint8_t b[5]) const noexcept
	{
		const uint64_t a = _a;
		b[0] = (uint8_t)(a >> 32U);
		b[1] = (uint8_t)(a >> 24U);
		b[2] = (uint8_t)(a >> 16U);
		b[3] = (uint8_t)(a >> 8U);
		b[4] = (uint8_t)a;
	}

	/**
	 * @return Integer containing address (0 to 2^40)
	 */
	ZT_INLINE uint64_t toInt() const noexcept { return _a; }

	/**
	 * Set address to zero/NIL
	 */
	ZT_INLINE void zero() noexcept { _a = 0; }

	/**
	 * @param s String with at least 11 characters of space available (10 + terminating NULL)
	 * @return Hexadecimal string
	 */
	ZT_INLINE char *toString(char s[ZT_ADDRESS_STRING_SIZE_MAX]) const noexcept
	{
		const uint64_t a = _a;
		const unsigned int m = 0xf;
		s[0] = Utils::HEXCHARS[(unsigned int)(a >> 36U) & m];
		s[1] = Utils::HEXCHARS[(unsigned int)(a >> 32U) & m];
		s[2] = Utils::HEXCHARS[(unsigned int)(a >> 28U) & m];
		s[3] = Utils::HEXCHARS[(unsigned int)(a >> 24U) & m];
		s[4] = Utils::HEXCHARS[(unsigned int)(a >> 20U) & m];
		s[5] = Utils::HEXCHARS[(unsigned int)(a >> 16U) & m];
		s[6] = Utils::HEXCHARS[(unsigned int)(a >> 12U) & m];
		s[7] = Utils::HEXCHARS[(unsigned int)(a >> 8U) & m];
		s[8] = Utils::HEXCHARS[(unsigned int)(a >> 4U) & m];
		s[9] = Utils::HEXCHARS[(unsigned int)a & m];
		s[10] = 0;
		return s;
	}
	ZT_INLINE String toString() const { char s[ZT_ADDRESS_STRING_SIZE_MAX]; toString(s); return String(s); }

	/**
	 * Check if this address is reserved
	 *
	 * The all-zero null address and any address beginning with 0xff are
	 * reserved. (0xff is reserved for future use to designate possibly
	 * longer addresses, addresses based on IPv6 innards, etc.)
	 *
	 * @return True if address is reserved and may not be used
	 */
	ZT_INLINE bool isReserved() const noexcept { return ((!_a) || ((_a >> 32U) == ZT_ADDRESS_RESERVED_PREFIX)); }

	ZT_INLINE unsigned long hashCode() const noexcept { return (unsigned long)_a; }

	ZT_INLINE operator bool() const noexcept { return (_a != 0); }
	ZT_INLINE operator uint64_t() const noexcept { return _a; }

	ZT_INLINE bool operator==(const Address &a) const noexcept { return _a == a._a; }
	ZT_INLINE bool operator!=(const Address &a) const noexcept { return _a != a._a; }
	ZT_INLINE bool operator>(const Address &a) const noexcept { return _a > a._a; }
	ZT_INLINE bool operator<(const Address &a) const noexcept { return _a < a._a; }
	ZT_INLINE bool operator>=(const Address &a) const noexcept { return _a >= a._a; }
	ZT_INLINE bool operator<=(const Address &a) const noexcept { return _a <= a._a; }

	ZT_INLINE bool operator==(const uint64_t a) const noexcept { return _a == a; }
	ZT_INLINE bool operator!=(const uint64_t a) const noexcept { return _a != a; }
	ZT_INLINE bool operator>(const uint64_t a) const noexcept { return _a > a; }
	ZT_INLINE bool operator<(const uint64_t a) const noexcept { return _a < a; }
	ZT_INLINE bool operator>=(const uint64_t a) const noexcept { return _a >= a; }
	ZT_INLINE bool operator<=(const uint64_t a) const noexcept { return _a <= a; }

private:
	uint64_t _a;
};

} // namespace ZeroTier

#endif
