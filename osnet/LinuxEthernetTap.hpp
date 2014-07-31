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

#ifndef ZT_LINUXETHERNETTAP_HPP
#define ZT_LINUXETHERNETTAP_HPP

#include <stdio.h>
#include <stdlib.h>

#include <stdexcept>

#include "../node/EthernetTap.hpp"
#include "../node/Thread.hpp"

namespace ZeroTier {

/**
 * Linux Ethernet tap using kernel tun/tap driver
 */
class LinuxEthernetTap : public EthernetTap
{
public:
	LinuxEthernetTap(
		const MAC &mac,
		unsigned int mtu,
		unsigned int metric,
		uint64_t nwid,
		const char *desiredDevice,
		const char *friendlyName,
		void (*handler)(void *,const MAC &,const MAC &,unsigned int,const Buffer<4096> &),
		void *arg);

	virtual ~LinuxEthernetTap();

	virtual void setEnabled(bool en);
	virtual bool enabled() const;
	virtual bool addIP(const InetAddress &ip);
	virtual bool removeIP(const InetAddress &ip);
	virtual std::set<InetAddress> ips() const;
	virtual void put(const MAC &from,const MAC &to,unsigned int etherType,const void *data,unsigned int len);
	virtual std::string deviceName() const;
	virtual void setFriendlyName(const char *friendlyName);
	virtual bool updateMulticastGroups(std::set<MulticastGroup> &groups);

	void threadMain()
		throw();

private:
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
