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

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "Constants.hpp"
#include "RoutingTable.hpp"
#include "Utils.hpp"

namespace ZeroTier {

std::string RoutingTable::Entry::toString() const
{
	char tmp[1024];
	Utils::snprintf(tmp,sizeof(tmp),"%s %s %s %d",destination.toString().c_str(),((gateway) ? gateway.toIpString().c_str() : "<link>"),device,metric);
	return std::string(tmp);
}

bool RoutingTable::Entry::operator==(const Entry &re) const
{
	return ((destination == re.destination)&&(gateway == re.gateway)&&(strcmp(device,re.device) == 0)&&(metric == re.metric));
}

bool RoutingTable::Entry::operator<(const Entry &re) const
{
	if (destination < re.destination)
		return true;
	else if (destination == re.destination) {
		if (gateway < re.gateway)
			return true;
		else if (gateway == re.gateway) {
			int tmp = (int)::strcmp(device,re.device);
			if (tmp < 0)
				return true;
			else if (tmp == 0)
				return (metric < re.metric);
		}
	}
	return false;
}

RoutingTable::RoutingTable()
{
}

RoutingTable::~RoutingTable()
{
}

} // namespace ZeroTier
