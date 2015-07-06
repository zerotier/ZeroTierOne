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

#ifndef ZT_PATH_HPP
#define ZT_PATH_HPP

#include "Constants.hpp"
#include "InetAddress.hpp"
#include "Utils.hpp"

namespace ZeroTier {

class Path
{
public:
	// Must be the same values as ZT1_LocalInterfaceAddressTrust in ZeroTierOne.h
	enum Trust
	{
		TRUST_NORMAL = 0,
		TRUST_PRIVACY = 1,
		TRUST_ULTIMATE = 2
	};

	Path() :
		_addr(),
		_metric(0),
		_trust(TRUST_NORMAL),
		_reliable(false)
	{
	}

	Path(const InetAddress &addr,int metric,Trust trust,bool reliable) :
		_addr(addr),
		_metric(metric),
		_trust(trust),
		_reliable(reliable)
	{
	}

	/**
	 * @return Physical address
	 */
	inline const InetAddress &address() const throw() { return _addr; }

	/**
	 * @return Metric (higher == worse) or negative if path is blacklisted
	 */
	inline int metric() const throw() { return _metric; }

	/**
	 * @return Path trust level
	 */
	inline Trust trust() const throw() { return _trust; }

	/**
	 * @return True if path is considered reliable (no NAT keepalives etc. are needed)
	 */
	inline bool reliable() const throw() { return _reliable; }

	/**
	 * @return True if address is non-NULL
	 */
	inline operator bool() const throw() { return (_addr); }

	// Comparisons are by address only
	inline bool operator==(const Path &p) const throw() { return (_addr == p._addr); }
	inline bool operator!=(const Path &p) const throw() { return (_addr != p._addr); }
	inline bool operator<(const Path &p) const throw() { return (_addr < p._addr); }
	inline bool operator>(const Path &p) const throw() { return (_addr > p._addr); }
	inline bool operator<=(const Path &p) const throw() { return (_addr <= p._addr); }
	inline bool operator>=(const Path &p) const throw() { return (_addr >= p._addr); }

protected:
	InetAddress _addr;
	int _metric; // negative == blacklisted
	Trust _trust;
	bool _reliable;
};

} // namespace ZeroTier

#endif
