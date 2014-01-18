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

#ifndef ZT_BWACCOUNT_HPP
#define ZT_BWACCOUNT_HPP

#include <stdint.h>
#include <math.h>

#include "Constants.hpp"
#include "Utils.hpp"

#ifdef __WINDOWS__
#define	round(x) ((x-floor(x))>0.5 ? ceil(x) : floor(x))
#endif

namespace ZeroTier {

/**
 * Bandwidth account used for rate limiting multicast groups
 *
 * This is used to apply a bank account model to multicast groups. Each
 * multicast packet counts against a balance, which accrues at a given
 * rate in bytes per second. Debt is possible. These parameters are
 * configurable.
 *
 * A bank account model permits bursting behavior, which correctly models
 * how OSes and apps typically use multicast. It's common for things to
 * spew lots of multicast messages at once, wait a while, then do it
 * again. A consistent bandwidth limit model doesn't fit.
 */
class BandwidthAccount
{
public:
	/**
	 * Create an uninitialized account
	 *
	 * init() must be called before this is used.
	 */
	BandwidthAccount() throw() {}

	/**
	 * Create and initialize
	 *
	 * @param preload Initial balance to place in account
	 * @param maxb Maximum allowed balance (> 0)
	 * @param acc Rate of accrual in bytes per second
	 */
	BandwidthAccount(uint32_t preload,uint32_t maxb,uint32_t acc)
		throw()
	{
		init(preload,maxb,acc);
	}

	/**
	 * Initialize or re-initialize account
	 *
	 * @param preload Initial balance to place in account
	 * @param maxb Maximum allowed balance (> 0)
	 * @param acc Rate of accrual in bytes per second
	 */
	inline void init(uint32_t preload,uint32_t maxb,uint32_t acc)
		throw()
	{
		_lastTime = Utils::nowf();
		_balance = preload;
		_maxBalance = maxb;
		_accrual = acc;
	}

	/**
	 * Update and retrieve balance of this account
	 *
	 * @return New balance updated from current clock
	 */
	inline uint32_t update()
		throw()
	{
		double lt = _lastTime;
		double now = Utils::nowf();
		_lastTime = now;
		return (_balance = std::min(_maxBalance,(uint32_t)round((double)_balance + ((double)_accrual * (now - lt)))));
	}

	/**
	 * Update balance and conditionally deduct
	 *
	 * If the deduction amount fits, it is deducted after update. Otherwise
	 * balance is updated and false is returned.
	 *
	 * @param amt Amount to deduct
	 * @return True if amount fit within balance and was deducted
	 */
	inline bool deduct(uint32_t amt)
		throw()
	{
		if (update() >= amt) {
			_balance -= amt;
			return true;
		}
		return false;
	}

	/**
	 * @return Most recent balance without update
	 */
	inline uint32_t balance() const
		throw()
	{
		return _balance;
	}

private:
	double _lastTime;
	uint32_t _balance;
	uint32_t _maxBalance;
	uint32_t _accrual;
};

} // namespace ZeroTier

#endif
