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

#ifndef _ZT_RANGE_HPP
#define _ZT_RANGE_HPP

namespace ZeroTier {

/**
 * A range of numeric values
 *
 * @tparam T Type, can be any numeric value (int, float, double, etc.)
 */
template<typename T>
class Range
{
public:
	/**
	 * Construct an empty range
	 */
	Range()
		throw() :
		start(0),
		end(0)
	{
	}

	/**
	 * @param s Starting value (inclusive)
	 * @param e Ending value (exclusive)
	 */
	Range(T s,T e)
		throw() :
		start(s),
		end(e)
	{
	}

	/**
	 * Construct a range containing from n to n+1 (thus only n for integers)
	 *
	 * @param n Number to contain
	 */
	Range(T n)
		throw() :
		start(n),
		end(n+1)
	{
	}

	/**
	 * @return end - start
	 */
	inline T magnitude() const
		throw()
	{
		return (end - start);
	}

	/**
	 * @return True if range contains something (magnitude is nonzero)
	 */
	inline operator bool() const
		throw()
	{
		return (end > start);
	}

	/**
	 * @param v Value to test
	 * @return True if value is between start (inclusive) and end (exclusive)
	 */
	inline bool operator()(const T &v) const
		throw()
	{
		return ((v >= start)&&(v < end));
	}

	inline bool operator==(const Range &r) const throw() { return ((start == r.start)&&(end == r.end)); }
	inline bool operator!=(const Range &r) const throw() { return (!(*this == r)); }
	inline bool operator<(const Range &r) const throw() { return ((start < r.start) ? true : ((start == r.start) ? (end < r.end) : false)); }
	inline bool operator>(const Range &r) const throw() { return (r < *this); }
	inline bool operator<=(const Range &r) const throw() { return !(r < *this); }
	inline bool operator>=(const Range &r) const throw() { return !(*this < r); }

	/**
	 * Start of range (may be modified directly)
	 */
	T start;

	/**
	 * End of range (may be modified directly)
	 */
	T end;
};

} // namespace ZeroTier

#endif
