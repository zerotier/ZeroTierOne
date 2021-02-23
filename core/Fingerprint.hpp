/*
 * Copyright (c)2013-2021 ZeroTier, Inc.
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

#ifndef ZT_FINGERPRINT_HPP
#define ZT_FINGERPRINT_HPP

#include "Constants.hpp"
#include "TriviallyCopyable.hpp"
#include "Address.hpp"
#include "Utils.hpp"

#define ZT_FINGERPRINT_STRING_SIZE_MAX 128
#define ZT_FINGERPRINT_MARSHAL_SIZE 53

namespace ZeroTier {

/**
 * Address and full hash of an identity's public keys.
 *
 * This is the same size as ZT_Fingerprint and should be cast-able back and forth.
 * This is checked in Tests.cpp.
 */
class Fingerprint : public ZT_Fingerprint, public TriviallyCopyable
{
public:
	ZT_INLINE Fingerprint() noexcept
	{ memoryZero(this); }

	ZT_INLINE Fingerprint(const ZT_Fingerprint &fp) noexcept
	{ Utils::copy< sizeof(ZT_Fingerprint) >(this, &fp); }

	/**
	 * @return True if hash is not all zero (missing/unspecified)
	 */
	ZT_INLINE bool haveHash() const noexcept
	{ return (!Utils::allZero(this->hash, ZT_FINGERPRINT_HASH_SIZE)); }

	/**
	 * Get a base32-encoded representation of this fingerprint
	 *
	 * @param s Base32 string
	 */
	ZT_INLINE char *toString(char s[ZT_FINGERPRINT_STRING_SIZE_MAX]) const noexcept
	{
		Address(this->address).toString(s);
		if (haveHash()) {
			s[ZT_ADDRESS_LENGTH_HEX] = '-';
			Utils::b32e(this->hash, ZT_FINGERPRINT_HASH_SIZE, s + (ZT_ADDRESS_LENGTH_HEX + 1), ZT_FINGERPRINT_STRING_SIZE_MAX - (ZT_ADDRESS_LENGTH_HEX + 1));
		}
		return s;
	}

	ZT_INLINE String toString() const
	{
		char tmp[ZT_FINGERPRINT_STRING_SIZE_MAX];
		return String(toString(tmp));
	}

	/**
	 * Set this fingerprint to a base32-encoded string
	 *
	 * @param s String to decode
	 * @return True if string appears to be valid and of the proper length (no other checking is done)
	 */
	ZT_INLINE bool fromString(const char *const s) noexcept
	{
		if (!s)
			return false;
		const int l = (int)strlen(s);
		if (l < ZT_ADDRESS_LENGTH_HEX)
			return false;
		char a[ZT_ADDRESS_LENGTH_HEX + 1];
		Utils::copy< ZT_ADDRESS_LENGTH_HEX >(a, s);
		a[ZT_ADDRESS_LENGTH_HEX] = 0;
		this->address = Utils::hexStrToU64(a) & ZT_ADDRESS_MASK;
		if (l > (ZT_ADDRESS_LENGTH_HEX + 1)) {
			if (Utils::b32d(s + (ZT_ADDRESS_LENGTH_HEX + 1), this->hash, ZT_FINGERPRINT_HASH_SIZE) != ZT_FINGERPRINT_HASH_SIZE)
				return false;
		} else {
			Utils::zero< ZT_FINGERPRINT_HASH_SIZE >(this->hash);
		}
		return true;
	}

	/**
	 * Check for equality with best possible specificity.
	 * 
	 * If both fingerprints have a hash, that is compared. Otherwise just the
	 * addresses are compared.
	 * 
	 * @param fp Fingerprint to test
	 */
	ZT_INLINE bool bestSpecificityEquals(const ZT_Fingerprint &fp) const noexcept
	{
		if (address == fp.address) {
			if (Utils::allZero(fp.hash, ZT_FINGERPRINT_HASH_SIZE) || Utils::allZero(hash, ZT_FINGERPRINT_HASH_SIZE))
				return true;
			return (memcmp(hash, fp.hash, ZT_FINGERPRINT_HASH_SIZE) == 0);
		}
		return false;
	}

	ZT_INLINE void zero() noexcept
	{ memoryZero(this); }

	ZT_INLINE unsigned long hashCode() const noexcept
	{ return (unsigned long)this->address; }

	ZT_INLINE operator bool() const noexcept
	{ return this->address != 0; }

	static constexpr int marshalSizeMax() noexcept
	{ return ZT_FINGERPRINT_MARSHAL_SIZE; }

	ZT_INLINE int marshal(uint8_t data[ZT_FINGERPRINT_MARSHAL_SIZE]) const noexcept
	{
		Address(this->address).copyTo(data);
		Utils::copy< ZT_FINGERPRINT_HASH_SIZE >(data + ZT_ADDRESS_LENGTH, this->hash);
		return ZT_FINGERPRINT_MARSHAL_SIZE;
	}

	ZT_INLINE int unmarshal(const uint8_t *const data, int len) noexcept
	{
		if (unlikely(len < ZT_FINGERPRINT_MARSHAL_SIZE))
			return -1;
		this->address = Address(data);
		Utils::copy< ZT_FINGERPRINT_HASH_SIZE >(hash, data + ZT_ADDRESS_LENGTH);
		return ZT_FINGERPRINT_MARSHAL_SIZE;
	}

	ZT_INLINE bool operator==(const ZT_Fingerprint &h) const noexcept
	{ return ((this->address == h.address) && (memcmp(this->hash, h.hash, ZT_FINGERPRINT_HASH_SIZE) == 0)); }

	ZT_INLINE bool operator!=(const ZT_Fingerprint &h) const noexcept
	{ return !(*this == h); }

	ZT_INLINE bool operator<(const ZT_Fingerprint &h) const noexcept
	{ return ((this->address < h.address) || ((this->address == h.address) && (memcmp(this->hash, h.hash, ZT_FINGERPRINT_HASH_SIZE) < 0))); }

	ZT_INLINE bool operator>(const ZT_Fingerprint &h) const noexcept
	{ return (*reinterpret_cast<const Fingerprint *>(&h) < *this); }

	ZT_INLINE bool operator<=(const ZT_Fingerprint &h) const noexcept
	{ return !(*reinterpret_cast<const Fingerprint *>(&h) < *this); }

	ZT_INLINE bool operator>=(const ZT_Fingerprint &h) const noexcept
	{ return !(*this < h); }
};

static_assert(sizeof(Fingerprint) == sizeof(ZT_Fingerprint), "size mismatch");

} // namespace ZeroTier

#endif
