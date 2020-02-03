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
#include "Identity.hpp"

namespace ZeroTier {

/**
 * A short address and a longer identity hash for extra confirmation of a node's identity.
 */
struct Fingerprint
{
	ZT_ALWAYS_INLINE Fingerprint() : address() { memset(hash,0,ZT_IDENTITY_HASH_SIZE); }
	explicit ZT_ALWAYS_INLINE Fingerprint(const Identity &id) : address(id.address()) { memcpy(hash,id.hash(),ZT_IDENTITY_HASH_SIZE); }

	ZT_ALWAYS_INLINE Fingerprint &operator=(const Identity &id)
	{
		address = id.address();
		memcpy(hash,id.hash(),ZT_IDENTITY_HASH_SIZE);
		return *this;
	}

	ZT_ALWAYS_INLINE bool operator==(const Fingerprint &fp) const { return ((address == fp.address)&&(memcmp(hash,fp.hash,ZT_IDENTITY_HASH_SIZE) == 0)); }
	ZT_ALWAYS_INLINE bool operator!=(const Fingerprint &fp) const { return ((address != fp.address)||(memcmp(hash,fp.hash,ZT_IDENTITY_HASH_SIZE) != 0)); }
	ZT_ALWAYS_INLINE bool operator<(const Fingerprint &fp) const { return ((address < fp.address)||((address == fp.address)&&(memcmp(hash,fp.hash,ZT_IDENTITY_HASH_SIZE) < 0))); }
	ZT_ALWAYS_INLINE bool operator>(const Fingerprint &fp) const { return (fp < *this); }
	ZT_ALWAYS_INLINE bool operator<=(const Fingerprint &fp) const { return !(fp < *this); }
	ZT_ALWAYS_INLINE bool operator>=(const Fingerprint &fp) const { return !(*this < fp); }

	ZT_ALWAYS_INLINE bool operator==(const Identity &id) const { return ((address == id.address())&&(memcmp(hash,id.hash(),ZT_IDENTITY_HASH_SIZE) == 0)); }
	ZT_ALWAYS_INLINE bool operator!=(const Identity &id) const { return ((address != id.address())||(memcmp(hash,id.hash(),ZT_IDENTITY_HASH_SIZE) != 0)); }
	ZT_ALWAYS_INLINE bool operator<(const Identity &id) const { return ((address < id.address())||((address == id.address())&&(memcmp(hash,id.hash(),ZT_IDENTITY_HASH_SIZE) < 0))); }
	ZT_ALWAYS_INLINE bool operator>(const Identity &id) const { return (Fingerprint(id) < *this); }
	ZT_ALWAYS_INLINE bool operator<=(const Identity &id) const { return !(Fingerprint(id) < *this); }
	ZT_ALWAYS_INLINE bool operator>=(const Identity &id) const { return !(*this < id); }

	ZT_ALWAYS_INLINE operator bool() const { return (address); }

	/**
	 * Short ZeroTier address
	 */
	Address address;

	/**
	 * SHA-384 hash of public portions of identity key(s)
	 */
	uint8_t hash[ZT_IDENTITY_HASH_SIZE];
};

} // namespace ZeroTier

#endif
