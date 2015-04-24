/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2015  ZeroTier, Inc.
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

#ifndef ZT_WINDOWSETHERNETTAP_HPP
#define ZT_WINDOWSETHERNETTAP_HPP

#include <stdio.h>
#include <stdlib.h>

#include <ifdef.h>

#include <string>
#include <queue>
#include <stdexcept>

#include "../node/Constants.hpp"
#include "../node/Mutex.hpp"
#include "../node/Array.hpp"
#include "../node/MulticastGroup.hpp"
#include "../osdep/Thread.hpp"

namespace ZeroTier {

class WindowsEthernetTap
{
public:
	WindowsEthernetTap(
		const char *hp,
		const MAC &mac,
		unsigned int mtu,
		unsigned int metric,
		uint64_t nwid,
		const char *friendlyName,
		void (*handler)(void *,uint64_t,const MAC &,const MAC &,unsigned int,unsigned int,const void *,unsigned int),
		void *arg);

	~WindowsEthernetTap();

	void setEnabled(bool en);
	bool enabled() const;
	bool addIp(const InetAddress &ip);
	bool removeIp(const InetAddress &ip);
	std::vector<InetAddress> ips() const;
	void put(const MAC &from,const MAC &to,unsigned int etherType,const void *data,unsigned int len);
	std::string deviceName() const;
	void setFriendlyName(const char *friendlyName);
	void scanMulticastGroups(std::vector<MulticastGroup> &added,std::vector<MulticastGroup> &removed);

	inline const NET_LUID &luid() const { return _deviceLuid; }
	inline const GUID &guid() const { return _deviceGuid; }
	inline const std::string &instanceId() const { return _deviceInstanceId; }

	void threadMain()
		throw();

	static void destroyAllPersistentTapDevices(const char *pathToHelpers);

private:
	bool _disableTapDevice();
	bool _enableTapDevice();
	NET_IFINDEX _getDeviceIndex(); // throws on failure
	std::vector<std::string> _getRegistryIPv4Value(const char *regKey);
	void _setRegistryIPv4Value(const char *regKey,const std::vector<std::string> &value);
	static void _deletePersistentTapDevice(const char *pathToHelpers,const char *instanceId);

	void (*_handler)(void *,uint64_t,const MAC &,const MAC &,unsigned int,unsigned int,const void *,unsigned int);
	void *_arg;
	MAC _mac;
	uint64_t _nwid;
	Thread _thread;

	volatile HANDLE _tap;
	HANDLE _injectSemaphore;

	GUID _deviceGuid;
	NET_LUID _deviceLuid;
	std::string _netCfgInstanceId; // NetCfgInstanceId, a GUID
	std::string _deviceInstanceId; // DeviceInstanceID, another kind of "instance ID"

	std::vector<MulticastGroup> _multicastGroups;

	std::queue< std::pair< Array<char,ZT_IF_MTU + 32>,unsigned int > > _injectPending;
	Mutex _injectPending_m;

	std::string _pathToHelpers;

	volatile bool _run;
	volatile bool _initialized;
	volatile bool _enabled;
};

} // namespace ZeroTier

#endif
