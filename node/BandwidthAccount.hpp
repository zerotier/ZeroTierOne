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

#ifndef _ZT_BWACCOUNT_HPP
#define _ZT_BWACCOUNT_HPP

#include <stdint.h>
#include <math.h>

#include "Constants.hpp"
#include "Utils.hpp"

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
	BandwidthAccount(int32_t preload,int32_t maxb,int32_t acc)
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
	inline void init(int32_t preload,int32_t maxb,int32_t acc)
		throw()
	{
		_lastTime = Utils::nowf();
		_balance = preload;
		_maxBalance = maxb;
		_accrual = acc;
	}

	/**
	 * Update balance by accruing and then deducting
	 *
	 * @param deduct Amount to deduct, or 0.0 to just update
	 * @return New balance after deduction -- if negative, it didn't fit
	 */
	inline int32_t update(int32_t deduct)
		throw()
	{
		double lt = _lastTime;
		double now = Utils::nowf();
		_lastTime = now;
		int32_t newbal = (int32_t)round((double)_balance + ((double)_accrual * (now - lt))) - deduct;
		_balance = std::max((int32_t)0,std::min(_maxBalance,newbal));
		return newbal;
	}

private:
	double _lastTime;
	int32_t _balance;
	int32_t _maxBalance;
	int32_t _accrual;
};

} // namespace ZeroTier

#endif
