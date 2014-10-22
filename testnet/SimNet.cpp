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

SimNetSocketManager *newEndpoint()
{
	Mutex::Lock _l(_lock);

	if (_endpoints.size() >= ZT_SIMNET_MAX_TESTNET_SIZE)
		return (SimNetSocketManager *)0;

	InetAddress fake;
	uint32_t ip = _prng.next32();
	for(;;) {
		++ip;
		ip &= 0x00ffffff;
		ip |= 0x0a000000; // 10.x.x.x
		if (((ip >> 16) & 0xff) == 0xff) ip ^= 0x00010000;
		if (((ip >> 8) & 0xff) == 0xff) ip ^= 0x00000100;
		if ((ip & 0xff) == 0xff) --ip;
		if ((ip & 0xff) == 0x00) ++ip;
		uint32_t ipn = Utils::hton(ip);
		fake.set(&ipn,4,8); // 10.x.x.x/8
		if (_endpoints.find(fake) == _endpoints.end()) {
			SimNetSocketManager *sm = &(_endpoints[fake]);
			sm->_sn = this;
			sm->_address = fake;
			return sm;
		}
	}
}

SimNetSocketManager *get(const InetAddress &addr)
{
	Mutex::Lock _l(_lock);
	std::map< InetAddress,SimNetSocketManager >::iterator ep(_endpoints.find(addr));
	if (ep == _endpoints.end())
		return (SimNetSocketManager *)0;
	return &(ep->second);
}

std::vector<SimNetSocketManager *> SimNet::all()
{
	std::vector<SimNetSocketManager *> a;
	Mutex::Lock _l(_lock);
	for (std::map< InetAddress,SimNetSocketManager >::iterator ep(_endpoints.begin());ep!=_endpoints.end();++ep)
		a.push_back(&(ep->second));
	return a;
}

} // namespace ZeroTier
