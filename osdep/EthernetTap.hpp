/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2023-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_ETHERNETTAP_HPP
#define ZT_ETHERNETTAP_HPP

#include "../node/Constants.hpp"
#include "../node/MAC.hpp"
#include "../node/InetAddress.hpp"
#include "../node/MulticastGroup.hpp"
#include "ManagedRoute.hpp"

#include <string>
#include <memory>
#include <vector>
#include <mutex>
#include <map>

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
	virtual bool addIps(std::vector<InetAddress> ips); // uses addIp() unless overridden
	virtual bool removeIp(const InetAddress &ip) = 0;
	virtual std::vector<InetAddress> ips() const = 0;
	virtual void put(const MAC &from,const MAC &to,unsigned int etherType,const void *data,unsigned int len) = 0;
	virtual std::string deviceName() const = 0;
	virtual std::string routingDeviceName() const;
	virtual void setFriendlyName(const char *friendlyName) = 0;
	virtual void scanMulticastGroups(std::vector<MulticastGroup> &added,std::vector<MulticastGroup> &removed) = 0;
	virtual void setMtu(unsigned int mtu) = 0;

	ZT_ALWAYS_INLINE int addRoute(const InetAddress &target,const InetAddress &via,const unsigned int metric)
	{
		const std::string dn(this->routingDeviceName());
		const char *const dnp = (dn.length() > 0) ? dn.c_str() : (const char *)0;
		std::lock_guard<std::mutex> l(_managedRoutes_l);
		_managedRoutes[std::pair<InetAddress,unsigned int>(target,metric)] = std::shared_ptr<ManagedRoute>(new ManagedRoute(target,via,dnp));
	}

	ZT_ALWAYS_INLINE int removeRoute(const InetAddress &target,const InetAddress &via,const unsigned int metric)
	{
		std::lock_guard<std::mutex> l(_managedRoutes_l);
		_managedRoutes.erase(std::pair<InetAddress,unsigned int>(target,metric));
	}

	ZT_ALWAYS_INLINE int syncRoutes()
	{
		std::lock_guard<std::mutex> l(_managedRoutes_l);
		for(auto r=_managedRoutes.begin();r!=_managedRoutes.end();++r) {
			r->second->sync();
		}
		return 0;
	}

private:
	std::map< std::pair<InetAddress,unsigned int>,std::shared_ptr<ManagedRoute> > _managedRoutes;
	std::mutex _managedRoutes_l;
};

} // namespace ZeroTier

#endif
