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
#include "../node/EthernetTap.hpp"
#include "../node/Mutex.hpp"
#include "../node/Thread.hpp"
#include "../node/Array.hpp"
#include "../node/MulticastGroup.hpp"

namespace ZeroTier {

class WindowsEthernetTap : public EthernetTap
{
public:
	WindowsEthernetTap(
		const char *pathToHelpers,
		const MAC &mac,
		unsigned int mtu,
		unsigned int metric,
		uint64_t nwid,
		const char *desiredDevice,
		const char *friendlyName,
		void (*handler)(void *,const MAC &,const MAC &,unsigned int,const Buffer<4096> &),
		void *arg);

	virtual ~WindowsEthernetTap();

	virtual void setEnabled(bool en);
	virtual bool enabled() const;
	virtual bool addIP(const InetAddress &ip);
	virtual bool removeIP(const InetAddress &ip);
	virtual std::set<InetAddress> ips() const;
	virtual void put(const MAC &from,const MAC &to,unsigned int etherType,const void *data,unsigned int len);
	virtual std::string deviceName() const;
	virtual void setFriendlyName(const char *friendlyName);
	virtual bool updateMulticastGroups(std::set<MulticastGroup> &groups);
	virtual bool injectPacketFromHost(const MAC &from,const MAC &to,unsigned int etherType,const void *data,unsigned int len);

	inline const NET_LUID &luid() const { return _deviceLuid; }
	inline const GUID &guid() const { return _deviceGuid; }
	inline const std::string &instanceId() const { return _deviceInstanceId; }

	void threadMain()
		throw();

private:
	bool _disableTapDevice();
	bool _enableTapDevice();
	NET_IFINDEX _getDeviceIndex(); // throws on failure
	std::vector<std::string> _getRegistryIPv4Value(const char *regKey);
	void _setRegistryIPv4Value(const char *regKey,const std::vector<std::string> &value);

	void (*_handler)(void *,const MAC &,const MAC &,unsigned int,const Buffer<4096> &);
	void *_arg;
	uint64_t _nwid;
	Thread _thread;

	volatile HANDLE _tap;
	HANDLE _injectSemaphore;

	GUID _deviceGuid;
	NET_LUID _deviceLuid;
	std::string _netCfgInstanceId; // NetCfgInstanceId, a GUID
	std::string _deviceInstanceId; // DeviceInstanceID, another kind of "instance ID"

	std::queue< std::pair< Array<char,ZT_IF_MTU + 32>,unsigned int > > _injectPending;
	Mutex _injectPending_m;

	std::string _pathToHelpers;

	volatile bool _run;
	volatile bool _initialized;
	volatile bool _enabled;
};

} // namespace ZeroTier

#endif
