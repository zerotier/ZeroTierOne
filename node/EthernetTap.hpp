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

#ifndef ZT_ETHERNETTAP_HPP
#define ZT_ETHERNETTAP_HPP

#include <stdio.h>
#include <stdlib.h>

#include <map>
#include <list>
#include <vector>
#include <set>
#include <string>
#include <queue>
#include <stdexcept>

#include "Constants.hpp"
#include "InetAddress.hpp"
#include "MAC.hpp"
#include "Mutex.hpp"
#include "Condition.hpp"
#include "MulticastGroup.hpp"
#include "Thread.hpp"
#include "Buffer.hpp"
#include "Array.hpp"

#ifdef __WINDOWS__
#include <WinSock2.h>
#include <Windows.h>
#endif

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * System ethernet tap device
 */
class EthernetTap
{
public:
	/**
	 * Construct a new TAP device
	 *
	 * Handler arguments: arg,from,to,etherType,data
	 * 
	 * @param renv Runtime environment
	 * @param tag A tag used to identify persistent taps at the OS layer (e.g. nwid in hex)
	 * @param mac MAC address of device
	 * @param mtu MTU of device
	 * @param desc If non-NULL, a description (not used on all OSes)
	 * @param handler Handler function to be called when data is received from the tap
	 * @param arg First argument to handler function
	 * @throws std::runtime_error Unable to allocate device
	 */
	EthernetTap(
		const RuntimeEnvironment *renv,
		const char *tag,
		const MAC &mac,
		unsigned int mtu,
		void (*handler)(void *,const MAC &,const MAC &,unsigned int,const Buffer<4096> &),
		void *arg)
		throw(std::runtime_error);

	/**
	 * Close tap and shut down thread
	 *
	 * This may block for a few seconds while thread exits.
	 */
	~EthernetTap();

	/**
	 * Set the user display name for this connection
	 *
	 * This does nothing on platforms that don't have this concept.
	 *
	 * @param dn User display name
	 */
	void setDisplayName(const char *dn);

	/**
	 * @return MAC address of this interface
	 */
	inline const MAC &mac() const throw() { return _mac; }

	/**
	 * @return MTU of this interface
	 */
	inline unsigned int mtu() const throw() { return _mtu; }

	/**
	 * Add an IP to this interface
	 *
	 * @param ip IP and netmask (netmask stored in port field)
	 * @return True if IP added successfully
	 */
	bool addIP(const InetAddress &ip);

	/**
	 * Remove an IP from this interface
	 *
	 * Link-local IP addresses may not be able to be removed, depending on platform and type.
	 *
	 * @param ip IP and netmask (netmask stored in port field)
	 * @return True if IP removed successfully
	 */
	bool removeIP(const InetAddress &ip);

	/**
	 * @return All IP addresses (V4 and V6) assigned to this interface (including link-local)
	 */
	std::set<InetAddress> ips() const;

	/**
	 * Set this tap's IP addresses to exactly this set of IPs
	 *
	 * New IPs are created, ones not in this list are removed.
	 *
	 * @param ips IP addresses with netmask in port field
	 */
	inline void setIps(const std::set<InetAddress> &allIps)
	{
		for(std::set<InetAddress>::iterator i(allIps.begin());i!=allIps.end();++i)
			addIP(*i);
		std::set<InetAddress> myIps(ips());
#ifdef __APPLE__
		bool haveV6LinkLocal = false;
		for(std::set<InetAddress>::iterator i(myIps.begin());i!=myIps.end();++i) {
			if (i->isLinkLocal()) {
				if (i->isV6())
					haveV6LinkLocal = true;
			} else if (!allIps.count(*i))
				removeIP(*i);
		}
		if (!haveV6LinkLocal)
			addIP(InetAddress::makeIpv6LinkLocal(_mac));
#else
		for(std::set<InetAddress>::iterator i(myIps.begin());i!=myIps.end();++i) {
			if ((!i->isLinkLocal())&&(!allIps.count(*i)))
				removeIP(*i);
		}
#endif
	}

	/**
	 * Put a frame, making it available to the OS for processing
	 *
	 * @param from MAC address from which frame originated
	 * @param to MAC address of destination (typically MAC of tap itself)
	 * @param etherType Ethernet protocol ID
	 * @param data Frame payload
	 * @param len Length of frame
	 */
	void put(const MAC &from,const MAC &to,unsigned int etherType,const void *data,unsigned int len);

	/**
	 * @return OS-specific device or connection name
	 */
	std::string deviceName() const;

	/**
	 * @return OS-internal persistent device ID or empty string if not applicable to this platform or not persistent
	 */
	std::string persistentId() const;

	/**
	 * Fill or modify a set to contain multicast groups for this device
	 *
	 * This populates a set or, if already populated, modifies it to contain
	 * only multicast groups in which this device is interested.
	 *
	 * This should always include the blind wildcard MulticastGroup (MAC of
	 * ff:ff:ff:ff:ff:ff and 0 ADI field).
	 *
	 * @param groups Set to modify in place
	 * @return True if set was changed since last call
	 */
	bool updateMulticastGroups(std::set<MulticastGroup> &groups);

	/**
	 * Thread main method; do not call elsewhere
	 */
	void threadMain()
		throw();

	/**
	 * Remove persistent tap device by device name
	 *
	 * This has no effect on platforms that do not have persistent taps.
	 * On platforms like Windows with persistent devices the device is
	 * uninstalled.
	 *
	 * @param _r Runtime environment
	 * @param pdev Device name as returned by persistentId() while tap is running
	 * @return True if a device was deleted
	 */
	static bool deletePersistentTapDevice(const RuntimeEnvironment *_r,const char *pid);

private:
	const MAC _mac;
	const unsigned int _mtu;

	const RuntimeEnvironment *_r;

	void (*_handler)(void *,const MAC &,const MAC &,unsigned int,const Buffer<4096> &);
	void *_arg;

	Thread _thread;

#ifdef __UNIX_LIKE__
	char _dev[16];
	int _fd;
	int _shutdownSignalPipe[2];
#endif

#ifdef __WINDOWS__
	void _syncIpsWithRegistry(const std::set<InetAddress> &haveIps);

	HANDLE _tap;
	OVERLAPPED _tapOvlRead,_tapOvlWrite;
	char _tapReadBuf[ZT_IF_MTU + 32];
	HANDLE _injectSemaphore;
	GUID _deviceGuid;
	std::string _myDeviceInstanceId; // NetCfgInstanceId, a GUID
	std::string _myDeviceInstanceIdPath; // DeviceInstanceID, another kind of "instance ID"
	std::queue< std::pair< Array<char,ZT_IF_MTU + 32>,unsigned int > > _injectPending;
	Mutex _injectPending_m;
	volatile bool _run;
	volatile bool _initialized;
#endif
};

} // namespace ZeroTier

#endif
