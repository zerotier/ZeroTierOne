/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2012-2013  ZeroTier Networks LLC
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

#ifndef _ZT_RATELIMITER_HPP
#define _ZT_RATELIMITER_HPP

#include <math.h>
#include "Utils.hpp"

namespace ZeroTier {

/**
 * Burstable rate limiter
 *
 * This limits a transfer rate to a maximum bytes per second using an
 * accounting method based on a balance rather than accumulating an
 * average rate. The result is a burstable rate limit rather than a
 * continuous rate limit; the link being limited may use all its balance
 * at once or slowly over time. Balance constantly replenishes over time
 * up to a configurable maximum balance.
 */
class RateLimiter
{
public:
	/**
	 * Create an uninitialized rate limiter
	 *
	 * init() must be called before this is used.
	 */
	RateLimiter() throw() {}

	/**
	 * @param bytesPerSecond Bytes per second to permit (average)
	 * @param preload Initial balance to place in account
	 * @param max Maximum balance to permit to ever accrue (max burst)
	 */
	RateLimiter(double bytesPerSecond,double preload,double max)
		throw()
	{
		init(bytesPerSecond,preload,max);
	}

	/**
	 * Initialize or re-initialize rate limiter
	 *
	 * @param bytesPerSecond Bytes per second to permit (average)
	 * @param preload Initial balance to place in account
	 * @param max Maximum balance to permit to ever accrue (max burst)
	 */
	inline void init(double bytesPerSecond,double preload,double max)
		throw()
	{
		_bytesPerSecond = bytesPerSecond;
		_lastTime = Utils::nowf();
		_balance = preload;
		_max = max;
	}

	/**
	 * Update balance based on current clock
	 *
	 * This can be called at any time to check the current balance without
	 * affecting the behavior of gate().
	 *
	 * @return New balance
	 */
	inline double updateBalance()
		throw()
	{
		double now = Utils::nowf();
		double b = _balance = fmin(_max,_balance + (_bytesPerSecond * (now - _lastTime)));
		_lastTime = now;
		return b;
	}

	/**
	 * Test balance and update / deduct if there is enough to transfer 'bytes'
	 *
	 * @param bytes Number of bytes that we wish to transfer
	 * @return True if balance was sufficient (balance is updated), false if not (balance unchanged)
	 */
	inline bool gate(double bytes)
		throw()
	{
		if (updateBalance() >= bytes) {
			_balance -= bytes;
			return true;
		}
		return false;
	}

private:
	double _bytesPerSecond;
	double _lastTime;
	double _balance;
	double _max;
};

} // namespace ZeroTier

#endif
