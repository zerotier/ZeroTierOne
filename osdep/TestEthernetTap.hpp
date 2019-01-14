/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2019  ZeroTier, Inc.  https://www.zerotier.com/
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

#ifndef ZT_TESTETHERNETTAP_HPP
#define ZT_TESTETHERNETTAP_HPP

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <string>
#include <vector>
#include <stdexcept>
#include <set>

#include "../node/Constants.hpp"
#include "../node/InetAddress.hpp"
#include "../node/MulticastGroup.hpp"
#include "../node/Mutex.hpp"
#include "../node/Utils.hpp"
#include "../osdep/OSUtils.hpp"

namespace ZeroTier {

/**
 * Dummy test Ethernet tap that does not actually open a device on the system
 */
class TestEthernetTap
{
public:
	TestEthernetTap(
		const char *homePath,
		const MAC &mac,
		unsigned int mtu,
		unsigned int metric,
		uint64_t nwid,
		const char *friendlyName,
		void (*handler)(void *,void *,uint64_t,const MAC &,const MAC &,unsigned int,unsigned int,const void *,unsigned int),
		void *arg) :
		_nwid(nwid),
		_dev("zt_test_"),
		_enabled(true)
	{
		char tmp[32];
		OSUtils::ztsnprintf(tmp,sizeof(tmp),"%.16llx",(unsigned long long)_nwid);
		_dev.append(tmp);
#ifdef ZT_TEST_TAP_REPORT_TO
		_reportTo.fromString(ZT_TEST_TAP_REPORT_TO);
		if (_reportTo.ss_family == AF_INET)
			_reportsock = socket(AF_INET,SOCK_DGRAM,0);
		else if (_reportTo.ss_family == AF_INET6)
			_reportsock = socket(AF_INET6,SOCK_DGRAM,0);
		else _reportsock = -1;
#endif
	}

	~TestEthernetTap()
	{
#ifdef ZT_TEST_TAP_REPORT_TO
		if (_reportsock >= 0)
			close(_reportsock);
#endif
	}

	inline void setEnabled(bool en) { _enabled = en; }
	inline bool enabled() const { return _enabled; }

	inline bool addIp(const InetAddress &ip)
	{
		Mutex::Lock _l(_lock);
		_ips.insert(ip);
		return true;
	}

	inline bool removeIp(const InetAddress &ip)
	{
		Mutex::Lock _l(_lock);
		_ips.erase(ip);
		return true;
	}

	inline std::vector<InetAddress> ips() const
	{
		Mutex::Lock _l(_lock);
		return std::vector<InetAddress>(_ips.begin(),_ips.end());
	}

	inline void put(const MAC &from,const MAC &to,unsigned int etherType,const void *data,unsigned int len)
	{
#ifdef ZT_TEST_TAP_REPORT_TO
		char tmp[10000];
		if ((_reportsock >= 0)&&(len < (sizeof(tmp) - 22))) {
			const uint64_t nwid2 = Utils::hton(_nwid);
			memcpy(tmp,&nwid2,8);
			from.copyTo(tmp + 8,6);
			to.copyTo(tmp + 14,6);
			const uint16_t etherType2 = Utils::hton((uint16_t)etherType);
			memcpy(tmp + 20,&etherType2,2);
			memcpy(tmp + 22,data,len);
			sendto(_reportsock,tmp,len + 22,0,reinterpret_cast<const struct sockaddr *>(&_reportTo),(_reportTo.ss_family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6));
		}
#endif
	}

	inline std::string deviceName() const
	{
		return _dev;
	}

	inline void setFriendlyName(const char *friendlyName)
	{
	}

	inline void scanMulticastGroups(std::vector<MulticastGroup> &added,std::vector<MulticastGroup> &removed)
	{
	}

	inline void setMtu(unsigned int mtu)
	{
	}

private:
	uint64_t _nwid;
	std::string _dev;
	std::set<InetAddress> _ips;
	InetAddress _reportTo;
#ifdef ZT_TEST_TAP_REPORT_TO
	int _reportsock;
#endif
	bool _enabled;
	Mutex _lock;
};

} // namespace ZeroTier

#endif
