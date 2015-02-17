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
	 * Default home path for this platform
	 */
	const std::string defaultHomePath;

	/**
	 * Default root topology dictionary
	 */
	const std::string defaultRootTopology;

	/**
	 * Identities permitted to sign root topology dictionaries
	 */
	const std::map< Address,Identity > rootTopologyAuthorities;

	/**
	 * Identities permitted to sign software updates
	 *
	 * ZTN can keep multiple signing identities and rotate them, keeping some in
	 * "cold storage" and obsoleting others gradually.
	 *
	 * If you don't build with ZT_OFFICIAL_BUILD, this isn't used since your
	 * build will not auto-update.
	 */
	const std::map< Address,Identity > updateAuthorities;

	/**
	 * URL to latest .nfo for software updates
	 */
	const std::string updateLatestNfoURL;

	/**
	 * URL to check for updates to root topology
	 */
	const std::string rootTopologyUpdateURL;

	/**
	 * Address for IPv4 LAN auto-location broadcasts: 255.255.255.255:9993
	 */
	const InetAddress v4Broadcast;
};

extern const Defaults ZT_DEFAULTS;

} // namespace ZeroTier

#endif
