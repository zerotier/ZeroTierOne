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

#ifndef ZT_EXPECT_HPP
#define ZT_EXPECT_HPP

#include "Constants.hpp"
#include "Utils.hpp"

/**
 * Number of buckets to use to maintain a list of expected replies.
 *
 * More buckets means less chance of two packets tagging the same
 * bucket. This doesn't actually hurt anything since this class
 * behaves like a bloom filter: you can have false positives but
 * not false negatives.
 *
 * OKs are also cryptographically authenticated, so this is not a
 * huge problem, but this helps harden the system against replay
 * attacks for e.g. denial of service.
 */
#define ZT_EXPECT_BUCKETS 131072

/**
 * 1/2 the TTL for expected replies in milliseconds
 *
 * Making this a power of two improves efficiency a little by allowing bit
 * shift division.
 */
#define ZT_EXPECT_TTL 4096LL

namespace ZeroTier {

/**
 * Tracker for expected OK replies to packet IDs of sent packets
 */
class Expect
{
public:
	ZT_INLINE Expect() : _salt(Utils::getSecureRandomU64()) {} // NOLINT(cppcoreguidelines-pro-type-member-init,hicpp-member-init)

	/**
	 * Called by other code when something is sending a packet that may receive an OK response
	 *
	 * @param packetId Packet ID of packet being sent (be sure it's post-armor())
	 * @param now Current time
	 */
	ZT_INLINE void sending(const uint64_t packetId,const int64_t now) noexcept
	{
		_packetIdSent[Utils::hash64(packetId ^ _salt) % ZT_EXPECT_BUCKETS].store((int32_t)(now / ZT_EXPECT_TTL));
	}

	/**
	 * Check whether an OK is expected for this packet
	 *
	 * @param inRePacketId
	 * @param now
	 * @return
	 */
	ZT_INLINE bool expecting(const uint64_t inRePacketId,const int64_t now) const noexcept
	{
		return (((now / ZT_EXPECT_TTL) - (int64_t)_packetIdSent[Utils::hash64(inRePacketId ^ _salt) % ZT_EXPECT_BUCKETS].load()) <= 1);
	}

private:
	// This is a static per-runtime salt that's XORed and mixed with the packet ID
	// to make it difficult for a third party to predict expected-reply buckets.
	// Such prediction would not be catastrophic but it's easy and good to harden
	// against it.
	const uint64_t _salt;

	// Each bucket contains a timestamp in units of the expect duration.
	std::atomic<int32_t> _packetIdSent[ZT_EXPECT_BUCKETS];
};

} // namespace ZeroTier

#endif
