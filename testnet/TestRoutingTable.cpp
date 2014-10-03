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

#include "TestRoutingTable.hpp"

namespace ZeroTier {

TestRoutingTable::TestRoutingTable()
{
}

TestRoutingTable::~TestRoutingTable()
{
}

std::vector<RoutingTable::Entry> TestRoutingTable::get(bool includeLinkLocal,bool includeLoopback) const
{
	return std::vector<RoutingTable::Entry>();
}

RoutingTable::Entry TestRoutingTable::set(const InetAddress &destination,const InetAddress &gateway,const char *device,int metric)
{
	return RoutingTable::Entry();
}

} // namespace ZeroTier
