/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2015  ZeroTier, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

#ifndef ZT_ANTIRECURSION_HPP
#define ZT_ANTIRECURSION_HPP

#include <string.h>
#include <stdlib.h>

#include "Constants.hpp"

namespace ZeroTier {

/**
 * Size of anti-recursion history
 */
#define ZT_ANTIRECURSION_HISTORY_SIZE 16

/**
 * Filter to prevent recursion (ZeroTier-over-ZeroTier)
 *
 * This works by logging ZeroTier packets that we send. It's then invoked
 * again against packets read from local Ethernet taps. If the last 32
 * bytes representing the ZeroTier packet match in the tap frame, then
 * the frame is a re-injection of a frame that we sent and is rejected.
 *
 * This means that ZeroTier packets simply will not traverse ZeroTier
 * networks, which would cause all sorts of weird problems.
 *
 * This is highly optimized code since it's checked for every packet.
 */
class AntiRecursion
{
public:
	AntiRecursion()
	{
		memset(_history,0,sizeof(_history));
		_ptr = 0;
	}

	/**
	 * Add an outgoing ZeroTier packet to the circular log
	 *
	 * @param data ZT packet data
	 * @param len Length of packet
	 */
	inline void logOutgoingZT(const void *const data,const unsigned int len)
	{
		if (len < 32)
			return;
#ifdef ZT_NO_TYPE_PUNNING
		memcpy(_history[++_ptr % ZT_ANTIRECURSION_HISTORY_SIZE].tail,reinterpret_cast<const uint8_t *>(data) + (len - 32),32);
#else
		uint64_t *t = _history[++_ptr % ZT_ANTIRECURSION_HISTORY_SIZE].tail;
		const uint64_t *p = reinterpret_cast<const uint64_t *>(reinterpret_cast<const uint8_t *>(data) + (len - 32));
		*(t++) = *(p++);
		*(t++) = *(p++);
		*(t++) = *(p++);
		*t = *p;
#endif
	}

	/**
	 * Check an ethernet frame from a local tap against anti-recursion history
	 *
	 * @param data Raw frame data
	 * @param len Length of frame
	 * @return True if frame is OK to be passed, false if it's a ZT frame that we sent
	 */
	inline bool checkEthernetFrame(const void *const data,const unsigned int len) const
	{
		if (len < 32)
			return true;
		const uint8_t *const pp = reinterpret_cast<const uint8_t *>(data) + (len - 32);
		const _ArItem *i = _history;
		const _ArItem *const end = i + ZT_ANTIRECURSION_HISTORY_SIZE;
		while (i != end) {
#ifdef ZT_NO_TYPE_PUNNING
			if (!memcmp(pp,i->tail,32))
				return false;
#else
			const uint64_t *t = i->tail;
			const uint64_t *p = reinterpret_cast<const uint64_t *>(pp);
			uint64_t bits = *(t++) ^ *(p++);
			bits |= *(t++) ^ *(p++);
			bits |= *(t++) ^ *(p++);
			bits |= *t ^ *p;
			if (!bits)
				return false;
#endif
			++i;
		}
		return true;
	}

private:
	struct _ArItem { uint64_t tail[4]; };
	_ArItem _history[ZT_ANTIRECURSION_HISTORY_SIZE];
	volatile unsigned long _ptr;
};

} // namespace ZeroTier

#endif
