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

#ifndef ZT_WINDOWSETHERNETTAP_HPP
#define ZT_WINDOWSETHERNETTAP_HPP

#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <queue>
#include <stdexcept>

#include "../Constants.hpp"
#include "../EthernetTap.hpp"
#include "../Mutex.hpp"
#include "../Thread.hpp"
#include "../Array.hpp"

namespace ZeroTier {

/**
 * Windows Ethernet tap device using bundled ztTap driver
 */
class WindowsEthernetTap : public EthernetTap
{
public:
	/**
	 * Open tap device, installing and creating one if it does not exist
	 * 
	 * @param renv Runtime environment
	 * @param tag A tag (presently the hex network ID) used to identify persistent tap devices in the registry
	 * @param mac MAC address of device
	 * @param mtu MTU of device
	 * @param desc If non-NULL, a description (not used on all OSes)
	 * @param handler Handler function to be called when data is received from the tap
	 * @param arg First argument to handler function
	 * @throws std::runtime_error Unable to allocate device
	 */
	WindowsEthernetTap(
		const RuntimeEnvironment *renv,
		const char *tag,
		const MAC &mac,
		unsigned int mtu,
		void (*handler)(void *,const MAC &,const MAC &,unsigned int,const Buffer<4096> &),
		void *arg)
		throw(std::runtime_error);

	virtual ~WindowsEthernetTap();

	virtual void setEnabled(bool en);
	virtual bool enabled() const;
	virtual void setDisplayName(const char *dn);
	virtual bool addIP(const InetAddress &ip);
	virtual bool removeIP(const InetAddress &ip);
	virtual std::set<InetAddress> ips() const;
	virtual void put(const MAC &from,const MAC &to,unsigned int etherType,const void *data,unsigned int len);
	virtual std::string deviceName() const;
	virtual std::string persistentId() const;
	virtual bool updateMulticastGroups(std::set<MulticastGroup> &groups);

	/**
	 * Thread main method; do not call elsewhere
	 */
	void threadMain()
		throw();

	/**
	 * Remove persistent tap device by device name
	 *
	 * @param _r Runtime environment
	 * @param pdev Device name as returned by persistentId() while tap is running
	 * @return True if a device was deleted
	 */
	static bool deletePersistentTapDevice(const RuntimeEnvironment *_r,const char *pid);

	/**
	 * Clean persistent tap devices that are not in the supplied set
	 *
	 * @param _r Runtime environment
	 * @param exceptThese Devices to leave in place
	 * @param alsoRemoveUnassociatedDevices If true, remove devices not associated with any network as well
	 * @return Number of devices deleted or -1 if an error prevented the operation from being performed
	 */
	static int cleanPersistentTapDevices(const RuntimeEnvironment *_r,const std::set<std::string> &exceptThese,bool alsoRemoveUnassociatedDevices);

private:
	void (*_handler)(void *,const MAC &,const MAC &,unsigned int,const Buffer<4096> &);
	void *_arg;
	Thread _thread;

	volatile HANDLE _tap;
	HANDLE _injectSemaphore;
	GUID _deviceGuid;
	std::string _netCfgInstanceId; // NetCfgInstanceId, a GUID
	std::string _deviceInstanceId; // DeviceInstanceID, another kind of "instance ID"
	std::queue< std::pair< Array<char,ZT_IF_MTU + 32>,unsigned int > > _injectPending;
	Mutex _injectPending_m;
	volatile bool _run;
	volatile bool _initialized;
	volatile bool _enabled;
};

} // namespace ZeroTier

#endif
