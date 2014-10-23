/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2011-2014  ZeroTier Networks LLC
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

#ifndef ZT_SIMNET_HPP
#define ZT_SIMNET_HPP

#include <map>
#include <vector>

#include "../node/Constants.hpp"
#include "../node/InetAddress.hpp"
#include "../node/Mutex.hpp"

#include "SimNetSocketManager.hpp"

#define ZT_SIMNET_MAX_TESTNET_SIZE 1048576

namespace ZeroTier {

/**
 * A simulated headless IP network for testing
 */
class SimNet
{
public:
	SimNet();
	~SimNet();

	/**
	 * @return New endpoint or NULL on failure
	 */
	SimNetSocketManager *newEndpoint(const InetAddress &addr);

	/**
	 * @param addr Address to look up
	 * @return Endpoint or NULL if none
	 */
	SimNetSocketManager *get(const InetAddress &addr);

private:
	std::map< InetAddress,SimNetSocketManager * > _endpoints;
	Mutex _lock;
};

} // namespace ZeroTier

#endif
