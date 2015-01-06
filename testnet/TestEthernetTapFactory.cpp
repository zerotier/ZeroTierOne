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

#include "TestEthernetTapFactory.hpp"
#include "TestEthernetTap.hpp"

namespace ZeroTier {

TestEthernetTapFactory::TestEthernetTapFactory()
{
}

TestEthernetTapFactory::~TestEthernetTapFactory()
{
	Mutex::Lock _l1(_taps_m);
	Mutex::Lock _l2(_tapsByMac_m);
	Mutex::Lock _l3(_tapsByNwid_m);
	for(std::set<EthernetTap *>::iterator t(_taps.begin());t!=_taps.end();++t)
		delete *t;
}

EthernetTap *TestEthernetTapFactory::open(
	const MAC &mac,
	unsigned int mtu,
	unsigned int metric,
	uint64_t nwid,
	const char *desiredDevice,
	const char *friendlyName,
	void (*handler)(void *,const MAC &,const MAC &,unsigned int,const Buffer<4096> &),
	void *arg)
{
	TestEthernetTap *tap = new TestEthernetTap(mac,mtu,metric,nwid,desiredDevice,friendlyName,handler,arg);
	Mutex::Lock _l1(_taps_m);
	Mutex::Lock _l2(_tapsByMac_m);
	Mutex::Lock _l3(_tapsByNwid_m);
	_taps.insert(tap);
	_tapsByMac[mac] = tap;
	_tapsByNwid[nwid] = tap;
	return tap;
}

void TestEthernetTapFactory::close(EthernetTap *tap,bool destroyPersistentDevices)
{
	Mutex::Lock _l1(_taps_m);
	Mutex::Lock _l2(_tapsByMac_m);
	Mutex::Lock _l3(_tapsByNwid_m);
	if (!tap)
		return;
	_taps.erase(tap);
	_tapsByMac.erase(tap->mac());
	_tapsByNwid.erase(((TestEthernetTap *)tap)->nwid());
	delete tap;
}

} // namespace ZeroTier
