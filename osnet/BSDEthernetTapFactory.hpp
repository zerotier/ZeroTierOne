/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2011-2015  ZeroTier Networks
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

#ifndef ZT_BSDETHERNETTAPFACTORY_HPP
#define ZT_BSDETHERNETTAPFACTORY_HPP

#include <vector>
#include <string>

#include "../node/EthernetTapFactory.hpp"
#include "../node/Mutex.hpp"

namespace ZeroTier {

class BSDEthernetTapFactory : public EthernetTapFactory
{
public:
	BSDEthernetTapFactory();
	virtual ~BSDEthernetTapFactory();

	virtual EthernetTap *open(
		const MAC &mac,
		unsigned int mtu,
		unsigned int metric,
		uint64_t nwid,
		const char *desiredDevice,
		const char *friendlyName,
		void (*handler)(void *,const MAC &,const MAC &,unsigned int,const Buffer<4096> &),
		void *arg);
	virtual void close(EthernetTap *tap,bool destroyPersistentDevices);

private:
	std::vector<EthernetTap *> _devices;
	Mutex _devices_m;
};

} // namespace ZeroTier

#endif
