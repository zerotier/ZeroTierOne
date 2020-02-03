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

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cmath>

#include <string>
#include <vector>
#include <algorithm>

#include "Constants.hpp"
#include "Utils.hpp"
#include "TriviallyCopyable.hpp"

namespace ZeroTier {

/**
 * A ZeroTier address
 */
class Address : public TriviallyCopyable
{
public:
	ZT_ALWAYS_INLINE Address() : _a(0) {}
	explicit ZT_ALWAYS_INLINE Address(const uint8_t b[5]) : _a(((uint64_t)b[0] << 32U) | ((uint64_t)b[1] << 24U) | ((uint64_t)b[2] << 16U) | ((uint64_t)b[3] << 8U) | (uint64_t)b[4]) {}
	explicit ZT_ALWAYS_INLINE Address(const uint64_t a) : _a(a & 0xffffffffffULL) {}

	ZT_ALWAYS_INLINE Address &operator=(const uint64_t a) { _a = (a & 0xffffffffffULL); return *this; }

	/**
	 * @param bits Raw address -- 5 bytes, big-endian byte order
	 * @param len Length of array
	 */
	ZT_ALWAYS_INLINE void setTo(const uint8_t b[5])
	{
		_a = ((uint64_t)b[0] << 32U) | ((uint64_t)b[1] << 24U) | ((uint64_t)b[2] << 16U) | ((uint64_t)b[3] << 8U) | (uint64_t)b[4];
	}

	/**
	 * @param bits Buffer to hold 5-byte address in big-endian byte order
	 * @param len Length of array
	 */
	ZT_ALWAYS_INLINE void copyTo(uint8_t b[5]) const
	{
		b[0] = (uint8_t)(_a >> 32U);
		b[1] = (uint8_t)(_a >> 24U);
		b[2] = (uint8_t)(_a >> 16U);
		b[3] = (uint8_t)(_a >> 8U);
		b[4] = (uint8_t)_a;
	}

	/**
	 * @return Integer containing address (0 to 2^40)
	 */
	ZT_ALWAYS_INLINE uint64_t toInt() const { return _a; }

	/**
	 * @return Hash code for use with Hashtable
	 */
	ZT_ALWAYS_INLINE unsigned long hashCode() const { return (unsigned long)_a; }

	/**
	 * @return Hexadecimal string
	 */
	ZT_ALWAYS_INLINE char *toString(char buf[11]) const { return Utils::hex10(_a,buf); }

	/**
	 * Check if this address is reserved
	 *
	 * The all-zero null address and any address beginning with 0xff are
	 * reserved. (0xff is reserved for future use to designate possibly
	 * longer addresses, addresses based on IPv6 innards, etc.)
	 *
	 * @return True if address is reserved and may not be used
	 */
	ZT_ALWAYS_INLINE bool isReserved() const { return ((!_a)||((_a >> 32U) == ZT_ADDRESS_RESERVED_PREFIX)); }

	/**
	 * @param i Value from 0 to 4 (inclusive)
	 * @return Byte at said position (address interpreted in big-endian order)
	 */
	ZT_ALWAYS_INLINE uint8_t operator[](unsigned int i) const { return (uint8_t)(_a >> (32 - (i * 8))); }

	ZT_ALWAYS_INLINE operator bool() const { return (_a != 0); }

	ZT_ALWAYS_INLINE void zero() { _a = 0; }

	ZT_ALWAYS_INLINE bool operator==(const uint64_t &a) const { return (_a == (a & 0xffffffffffULL)); }
	ZT_ALWAYS_INLINE bool operator!=(const uint64_t &a) const { return (_a != (a & 0xffffffffffULL)); }
	ZT_ALWAYS_INLINE bool operator>(const uint64_t &a) const { return (_a > (a & 0xffffffffffULL)); }
	ZT_ALWAYS_INLINE bool operator<(const uint64_t &a) const { return (_a < (a & 0xffffffffffULL)); }
	ZT_ALWAYS_INLINE bool operator>=(const uint64_t &a) const { return (_a >= (a & 0xffffffffffULL)); }
	ZT_ALWAYS_INLINE bool operator<=(const uint64_t &a) const { return (_a <= (a & 0xffffffffffULL)); }

	ZT_ALWAYS_INLINE bool operator==(const Address &a) const { return (_a == a._a); }
	ZT_ALWAYS_INLINE bool operator!=(const Address &a) const { return (_a != a._a); }
	ZT_ALWAYS_INLINE bool operator>(const Address &a) const { return (_a > a._a); }
	ZT_ALWAYS_INLINE bool operator<(const Address &a) const { return (_a < a._a); }
	ZT_ALWAYS_INLINE bool operator>=(const Address &a) const { return (_a >= a._a); }
	ZT_ALWAYS_INLINE bool operator<=(const Address &a) const { return (_a <= a._a); }

#if 0
	/**
	 * Create a list of the first N bits of a list of unique addresses with N as the minimum unique size
	 *
	 * The list is stored in a space-efficient packed bit format.
	 *
	 * @param start Starting Address iterator/pointer
	 * @param end Ending Address iterator/pointer
	 * @param list Pointer to location to write list
	 * @param listCapacityBytes Number of bytes available for list
	 * @return Number of bytes written or -1 on overflow or other error
	 * @tparam I Input iterator type
	 */
	template<typename I>
	static inline int createMinPrefixList(I start,I end,uint8_t *list,const int listCapacityBytes)
	{
		std::vector<Address> sortedAddrs(start,end);
		if (sortedAddrs.empty())
			return 0;
		if (listCapacityBytes == 0)
			return -1;
		std::sort(sortedAddrs.begin(),sortedAddrs.end());

		unsigned int bits = (unsigned int)fmaxf(log2f((float)(sortedAddrs.size() * 2)),3.0F);
		uint64_t mask;
try_additional_bits: {
			mask = 0xffffffffffffffffULL >> (64 - bits);
			std::vector<Address>::iterator a(sortedAddrs.begin());
			uint64_t aa = *(a++) & mask;
			aa |= (uint64_t)(aa == 0);
			uint64_t lastMaskedAddress = aa;
			while (a != sortedAddrs.end()) {
				aa = *(a++) & mask;
				aa |= (uint64_t)(aa == 0);
				if (aa == lastMaskedAddress) {
					++bits;
					goto try_additional_bits;
				}
				lastMaskedAddress = aa;
			}
		}

		int l = 0;
		unsigned int bitPtr = 0;
		for(I a(start);a!=end;) {
			uint64_t aa = *(a++) & mask;
			aa |= (uint64_t)(aa == 0);
			unsigned int br = bits;
			if (bitPtr > 0) {
				unsigned int w = 8 - bitPtr;
				if (w > br) w = br;
				list[l] = (list[l] << w) | (((uint8_t)aa) & (0xff >> (8 - w)));
				bitPtr += w;
				if (bitPtr == 8) {
					bitPtr = 0;
					if (l >= listCapacityBytes)
						return -1;
					++l;
				}
				aa >>= w;
				br -= w;
			}
			while (br >= 8) {
				if (l >= listCapacityBytes)
					return -1;
				list[l++] = (uint8_t)aa;
				br -= 8;
				aa >>= 8;
			}
			if (br > 0) {
				list[l] = (uint8_t)aa;
				bitPtr = br;
			}
		}
		if (bitPtr > 0) {
			if (l >= listCapacityBytes)
				return -1;
			++l;
		}

		return l;
	}
#endif

private:
	uint64_t _a;
};

} // namespace ZeroTier

#endif
