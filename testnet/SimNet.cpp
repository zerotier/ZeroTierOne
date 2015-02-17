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

#include "SimNet.hpp"

#include "../node/Constants.hpp"
#include "../node/Utils.hpp"

namespace ZeroTier {

SimNet::SimNet()
{
}

SimNet::~SimNet()
{
}

SimNetSocketManager *SimNet::newEndpoint(const InetAddress &addr)
{
	Mutex::Lock _l(_lock);

	if (_endpoints.size() >= ZT_SIMNET_MAX_TESTNET_SIZE)
		return (SimNetSocketManager *)0;
	if (_endpoints.find(addr) != _endpoints.end())
		return (SimNetSocketManager *)0;

	SimNetSocketManager *sm = new SimNetSocketManager();
	sm->_sn = this;
	sm->_address = addr;
	_endpoints[addr] = sm;
	return sm;
}

SimNetSocketManager *SimNet::get(const InetAddress &addr)
{
	Mutex::Lock _l(_lock);
	std::map< InetAddress,SimNetSocketManager * >::iterator ep(_endpoints.find(addr));
	if (ep == _endpoints.end())
		return (SimNetSocketManager *)0;
	return ep->second;
}

} // namespace ZeroTier
