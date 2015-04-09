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

#ifndef ZT_DEFAULTS_HPP
#define ZT_DEFAULTS_HPP

#include <stdexcept>
#include <string>
#include <vector>
#include <map>

#include "Constants.hpp"
#include "Identity.hpp"
#include "InetAddress.hpp"

namespace ZeroTier {

/**
 * Static configuration defaults
 * 
 * These are the default values that ship baked into the ZeroTier binary. They
 * define the basic parameters required for it to connect to the rest of the
 * network and obtain software updates.
 */
class Defaults
{
public:
	Defaults();

	/**
	 * Default root topology dictionary
	 */
	const std::string defaultRootTopology;

	/**
	 * Identities permitted to sign root topology dictionaries
	 */
	const std::map< Address,Identity > rootTopologyAuthorities;

	/**
	 * Address for IPv4 LAN auto-location broadcasts: 255.255.255.255:9993
	 */
	const InetAddress v4Broadcast;
};

extern const Defaults ZT_DEFAULTS;

} // namespace ZeroTier

#endif
