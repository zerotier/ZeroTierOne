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

#include "Network.hpp"
#include "Switch.hpp"

namespace ZeroTier {

Network::Network(const RuntimeEnvironment *renv,uint64_t id)
	throw(std::runtime_error) :
	Thread(),
	_r(renv),
	_id(id),
	_tap(renv,renv->identity.address().toMAC(),ZT_IF_MTU),
	_members(),
	_open(false),
	_lock()
{
	TRACE("new network %llu created, TAP device: %s",id,_tap.deviceName().c_str());
	start();
}

Network::~Network()
{
	_tap.close();
	join();
	TRACE("network %llu (%s) closed",_id,_tap.deviceName().c_str());
}

void Network::main()
	throw()
{
	Buffer<4096> buf;
	MAC from,to;
	unsigned int etherType = 0;

	while (_tap.open()) {
		unsigned int len = _tap.get(from,to,etherType,buf.data());
		if (len) {
			buf.setSize(len);
			try {
				if (!*__refCount)
					break; // sanity check
				_r->sw->onLocalEthernet(SharedPtr<Network>(this),from,to,etherType,buf);
			} catch (std::exception &exc) {
				TRACE("unexpected exception handling local packet: %s",exc.what());
			} catch ( ... ) {
				TRACE("unexpected exception handling local packet");
			}
		} else break;
	}

	TRACE("network %llu thread terminating",_id);
}

} // namespace ZeroTier
