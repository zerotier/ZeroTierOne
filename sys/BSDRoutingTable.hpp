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

#ifndef ZT_BSDROUTINGTABLE_HPP
#define ZT_BSDROUTINGTABLE_HPP

#include "../RoutingTable.hpp"

namespace ZeroTier {

/**
 * Routing table interface for BSD with sysctl() and BSD /sbin/route
 *
 * Has currently only been tested on OSX/Darwin.
 */
class BSDRoutingTable : public RoutingTable
{
public:
	BSDRoutingTable();
	virtual ~BSDRoutingTable();
	virtual std::vector<RoutingTable::Entry> get(bool includeLinkLocal = false,bool includeLoopback = false) const;
	virtual RoutingTable::Entry set(const InetAddress &destination,const InetAddress &gateway,const char *device,int metric);
};

} // namespace ZeroTier

#endif
