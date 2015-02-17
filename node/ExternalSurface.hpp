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

#ifndef ZT_EXTERNALSURFACE_HPP
#define ZT_EXTERNALSURFACE_HPP

#include "InetAddress.hpp"

namespace ZeroTier {

/**
 * Tracks changes to this peer's real world addresses
 */
class ExternalSurface
{
public:
	ExternalSurface() {}

	/**
	 * Revise our external surface image, return true if it changed
	 *
	 * @param remote Remote address as reflected by any trusted peer
	 * @return True if our external surface has changed
	 */
	inline bool update(const InetAddress &remote)
		throw()
	{
		const unsigned long idx = (remote.isV4() ? 0 : 2) | (remote.isLinkLocal() ? 1 : 0);
		if (_s[idx] != remote) {
			_s[idx] = remote;
			return true;
		}
		return false;
	}

private:
	InetAddress _s[4]; // global v4, link-local v4, global v6, link-local v6
};

} // namespace ZeroTier

#endif
