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
 * Container for 384-bit identity hashes
 *
 * The size of the hash used with this container must be a multiple of 64 bits.
 * Currently it's used as H<384> and H<512>.
 *
 * Warning: the [] operator is not bounds checked.
 *
 * @tparam BITS Bits in hash, must be a multiple of 64
 */
class Fingerprint : public TriviallyCopyable
{
	friend class Identity;

public:
	/**
	 * Create an empty/nil fingerprint
	 */
	ZT_ALWAYS_INLINE Fingerprint() noexcept { memoryZero(this); }

	ZT_ALWAYS_INLINE Address address() const noexcept { return Address(_fp.address); }
	ZT_ALWAYS_INLINE const uint8_t *hash() const noexcept { return _fp.hash; }

	/**
	 * Copy into ZT_Fingerprint struct as used in API and trace messages
	 *
	 * @param fp ZT_Fingerprint
	 */
	ZT_ALWAYS_INLINE void getAPIFingerprint(ZT_Fingerprint *fp) const noexcept { memcpy(fp,&_fp,sizeof(ZT_Fingerprint)); }

	/**
	 * @return Pointer to ZT_Fingerprint for API use
	 */
	ZT_ALWAYS_INLINE const ZT_Fingerprint *apiFingerprint() const noexcept { return &_fp; }

	/**
	 * Get a base32-encoded representation of this fingerprint
	 *
	 * @param s Base32 string
	 */
	ZT_ALWAYS_INLINE void toString(char s[ZT_FINGERPRINT_STRING_BUFFER_LENGTH])
	{
		uint8_t tmp[48 + 5];
		address().copyTo(tmp);
		memcpy(tmp + 5,_fp.hash,48);
		Utils::b32e(tmp,sizeof(tmp),s,ZT_FINGERPRINT_STRING_BUFFER_LENGTH);
		s[ZT_FINGERPRINT_STRING_BUFFER_LENGTH-1] = 0; // sanity check, ensure always zero terminated
	}

	/**
	 * Set this fingerprint to a base32-encoded string
	 *
	 * @param s String to decode
	 * @return True if string appears to be valid and of the proper length (no other checking is done)
	 */
	ZT_ALWAYS_INLINE bool fromString(const char *s)
	{
		uint8_t tmp[48 + 5];
		if (Utils::b32d(s,tmp,sizeof(tmp)) != sizeof(tmp))
			return false;
		_fp.address = Address(tmp).toInt();
		memcpy(_fp.hash,tmp + 5,48);
		return true;
	}

	ZT_ALWAYS_INLINE void zero() noexcept { memoryZero(this); }
	ZT_ALWAYS_INLINE unsigned long hashCode() const noexcept { return _fp.address; }

	ZT_ALWAYS_INLINE operator bool() const noexcept { return (_fp.address != 0); }

	ZT_ALWAYS_INLINE bool operator==(const Fingerprint &h) const noexcept { return ((_fp.address == h._fp.address)&&(memcmp(_fp.hash,h._fp.hash,ZT_IDENTITY_HASH_SIZE) == 0)); }
	ZT_ALWAYS_INLINE bool operator!=(const Fingerprint &h) const noexcept { return !(*this == h); }
	ZT_ALWAYS_INLINE bool operator<(const Fingerprint &h) const noexcept { return ((_fp.address < h._fp.address) || ((_fp.address == h._fp.address)&&(memcmp(_fp.hash,h._fp.hash,ZT_IDENTITY_HASH_SIZE) < 0))); }
	ZT_ALWAYS_INLINE bool operator>(const Fingerprint &h) const noexcept { return (h < *this); }
	ZT_ALWAYS_INLINE bool operator<=(const Fingerprint &h) const noexcept { return !(h < *this); }
	ZT_ALWAYS_INLINE bool operator>=(const Fingerprint &h) const noexcept { return !(*this < h); }

private:
	ZT_Fingerprint _fp;
};

} // namespace ZeroTier

#endif
