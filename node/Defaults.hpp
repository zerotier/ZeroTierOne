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

#ifndef _ZT_DEFAULTS_HPP
#define _ZT_DEFAULTS_HPP

#include <stdexcept>
#include <string>
#include <vector>
#include <map>
#include "Identity.hpp"
#include "InetAddress.hpp"
#include "Filter.hpp"

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
	Defaults()
		throw(std::runtime_error);
	~Defaults() {}

	/**
	 * Supernodes on the ZeroTier network
	 */
	const std::map< Identity,std::vector<InetAddress> > supernodes;

	/**
	 * URL prefix for autoconfiguration
	 */
	const std::string configUrlPrefix;

	/**
	 * Identity used to encrypt and authenticate configuration from URL
	 */
	const std::string configAuthority;

	/**
	 * Default node filter for this platform
	 */
	const Filter defaultNodeFilter;
};

extern const Defaults ZT_DEFAULTS;

} // namespace ZeroTier

#endif
