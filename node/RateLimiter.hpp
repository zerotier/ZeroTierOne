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

#include "Constants.hpp"
#include "Utils.hpp"

#ifdef __WINDOWS__
#define fmin(a,b) (((a) <= (b)) ? (a) : (b))
#define fmax(a,b) (((a) >= (b)) ? (a) : (b))
#endif

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
	 * Limits to apply to a rate limiter
	 *
	 * Since many rate limiters may share the same fixed limit values,
	 * save memory by breaking this out into a struct parameter that
	 * can be passed into RateLimiter's methods.
	 */
	struct Limit
	{
		/**
		 * Speed in bytes per second, or rate of balance accrual
		 */
		double bytesPerSecond;

		/**
		 * Maximum balance that can ever be accrued (should be > 0.0)
		 */
		double maxBalance;

		/**
		 * Minimum balance, or maximum allowable "debt" (should be <= 0.0)
		 */
		double minBalance;
	};

	/**
	 * Create an uninitialized rate limiter
	 *
	 * init() must be called before this is used.
	 */
	RateLimiter() throw() {}

	/**
	 * @param preload Initial balance to place in account
	 */
	RateLimiter(double preload)
		throw()
	{
		init(preload);
	}

	/**
	 * Initialize or re-initialize rate limiter
	 *
	 * @param preload Initial balance to place in account
	 */
	inline void init(double preload)
		throw()
	{
		_lastTime = Utils::nowf();
		_balance = preload;
	}

	/**
	 * Update balance based on current clock and supplied Limit
	 *
	 * @param lim Current limits in effect
	 * @param deduct Amount to deduct, or 0.0 to just update
	 * @return New balance with deduction applied
	 */
	inline double update(const Limit &lim,double deduct)
		throw()
	{
		double lt = _lastTime;
		double now = _lastTime = Utils::nowf();
		return (_balance = fmax(lim.minBalance,fmin(lim.maxBalance,(_balance + (lim.bytesPerSecond * (now - lt))) - deduct)));
	}

private:
	double _lastTime;
	double _balance;
};

} // namespace ZeroTier

#endif
