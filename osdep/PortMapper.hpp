/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2019  ZeroTier, Inc.  https://www.zerotier.com/
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

#ifdef ZT_USE_MINIUPNPC

#ifndef ZT_PORTMAPPER_HPP
#define ZT_PORTMAPPER_HPP

#include <vector>

#include "../node/Constants.hpp"
#include "../node/InetAddress.hpp"
#include "../node/Mutex.hpp"
#include "Thread.hpp"

/**
 * How frequently should we refresh our UPNP/NAT-PnP/whatever state?
 */
#define ZT_PORTMAPPER_REFRESH_DELAY 300000

namespace ZeroTier {

class PortMapperImpl;

/**
 * UPnP/NAT-PnP port mapping "daemon"
 */
class PortMapper
{
	friend class PortMapperImpl;

public:
	/**
	 * Create and start port mapper service
	 *
	 * @param localUdpPortToMap Port we want visible to the outside world
	 * @param name Unique name of this endpoint (based on ZeroTier address)
	 */
	PortMapper(int localUdpPortToMap,const char *uniqueName);

	~PortMapper();

	/**
	 * @return All current external mappings for our port
	 */
	std::vector<InetAddress> get() const;

private:
	PortMapperImpl *_impl;
};

} // namespace ZeroTier

#endif

#endif // ZT_USE_MINIUPNPC
