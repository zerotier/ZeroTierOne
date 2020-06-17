/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2024-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#include "EthernetTap.hpp"
#include "OSUtils.hpp"

#ifdef ZT_SDK

#include "../controller/EmbeddedNetworkController.hpp"
#include "../core/Node.hpp"
#include "../include/VirtualTap.hpp"

#else

#ifdef __APPLE__
#include <sys/sysctl.h>
#include "MacEthernetTap.hpp"
#include "MacKextEthernetTap.hpp"
#endif // __APPLE__

#ifdef __LINUX__
#include "LinuxEthernetTap.hpp"
#endif // __LINUX__

#ifdef __WINDOWS__
#include "WindowsEthernetTap.hpp"
#endif // __WINDOWS__

#ifdef __FreeBSD__
#include "BSDEthernetTap.hpp"
#endif // __FreeBSD__

#ifdef __NetBSD__
#include "NetBSDEthernetTap.hpp"
#endif // __NetBSD__

#ifdef __OpenBSD__
#include "BSDEthernetTap.hpp"
#endif // __OpenBSD__

#endif

namespace ZeroTier {

std::shared_ptr<EthernetTap> EthernetTap::newInstance(
	const char *tapDeviceType, // OS-specific, NULL for default
	const char *homePath,
	const MAC &mac,
	unsigned int mtu,
	unsigned int metric,
	uint64_t nwid,
	const char *friendlyName,
	void (*handler)(void *,void *,uint64_t,const MAC &,const MAC &,unsigned int,unsigned int,const void *,unsigned int),
	void *arg)
{

#ifdef ZT_SDK

	return std::shared_ptr<EthernetTap>(new VirtualTap(homePath,mac,mtu,metric,nwid,friendlyName,handler,arg));

#else // not ZT_SDK

#ifdef __APPLE__
	char osrelease[256];
	size_t size = sizeof(osrelease);
	if (sysctlbyname("kern.osrelease",osrelease,&size,nullptr,0) == 0) {
		char *dotAt = strchr(osrelease,'.');
		if (dotAt) {
			*dotAt = (char)0;
			// The "feth" virtual Ethernet device type appeared in Darwin 17.x.x. Older versions
			// (Sierra and earlier) must use the a kernel extension.
			if (strtol(osrelease,(char **)0,10) < 17) {
				return std::shared_ptr<EthernetTap>(new MacKextEthernetTap(homePath,mac,mtu,metric,nwid,friendlyName,handler,arg));
			} else {
				return std::shared_ptr<EthernetTap>(new MacEthernetTap(homePath,mac,mtu,metric,nwid,friendlyName,handler,arg));
			}
		}
	}
#endif // __APPLE__

#ifdef __LINUX__
	return std::shared_ptr<EthernetTap>(new LinuxEthernetTap(homePath,mac,mtu,metric,nwid,friendlyName,handler,arg));
#endif // __LINUX__

#ifdef __WINDOWS__
	return std::shared_ptr<EthernetTap>(new WindowsEthernetTap(homePath,mac,mtu,metric,nwid,friendlyName,handler,arg));
#endif // __WINDOWS__

#ifdef __FreeBSD__
	return std::shared_ptr<EthernetTap>(new BSDEthernetTap(homePath,mac,mtu,metric,nwid,friendlyName,handler,arg));
#endif // __FreeBSD__

#ifdef __NetBSD__
	return std::shared_ptr<EthernetTap>(new NetBSDEthernetTap(homePath,mac,mtu,metric,nwid,friendlyName,handler,arg));
#endif // __NetBSD__

#ifdef __OpenBSD__
	return std::shared_ptr<EthernetTap>(new BSDEthernetTap(homePath,mac,mtu,metric,nwid,friendlyName,handler,arg));
#endif // __OpenBSD__

#endif // ZT_SDK?

	return std::shared_ptr<EthernetTap>();
}

EthernetTap::EthernetTap() {}
EthernetTap::~EthernetTap() {}

bool EthernetTap::addIps(std::vector<InetAddress> ips)
{
	for(std::vector<InetAddress>::const_iterator i(ips.begin());i!=ips.end();++i) {
		if (!addIp(*i))
			return false;
	}
	return true;
}

std::string EthernetTap::routingDeviceName() const
{
#ifdef __WINDOWS__
	char tapdev[64];
	OSUtils::ztsnprintf(tapdev,sizeof(tapdev),"%.16llx",(unsigned long long)(((const WindowsEthernetTap *)(this))->luid().Value));
	return std::string(tapdev);
#else
	return this->deviceName();
#endif
}

} // namespace ZeroTier
