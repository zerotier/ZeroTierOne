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
		Entry() { device[0] = (char)0; }

		InetAddress destination;
		InetAddress gateway; // port/netmaskBits field not used, should be 0
		char device[128];
		int metric;

		std::string toString() const;

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
	 * @return Sorted routing table entries
	 */
	virtual std::vector<Entry> get() const = 0;

	/**
	 * Add or update a routing table entry
	 *
	 * @param re Entry to add/update
	 * @return True if change successful (or unchanged)
	 */
	virtual bool set(const Entry &re) = 0;

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
