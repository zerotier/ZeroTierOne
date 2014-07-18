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

#ifndef ZT_ROUTINGTABLE_HPP
#define ZT_ROUTINGTABLE_HPP

#include <vector>
#include <string>

#include "InetAddress.hpp"
#include "NonCopyable.hpp"

namespace ZeroTier {

/**
 * Base class for OS routing table interfaces
 */
class RoutingTable : NonCopyable
{
public:
	class Entry
	{
	public:
		Entry() throw() { device[0] = (char)0; }

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
		 * System device name
		 */
		char device[128];

		/**
		 * @return Human-readable representation of this route
		 */
		std::string toString() const;

		/**
		 * @return True if at least one required field is present (object is not null)
		 */
		inline operator bool() const { return ((destination)||(gateway)||(device[0])); }

		bool operator==(const Entry &re) const;
		inline bool operator!=(const Entry &re) const { return (!(*this == re)); }
		bool operator<(const Entry &re) const;
		inline bool operator>(const Entry &re) const { return (re < *this); }
		inline bool operator<=(const Entry &re) const { return (!(re < *this)); }
		inline bool operator>=(const Entry &re) const { return (!(*this < re)); }
	};

	RoutingTable();
	virtual ~RoutingTable();

	/**
	 * Get routing table
	 *
	 * @param includeLinkLocal If true, include link-local address routes (default: false)
	 * @param includeLoopback Include loopback (default: false)
	 * @return Sorted routing table entries
	 */
	virtual std::vector<RoutingTable::Entry> get(bool includeLinkLocal = false,bool includeLoopback = false) const = 0;

	/**
	 * Add or update a routing table entry
	 *
	 * If there is no change, the existing entry is returned. Use a value of -1
	 * for metric to delete a route.
	 *
	 * @param destination Destination IP/netmask
	 * @param gateway Gateway IP (netmask/port part unused) or NULL/zero for device-level route
	 * @param device Device name (can be null for gateway routes)
	 * @param metric Route metric or hop count (higher = lower priority) or negative to delete
	 * @return Entry or null entry on failure (or delete)
	 */
	virtual RoutingTable::Entry set(const InetAddress &destination,const InetAddress &gateway,const char *device,int metric) = 0;

	/**
	 * Compute a 64-bit value that hashes the current state of the network environment
	 *
	 * This shouldn't be overridden -- uses get() to get underlying routing table.
	 *
	 * @param ignoreInterfaces Names of interfaces to exclude from fingerprint (e.g. my own)
	 * @return Integer CRC-type fingerprint of current network environment
	 */
	uint64_t networkEnvironmentFingerprint(const std::vector<std::string> &ignoreInterfaces) const;
};

} // namespace ZeroTier

#endif
