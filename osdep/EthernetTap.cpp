/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

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
	unsigned int concurrency,
	bool pinning,
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
	return std::shared_ptr<EthernetTap>(new LinuxEthernetTap(homePath,concurrency,pinning,mac,mtu,metric,nwid,friendlyName,handler,arg));
#endif // __LINUX__

#ifdef __WINDOWS__
	HRESULT hres = CoInitializeEx(0, COINIT_MULTITHREADED);
	if (FAILED(hres)) {
		throw std::runtime_error("WinEthernetTap: COM initialization failed");
	}

	static bool _comInit = false;
	static Mutex _comInit_m;

	{
		Mutex::Lock l(_comInit_m);
		if (!_comInit) {
			hres = CoInitializeSecurity(
				NULL,
				-1,
				NULL,
				NULL,
				RPC_C_AUTHN_LEVEL_PKT,
				RPC_C_IMP_LEVEL_IMPERSONATE,
				NULL,
				EOAC_NONE,
				NULL
			);
			if (FAILED(hres)) {
				CoUninitialize();
				fprintf(stderr, "WinEthernetTap: Failed to initialize security");
				throw std::runtime_error("WinEthernetTap: Failed to initialize security");
			}
			_comInit = true;
		}
	}
	return std::shared_ptr<EthernetTap>(new WindowsEthernetTap(homePath,mac,mtu,metric,nwid,friendlyName,handler,arg));
#endif // __WINDOWS__

#ifdef __FreeBSD__
	return std::shared_ptr<EthernetTap>(new BSDEthernetTap(homePath,concurrency,pinning,mac,mtu,metric,nwid,friendlyName,handler,arg));
#endif // __FreeBSD__

#ifdef __NetBSD__
	return std::shared_ptr<EthernetTap>(new NetBSDEthernetTap(homePath,mac,mtu,metric,nwid,friendlyName,handler,arg));
#endif // __NetBSD__

#ifdef __OpenBSD__
	return std::shared_ptr<EthernetTap>(new BSDEthernetTap(homePath,concurrency,mac,mtu,metric,nwid,friendlyName,handler,arg));
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

std::string EthernetTap::friendlyName() const
{
	// Most platforms do not have this.
	return std::string();
}

} // namespace ZeroTier
