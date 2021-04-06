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

#ifndef ZT_EXPECT_HPP
#define ZT_EXPECT_HPP

#include "Constants.hpp"
#include "Utils.hpp"

/**
 * Number of buckets to use to maintain a list of expected replies.
 *
 * This must be a power of two. Memory consumed will be about this*4 bytes.
 */
#define ZT_EXPECT_BUCKETS 32768

/**
 * 1/2 the TTL for expected replies in milliseconds
 *
 * This must be a power of two.
 */
#define ZT_EXPECT_TTL 4096LL

namespace ZeroTier {

/**
 * Tracker for expected OK replies to packet IDs of sent packets
 */
class Expect
{
public:
	ZT_INLINE Expect() :
		m_packetIdSent()
	{}

	/**
	 * Called by other code when something is sending a packet that could potentially receive an OK response
	 *
	 * @param packetId Packet ID of packet being sent (be sure it's post-armor())
	 * @param now Current time
	 */
	ZT_INLINE void sending(const uint64_t packetId, const int64_t now) noexcept
	{ m_packetIdSent[Utils::hash64(packetId ^ Utils::s_mapNonce) % ZT_EXPECT_BUCKETS] = (uint32_t)(now / ZT_EXPECT_TTL); }

	/**
	 * Check if an OK is expected and if so reset the corresponding bucket.
	 *
	 * This means this call mutates the state. If it returns true, it will
	 * subsequently return false. This is to filter OKs against replays or
	 * responses to queries we did not send.
	 *
	 * @param inRePacketId In-re packet ID we're expecting
	 * @param now Current time
	 * @return True if we're expecting a reply (and a reset occurred)
	 */
	ZT_INLINE bool expecting(const uint64_t inRePacketId, const int64_t now) noexcept
	{ return (((now / ZT_EXPECT_TTL) - (int64_t)m_packetIdSent[(unsigned long)Utils::hash64(inRePacketId ^ Utils::s_mapNonce) % ZT_EXPECT_BUCKETS].exchange(0)) <= 1); }

private:
	// Each bucket contains a timestamp in units of the max expect duration.
	std::atomic< uint32_t > m_packetIdSent[ZT_EXPECT_BUCKETS];
};

} // namespace ZeroTier

#endif
