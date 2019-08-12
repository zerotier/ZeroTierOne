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

#ifndef ZT_ETHERNETTAP_HPP
#define ZT_ETHERNETTAP_HPP

#include "../node/Constants.hpp"
#include "../node/MAC.hpp"
#include "../node/InetAddress.hpp"
#include "../node/MulticastGroup.hpp"

#include <string>
#include <memory>
#include <vector>

namespace ZeroTier {

class EthernetTap
{
public:
	static std::shared_ptr<EthernetTap> newInstance(
		const char *tapDeviceType, // OS-specific, NULL for default
		const char *homePath,
		const MAC &mac,
		unsigned int mtu,
		unsigned int metric,
		uint64_t nwid,
		const char *friendlyName,
		void (*handler)(void *,void *,uint64_t,const MAC &,const MAC &,unsigned int,unsigned int,const void *,unsigned int),
		void *arg);

	EthernetTap();
	virtual ~EthernetTap();

	virtual void setEnabled(bool en) = 0;
	virtual bool enabled() const = 0;
	virtual bool addIp(const InetAddress &ip) = 0;
	virtual bool removeIp(const InetAddress &ip) = 0;
	virtual std::vector<InetAddress> ips() const = 0;
	virtual void put(const MAC &from,const MAC &to,unsigned int etherType,const void *data,unsigned int len) = 0;
	virtual std::string deviceName() const = 0;
	virtual void setFriendlyName(const char *friendlyName) = 0;
	virtual void scanMulticastGroups(std::vector<MulticastGroup> &added,std::vector<MulticastGroup> &removed) = 0;
	virtual void setMtu(unsigned int mtu) = 0;
};

} // namespace ZeroTier

#endif
