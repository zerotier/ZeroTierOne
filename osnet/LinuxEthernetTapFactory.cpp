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

#include "LinuxEthernetTapFactory.hpp"
#include "LinuxEthernetTap.hpp"

namespace ZeroTier {

LinuxEthernetTapFactory::LinuxEthernetTapFactory()
{
}

LinuxEthernetTapFactory::~LinuxEthernetTapFactory()
{
	Mutex::Lock _l(_devices_m);
	for(std::vector<EthernetTap *>::iterator d(_devices.begin());d!=_devices.end();++d)
		delete *d;
}

EthernetTap *LinuxEthernetTapFactory::open(
	const MAC &mac,
	unsigned int mtu,
	unsigned int metric,
	uint64_t nwid,
	const char *desiredDevice,
	const char *friendlyName,
	void (*handler)(void *,const MAC &,const MAC &,unsigned int,const Buffer<4096> &),
	void *arg)
{
	Mutex::Lock _l(_devices_m);
	EthernetTap *t = new LinuxEthernetTap(mac,mtu,metric,nwid,desiredDevice,friendlyName,handler,arg);
	_devices.push_back(t);
	return t;
}

void LinuxEthernetTapFactory::close(EthernetTap *tap,bool destroyPersistentDevices)
{
	{
		Mutex::Lock _l(_devices_m);
		for(std::vector<EthernetTap *>::iterator d(_devices.begin());d!=_devices.end();++d) {
			if (*d == tap) {
				_devices.erase(d);
				break;
			}
		}
	}
	delete tap;
}

std::vector<std::string> allTapDeviceNames() const
{
	std::vector<std::string> dn;
	Mutex::Lock _l(_devices_m);
	for(std::vector<EthernetTap *>::const_iterator d(_devices.begin());d!=_devices.end();++d)
		dn.push_back(d->deviceName());
	return dn;
}

} // namespace ZeroTier
