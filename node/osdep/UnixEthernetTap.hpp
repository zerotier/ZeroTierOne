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

#ifndef ZT_UNIXETHERNETTAP_HPP
#define ZT_UNIXETHERNETTAP_HPP

#include <stdio.h>
#include <stdlib.h>

#include <stdexcept>

#include "EthernetTap.hpp"
#include "Mutex.hpp"
#include "Thread.hpp"

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * Tap device using Unix taps (contains special case code for Linux, OSX, etc. but they're all similar)
 */
class UnixEthernetTap : public EthernetTap
{
public:
	/**
	 * Construct a new TAP device
	 *
	 * Handler arguments: arg,from,to,etherType,data
	 * 
	 * @param renv Runtime environment
	 * @param tryToGetDevice Unix device name (e.g. zt0)
	 * @param mac MAC address of device
	 * @param mtu MTU of device
	 * @param desc If non-NULL, a description (not used on all OSes)
	 * @param handler Handler function to be called when data is received from the tap
	 * @param arg First argument to handler function
	 * @throws std::runtime_error Unable to allocate tap device
	 */
	UnixEthernetTap(
		const RuntimeEnvironment *renv,
		const char *tryToGetDevice,
		const MAC &mac,
		unsigned int mtu,
		void (*handler)(void *,const MAC &,const MAC &,unsigned int,const Buffer<4096> &),
		void *arg)
		throw(std::runtime_error);

	virtual ~UnixEthernetTap();

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

private:
	const RuntimeEnvironment *_r;
	void (*_handler)(void *,const MAC &,const MAC &,unsigned int,const Buffer<4096> &);
	void *_arg;
	Thread _thread;
	std::string _dev;
	int _fd;
	int _shutdownSignalPipe[2];
	volatile bool _enabled;
};

} // namespace ZeroTier

#endif
