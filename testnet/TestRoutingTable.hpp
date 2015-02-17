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

#ifndef ZT_TESTROUTINGTABLE_HPP
#define ZT_TESTROUTINGTABLE_HPP

#include "../node/RoutingTable.hpp"

namespace ZeroTier {

/**
 * Dummy routing table -- right now this just does nothing
 */
class TestRoutingTable : public RoutingTable
{
public:
	TestRoutingTable();
	virtual ~TestRoutingTable();

	virtual std::vector<RoutingTable::Entry> get(bool includeLinkLocal = false,bool includeLoopback = false) const;
	virtual RoutingTable::Entry set(const InetAddress &destination,const InetAddress &gateway,const char *device,int metric);
};

} // namespace ZeroTier

#endif
