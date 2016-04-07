/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2016  ZeroTier, Inc.  https://www.zerotier.com/
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
 */

#ifndef ZT_ROUTINGTABLE_HPP
#define ZT_ROUTINGTABLE_HPP

#include <vector>

#include "../node/Constants.hpp"
#include "../node/InetAddress.hpp"

namespace ZeroTier {

class RoutingTable
{
public:
	struct Entry
	{
		/**
		 * Destination IP and netmask bits (CIDR format)
		 */
		InetAddress destination;

		/**
		 * Gateway or null address if direct link-level route, netmask/port part of InetAddress not used
		 */
		InetAddress gateway;

		/**
		 * System device index or ID (not included in comparison operators, may not be set on all platforms)
		 */
		int deviceIndex;

		/**
		 * Metric or hop count -- higher = lower routing priority
		 */
		int metric;

		/**
		 * Interface scoped route? (always false if not meaningful on this OS)
		 */
		bool ifscope;

		/**
		 * System device name (may be empty if it doesn't exist or isn't important on this OS)
		 */
		char device[128];

		/**
		 * @return True if at least one required field is present (object is not null)
		 */
		inline operator bool() const { return ((destination)||(gateway)); }
	};

	/**
	 * Get routing table
	 *
	 * @param includeLinkLocal Include link-local IPs?
	 * @param includeLoopback Include loopback routes?
	 */
	static std::vector<RoutingTable::Entry> get(bool includeLinkLocal,bool includeLoopback);

	/**
	 * Add or replace a routing table entry
	 *
	 * @param destination Route destination
	 * @param gateway Gateway or null if local
	 * @param device Device name (if applicable)
	 * @param metric Route metric (if applicable)
	 * @param ifscope Interface bound route? If so, device must be set. (only applicable on some OSes)
	 */
	static RoutingTable::Entry set(const InetAddress &destination,const InetAddress &gateway,const char *device,int metric,bool ifscope);
};

} // namespace ZeroTier

#endif
