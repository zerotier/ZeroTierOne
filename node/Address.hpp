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

#ifndef _ZT_ADDRESS_HPP
#define _ZT_ADDRESS_HPP

#include <stdint.h>
#include <string>
#include "Utils.hpp"
#include "MAC.hpp"
#include "Constants.hpp"

namespace ZeroTier {

/**
 * ZeroTier address, which doubles as the last 5 octets of the MAC on taps
 *
 * Natural sort order will differ on big vs. little endian machines, but that
 * won't matter when it's used as a local map/set key.
 */
class Address
{
private:
	union {
		unsigned char o[ZT_ADDRESS_LENGTH];
		uint64_t v;
	} _a;

public:
	Address()
		throw()
	{
		_a.v = 0;
	}

	Address(const Address &a)
		throw()
	{
		_a.v = a._a.v;
	}

	/**
	 * Create from a ZeroTier MAC
	 *
	 * @param m MAC (assumed to be a ZeroTier MAC)
	 */
	Address(const MAC &m)
		throw()
	{
		_a.v = 0;
		for(int i=0;i<ZT_ADDRESS_LENGTH;++i)
			_a.o[i] = m.data[i + 1];
	}

	/**
	 * @param bits Raw address -- 5 bytes in length
	 */
	Address(const void *bits)
		throw()
	{
		_a.v = 0;
		for(int i=0;i<ZT_ADDRESS_LENGTH;++i)
			_a.o[i] = ((const unsigned char *)bits)[i];
	}

	inline Address &operator=(const Address &a)
		throw()
	{
		_a.v = a._a.v;
		return *this;
	}

	/**
	 * Derive a MAC whose first octet is the ZeroTier LAN standard
	 * 
	 * @return Ethernet MAC derived from address
	 */
	inline MAC toMAC() const
		throw()
	{
		MAC m;
		m.data[0] = ZT_MAC_FIRST_OCTET;
		for(int i=1;i<6;++i)
			m.data[i] = _a.o[i - 1];
		return m;
	}

	/**
	 * @return Hexadecimal string
	 */
	inline std::string toString() const
	{
		return Utils::hex(_a.o,ZT_ADDRESS_LENGTH);
	};

	/**
	 * Set address to zero
	 */
	inline void zero() throw() { _a.v = 0; }

	/**
	 * @return True if this address is not zero
	 */
	inline operator bool() const throw() { return (_a.v); }

	/**
	 * @return Sum of all bytes in address
	 */
	inline unsigned int sum() const
		throw()
	{
		unsigned int s = 0;
		for(unsigned int i=0;i<ZT_ADDRESS_LENGTH;++i)
			s += _a.o[i];
		return s;
	}

	/**
	 * Check if this address is reserved
	 * 
	 * The all-zero null address and any address beginning with 0xff are
	 * reserved. (0xff is reserved for future use to designate possibly
	 * longer addresses, addresses based on IPv6 innards, etc.)
	 * 
	 * @return True if address is reserved and may not be used
	 */
	inline bool isReserved() const
		throw()
	{
		return ((!_a.v)||(_a.o[0] == ZT_ADDRESS_RESERVED_PREFIX));
	}

	inline unsigned char *data() throw() { return _a.o; }
	inline const unsigned char *data() const throw() { return _a.o; }

	inline unsigned int size() const throw() { return ZT_ADDRESS_LENGTH; }

	inline unsigned char &operator[](unsigned int i) throw() { return _a.o[i]; }
	inline unsigned char operator[](unsigned int i) const throw() { return _a.o[i]; }

	inline bool operator==(const Address &a) const throw() { return (_a.v == a._a.v); }
	inline bool operator!=(const Address &a) const throw() { return (_a.v != a._a.v); }
	inline bool operator<(const Address &a) const throw() { return (_a.v < a._a.v); }
	inline bool operator>(const Address &a) const throw() { return (_a.v > a._a.v); }
	inline bool operator<=(const Address &a) const throw() { return (_a.v <= a._a.v); }
	inline bool operator>=(const Address &a) const throw() { return (_a.v >= a._a.v); }
};

} // namespace ZeroTier

#endif

