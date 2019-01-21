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
#include "../node/MulticastGroup.hpp"
#include "../node/InetAddress.hpp"
#include "../osdep/Thread.hpp"

namespace ZeroTier {

class WindowsEthernetTap
{
public:
	/**
	 * Installs a new instance of the ZT tap driver
	 *
	 * @param pathToInf Path to zttap driver .inf file
	 * @param deviceInstanceId Buffer to fill with device instance ID on success (and if SetupDiGetDeviceInstanceIdA succeeds, which it should)
	 * @return Empty string on success, otherwise an error message
	 */
	static std::string addNewPersistentTapDevice(const char *pathToInf,std::string &deviceInstanceId);

	/**
	 * Uninstalls all persistent tap devices that have legacy drivers
	 *
	 * @return Empty string on success, otherwise an error message
	 */
	static std::string destroyAllLegacyPersistentTapDevices();

	/**
	 * Uninstalls all persistent tap devices on the system
	 *
	 * @return Empty string on success, otherwise an error message
	 */
	static std::string destroyAllPersistentTapDevices();

	/**
	 * Uninstalls a specific persistent tap device by instance ID
	 *
	 * @param instanceId Device instance ID
	 * @return Empty string on success, otherwise an error message
	 */
	static std::string deletePersistentTapDevice(const char *instanceId);

	/**
	 * Disable a persistent tap device by instance ID
	 *
	 * @param instanceId Device instance ID
	 * @param enabled Enable device?
	 * @return True if device was found and disabled
	 */
	static bool setPersistentTapDeviceState(const char *instanceId,bool enabled);

	WindowsEthernetTap(
		const char *hp,
		const MAC &mac,
		unsigned int mtu,
		unsigned int metric,
		uint64_t nwid,
		const char *friendlyName,
		void (*handler)(void *,void *,uint64_t,const MAC &,const MAC &,unsigned int,unsigned int,const void *,unsigned int),
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
	void setMtu(unsigned int mtu);

	inline const NET_LUID &luid() const { return _deviceLuid; }
	inline const GUID &guid() const { return _deviceGuid; }
	inline const std::string &instanceId() const { return _deviceInstanceId; }
	NET_IFINDEX interfaceIndex() const;

	void threadMain()
		throw();

    bool isInitialized() const { return _initialized; };

private:
	NET_IFINDEX _getDeviceIndex(); // throws on failure
	std::vector<std::string> _getRegistryIPv4Value(const char *regKey);
	void _setRegistryIPv4Value(const char *regKey,const std::vector<std::string> &value);
	void _syncIps();

	void (*_handler)(void *,void *,uint64_t,const MAC &,const MAC &,unsigned int,unsigned int,const void *,unsigned int);
	void *_arg;
	MAC _mac;
	uint64_t _nwid;
	volatile unsigned int _mtu;
	Thread _thread;

	volatile HANDLE _tap;
	HANDLE _injectSemaphore;

	GUID _deviceGuid;
	NET_LUID _deviceLuid;
	std::string _netCfgInstanceId;
	std::string _deviceInstanceId;
	std::string _mySubkeyName;

	std::string _friendlyName;

	std::vector<InetAddress> _assignedIps; // IPs assigned with addIp
	Mutex _assignedIps_m;

	std::vector<MulticastGroup> _multicastGroups;

	struct _InjectPending
	{
		unsigned int len;
		char data[ZT_MAX_MTU + 32];
	};
	std::queue<_InjectPending> _injectPending;
	Mutex _injectPending_m;

	std::string _pathToHelpers;

	volatile bool _run;
	volatile bool _initialized;
	volatile bool _enabled;
};

} // namespace ZeroTier

#endif
