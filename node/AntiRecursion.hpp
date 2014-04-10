/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2011-2014  ZeroTier Networks LLC
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

#define ZT_ANTIRECURSION_TAIL_LEN 256

/**
 * Filter to prevent recursion (ZeroTier-over-ZeroTier)
 *
 * This works by logging ZeroTier packets that we send. It's then invoked
 * again against packets read from local Ethernet taps. If the last N
 * bytes representing the ZeroTier packet match in the tap frame, then
 * the frame is a re-injection of a frame that we sent and is rejected.
 *
 * This means that ZeroTier packets simply will not traverse ZeroTier
 * networks, which would cause all sorts of weird problems.
 *
 * NOTE: this is applied to low-level packets before they are sent to
 * SocketManager and/or sockets, not to fully assembled packets before
 * (possible) fragmentation.
 */
class AntiRecursion
{
public:
	AntiRecursion()
		throw()
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
	inline void logOutgoingZT(const void *data,unsigned int len)
		throw()
	{
		ArItem *i = &(_history[_ptr++ % ZT_ANTIRECURSION_HISTORY_SIZE]);
		const unsigned int tl = (len > ZT_ANTIRECURSION_TAIL_LEN) ? ZT_ANTIRECURSION_TAIL_LEN : len;
		memcpy(i->tail,((const unsigned char *)data) + (len - tl),tl);
		i->len = tl;
	}

	/**
	 * Check an ethernet frame from a local tap against anti-recursion history
	 *
	 * @param data Raw frame data
	 * @param len Length of frame
	 * @return True if frame is OK to be passed, false if it's a ZT frame that we sent
	 */
	inline bool checkEthernetFrame(const void *data,unsigned int len)
		throw()
	{
		for(unsigned int h=0;h<ZT_ANTIRECURSION_HISTORY_SIZE;++h) {
			ArItem *i = &(_history[h]);
			if ((len >= i->len)&&(!memcmp(((const unsigned char *)data) + (len - i->len),i->tail,i->len)))
				return false;
		}
		return true;
	}

private:
	struct ArItem
	{
		unsigned char tail[ZT_ANTIRECURSION_TAIL_LEN];
		unsigned int len;
	};
	ArItem _history[ZT_ANTIRECURSION_HISTORY_SIZE];
	volatile unsigned int _ptr;
};

} // namespace ZeroTier

#endif
