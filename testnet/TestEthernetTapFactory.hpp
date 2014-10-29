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

#ifndef ZT_TESTETHERNETTAPFACTORY_HPP
#define ZT_TESTETHERNETTAPFACTORY_HPP

#include <vector>
#include <string>
#include <set>

#include "../node/EthernetTapFactory.hpp"
#include "../node/Mutex.hpp"
#include "../node/MAC.hpp"
#include "TestEthernetTap.hpp"

namespace ZeroTier {

class TestEthernetTapFactory : public EthernetTapFactory
{
public:
	TestEthernetTapFactory();
	virtual ~TestEthernetTapFactory();

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

	inline TestEthernetTap *getByMac(const MAC &mac) const
	{
		Mutex::Lock _l(_tapsByMac_m);
		std::map< MAC,TestEthernetTap * >::const_iterator t(_tapsByMac.find(mac));
		if (t == _tapsByMac.end())
			return (TestEthernetTap *)0;
		return t->second;
	}

	inline TestEthernetTap *getByNwid(uint64_t nwid) const
	{
		Mutex::Lock _l(_tapsByNwid_m);
		std::map< uint64_t,TestEthernetTap * >::const_iterator t(_tapsByNwid.find(nwid));
		if (t == _tapsByNwid.end())
			return (TestEthernetTap *)0;
		return t->second;
	}

private:
	std::set< EthernetTap * > _taps;
	Mutex _taps_m;

	std::map< MAC,TestEthernetTap * > _tapsByMac;
	Mutex _tapsByMac_m;

	std::map< uint64_t,TestEthernetTap * > _tapsByNwid;
	Mutex _tapsByNwid_m;
};

} // namespace ZeroTier

#endif
