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

#include "EthernetTap.hpp"
#include "OSUtils.hpp"

#include <stdlib.h>
#include <string.h>

#ifdef ZT_SDK

#include "../controller/EmbeddedNetworkController.hpp"
#include "../node/Node.hpp"
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

} // namespace ZeroTier
