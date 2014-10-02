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

#include "../node/SharedPtr.hpp"
#include "../node/EthernetTapFactory.hpp"
#include "../node/Mutex.hpp"
#include "../node/MAC.hpp"
#include "../node/CMWC4096.hpp"

namespace ZeroTier {

class TestEthernetTap;

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

	inline SharedPtr<TestEthernetTap> getByMac(const MAC &mac) const
	{
		Mutex::Lock _l(_tapsByMac_m);
		std::map< MAC,SharedPtr<TestEthernetTap> >::const_iterator t(_tapsByMac.find(mac));
		if (t == _tapsByMac.end())
			return SharedPtr<TestEthernetTap>();
		return t->second;
	}

	inline SharedPtr<TestEthernetTap> getByDevice(const std::string &dev) const
	{
		Mutex::Lock _l(_tapsByDevice_m);
		std::map< std::string,SharedPtr<TestEthernetTap> >::const_iterator t(_tapsByDevice.find(dev));
		if (t == _tapsByDevice.end())
			return SharedPtr<TestEthernetTap>();
		return t->second;
	}

	inline SharedPtr<TestEthernetTap> getFirst() const
	{
		Mutex::Lock _l(_taps_m);
		if (_taps.empty())
			return SharedPtr<TestEthernetTap>();
		return *(_taps.begin());
	}

	inline SharedPtr<TestEthernetTap> getRandom() const
	{
		Mutex::Lock _l(_taps_m);
		Mutex::Lock _l2(_prng_m);
		if (_taps.empty())
			return SharedPtr<TestEthernetTap>();
		unsigned int x = (const_cast<CMWC4096 *>(&_prng))->next32() % (unsigned int)_taps.size();
		unsigned int i = 0;
		for(std::set< SharedPtr<TestEthernetTap> >::const_iterator t(_taps.begin());t!=_taps.end();++t) {
			if (i++ == x)
				return *t;
		}
		return SharedPtr<TestEthernetTap>(); // never reached
	}

private:
	std::set< SharedPtr<TestEthernetTap> > _taps;
	Mutex _taps_m;

	std::map<std::string,SharedPtr<TestEthernetTap> > _tapsByDevice;
	Mutex _tapsByDevice_m;

	std::map<MAC,SharedPtr<TestEthernetTap> > _tapsByMac;
	Mutex _tapsByMac_m;

	CMWC4096 _prng;
	Mutex _prng_m;
};

} // namespace ZeroTier

#endif
