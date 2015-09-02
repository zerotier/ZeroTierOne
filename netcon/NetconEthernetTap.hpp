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

#ifndef ZT_NETCONETHERNETTAP_HPP
#define ZT_NETCONETHERNETTAP_HPP

#ifdef ZT_ENABLE_NETCON

#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <vector>
#include <stdexcept>

#include "../node/Constants.hpp"
#include "../node/MulticastGroup.hpp"
#include "../node/Mutex.hpp"
#include "../osdep/Thread.hpp"
#include "../osdep/Phy.hpp"

namespace ZeroTier {

/**
 * Network Containers instance -- emulates an Ethernet tap device as far as OneService knows
 */
class NetconEthernetTap
{
	friend class Phy<NetconEthernetTap>;

public:
	NetconEthernetTap(
		const char *homePath,
		const MAC &mac,
		unsigned int mtu,
		unsigned int metric,
		uint64_t nwid,
		const char *friendlyName,
		void (*handler)(void *,uint64_t,const MAC &,const MAC &,unsigned int,unsigned int,const void *,unsigned int),
		void *arg);

	~NetconEthernetTap();

	void setEnabled(bool en);
	bool enabled() const;
	bool addIp(const InetAddress &ip);
	bool removeIp(const InetAddress &ip);
	std::vector<InetAddress> ips() const;
	void put(const MAC &from,const MAC &to,unsigned int etherType,const void *data,unsigned int len);
	std::string deviceName() const;
	void setFriendlyName(const char *friendlyName);
	void scanMulticastGroups(std::vector<MulticastGroup> &added,std::vector<MulticastGroup> &removed);

	void threadMain()
		throw();

private:
	void phyOnDatagram(PhySocket *sock,void **uptr,const struct sockaddr *from,void *data,unsigned long len);
  void phyOnTcpConnect(PhySocket *sock,void **uptr,bool success);
  void phyOnTcpAccept(PhySocket *sockL,PhySocket *sockN,void **uptrL,void **uptrN,const struct sockaddr *from);
  void phyOnTcpClose(PhySocket *sock,void **uptr);
  void phyOnTcpData(PhySocket *sock,void **uptr,void *data,unsigned long len);
  void phyOnTcpWritable(PhySocket *sock,void **uptr);
  void phyOnUnixAccept(PhySocket *sockL,PhySocket *sockN,void **uptrL,void **uptrN);
  void phyOnUnixClose(PhySocket *sock,void **uptr);
  void phyOnUnixData(PhySocket *sock,void **uptr,void *data,unsigned long len);
  void phyOnUnixWritable(PhySocket *sock,void **uptr);

	void (*_handler)(void *,uint64_t,const MAC &,const MAC &,unsigned int,unsigned int,const void *,unsigned int);
	void *_arg;

	Phy<NetconEthernetTap> _phy;
	PhySocket *_unixListenSocket;

	uint64_t _nwid;
	Thread _thread;
	std::string _homePath;
	std::string _dev; // path to Unix domain socket
	std::vector<MulticastGroup> _multicastGroups;
	unsigned int _mtu;
	volatile bool _enabled;
	volatile bool _run;
};

} // namespace ZeroTier

#endif // ZT_ENABLE_NETCON

#endif
