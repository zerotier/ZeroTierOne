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

#include "TestEthernetTapFactory.hpp"
#include "TestEthernetTap.hpp"

namespace ZeroTier {

TestEthernetTapFactory::TestEthernetTapFactory()
{
}

TestEthernetTapFactory::~TestEthernetTapFactory()
{
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
	SharedPtr<TestEthernetTap> tap(new TestEthernetTap(this,mac,mtu,metric,nwid,desiredDevice,friendlyName,handler,arg));
	{
		Mutex::Lock _l(_taps_m);
		_taps.insert(tap);
	}
	{
		Mutex::Lock _l(_tapsByMac_m);
		_tapsByMac[mac] = tap;
	}
	{
		Mutex::Lock _l(_tapsByNwid_m);
		_tapsByNwid[nwid] = tap;
	}
	return tap.ptr();
}

void TestEthernetTapFactory::close(EthernetTap *tap,bool destroyPersistentDevices)
{
	if (!tap)
		return;
	SharedPtr<TestEthernetTap> tapp((TestEthernetTap *)tap);
	{
		Mutex::Lock _l(_taps_m);
		_taps.erase(tapp);
	}
	{
		Mutex::Lock _l(_tapsByMac_m);
		_tapsByMac.erase(tapp->mac());
	}
	{
		Mutex::Lock _l(_tapsByNwid_m);
		_tapsByNwid.erase(tapp->nwid());
	}
}

} // namespace ZeroTier
