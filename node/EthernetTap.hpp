/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2012-2013  ZeroTier Networks LLC
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

#ifndef _ZT_ETHERNETTAP_HPP
#define _ZT_ETHERNETTAP_HPP

#include <stdio.h>
#include <stdlib.h>
#include <map>
#include <list>
#include <vector>
#include <set>
#include <string>
#include <stdexcept>
#include "Array.hpp"
#include "Utils.hpp"
#include "InetAddress.hpp"
#include "NonCopyable.hpp"
#include "MAC.hpp"
#include "Constants.hpp"
#include "Mutex.hpp"
#include "MulticastGroup.hpp"

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * System ethernet tap device
 */
class EthernetTap : NonCopyable
{
public:
	/**
	 * Construct a new TAP device
	 *
	 * @param renv Runtime environment
	 * @param mac MAC address of device
	 * @param mtu MTU of device
	 * @throws std::runtime_error Unable to allocate device
	 */
	EthernetTap(const RuntimeEnvironment *renv,const MAC &mac,unsigned int mtu)
		throw(std::runtime_error);

	~EthernetTap();

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
	 * @param ip IP and netmask (netmask stored in port field)
	 * @return True if IP removed successfully
	 */
	bool removeIP(const InetAddress &ip);

	/**
	 * @return Set of IP addresses / netmasks
	 */
	inline std::set<InetAddress> ips() const
	{
		Mutex::Lock _l(_ips_m);
		return _ips;
	}

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
		for(std::set<InetAddress>::iterator i(myIps.begin());i!=myIps.end();++i) {
			if (!allIps.count(*i))
				removeIP(*i);
		}
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
	 * Get the next packet from the interface, blocking if none is available.
	 *
	 * @param from Filled with MAC address of source (normally our own)
	 * @param to Filled with MAC address of destination
	 * @param etherType Filled with Ethernet frame type
	 * @param buf Buffer to fill (must have room for MTU bytes)
	 * @return Number of bytes read or 0 if none
	 */
	unsigned int get(MAC &from,MAC &to,unsigned int &etherType,void *buf);

	/**
	 * @return OS-specific device or connection name
	 */
	std::string deviceName();

	/**
	 * @return True if tap is open
	 */
	bool open() const;

	/**
	 * Close this tap, invalidating the object and causing get() to abort
	 */
	void close();

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

private:
	const MAC _mac;
	const unsigned int _mtu;

	const RuntimeEnvironment *_r;

	std::set<InetAddress> _ips;
	Mutex _ips_m;

#if defined(__APPLE__) || defined(__linux__) || defined(linux) || defined(__LINUX__) || defined(__linux)

	char _dev[16];
	unsigned char *_putBuf;
	unsigned char *_getBuf;
	int _fd;

	bool _isReading;
	pthread_t _isReadingThreadId;
	Mutex _isReading_m;

#elif defined(_WIN32) /* -------------------------------------------------- */

#endif /* ----------------------------------------------------------------- */
};

} // namespace ZeroTier

#endif
