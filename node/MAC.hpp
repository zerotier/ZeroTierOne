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

#ifndef _ZT_MAC_HPP
#define _ZT_MAC_HPP

#include <stdio.h>
#include <stdlib.h>

#include "Constants.hpp"
#include "Array.hpp"
#include "Utils.hpp"

namespace ZeroTier {

/**
 * An Ethernet MAC address
 */
class MAC : public Array<unsigned char,6>
{
public:
	/**
	 * Create a zero/null MAC
	 */
	MAC()
		throw()
	{
		for(unsigned int i=0;i<6;++i)
			data[i] = 0;
	}

	/**
	 * Create a MAC consisting of only this octet
	 *
	 * @param octet Octet to fill MAC with (e.g. 0xff for broadcast-all)
	 */
	MAC(const unsigned char octet)
		throw()
	{
		for(unsigned int i=0;i<6;++i)
			data[i] = octet;
	}

	/**
	 * Create a MAC from raw bits
	 *
	 * @param bits 6 bytes of MAC address data
	 */
	MAC(const void *bits)
		throw()
	{
		for(unsigned int i=0;i<6;++i)
			data[i] = ((const unsigned char *)bits)[i];
	}

	/**
	 * @return True if non-NULL (not all zero)
	 */
	inline operator bool() const
		throw()
	{
		for(unsigned int i=0;i<6;++i) {
			if (data[i])
				return true;
		}
		return false;
	}

	/**
	 * @return True if this is the broadcast-all MAC (0xff:0xff:...)
	 */
	inline bool isBroadcast() const
		throw()
	{
		for(unsigned int i=0;i<6;++i) {
			if (data[i] != 0xff)
				return false;
		}
		return true;
	}

	/**
	 * @return True if this is a multicast/broadcast address
	 */
	inline bool isMulticast() const
		throw()
	{
		return ((data[0] & 1));
	}

	/**
	 * @return True if this is a ZeroTier unicast MAC
	 */
	inline bool isZeroTier() const
		throw()
	{
		return (data[0] == ZT_MAC_FIRST_OCTET);
	}

	/**
	 * Zero this MAC
	 */
	inline void zero()
		throw()
	{
		for(unsigned int i=0;i<6;++i)
			data[i] = 0;
	}

	/**
	 * @param s String hex representation (with or without :'s)
	 * @return True if string decoded into a full-length MAC
	 */
	inline void fromString(const char *s)
	{
		Utils::unhex(s,data,6);
	}

	inline std::string toString() const
	{
		char tmp[32];
		Utils::snprintf(tmp,sizeof(tmp),"%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",(int)data[0],(int)data[1],(int)data[2],(int)data[3],(int)data[4],(int)data[5]);
		return std::string(tmp);
	}
};

} // namespace ZeroTier

#endif
