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

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <vector>
#include <string>

#include "InetAddress.hpp"
#include "NonCopyable.hpp"
#include "Utils.hpp"

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
		InetAddress gateway;
		char device[128];
		int metric;

		inline bool operator==(const Entry &re) const { return ((destination == re.destination)&&(gateway == re.gateway)&&(strcmp(device == re.device) == 0)&&(metric == re.metric)); }
		inline bool operator!=(const Entry &re) const { return (!(*this == re)); }
		inline bool operator<(const Entry &re) const
		{
			if (destination < re.destination)
				return true;
			if (destination == re.destination) {
				if (gateway < re.gateway)
					return true;
				if (gateway == re.gateway) {
					int tmp = (int)::strcmp(device,re.device);
					if (tmp < 0)
						return true;
					if (tmp == 0)
						return (metric < re.metric);
				}
			}
			return false;
		}
		inline bool operator>(const Entry &re) const { return (re < *this); }
		inline bool operator<=(const Entry &re) const { return (!(re < *this)); }
		inline bool operator>=(const Entry &re) const { return (!(*this < re)); }
	};

	SystemNetworkStack() {}
	virtual ~SystemNetworkStack() {}

	/**
	 * @return All routing table entries sorted in order of destination address / netmask
	 */
	virtual std::vector<Entry> routingTable() const = 0;

	/**
	 * Add or update a routing table entry
	 *
	 * @param re Entry to add/update
	 * @return True if successful
	 */
	virtual bool addUpdateRoute(const Entry &re) = 0;

	/**
	 * Compute a 64-bit value that hashes the current state of the network environment
	 *
	 * @param ignoreInterfaces Names of interfaces to exclude from fingerprint (e.g. my own)
	 * @return Integer CRC-type fingerprint of current network environment
	 */
	inline uint64_t networkEnvironmentFingerprint(const std::vector<std::string> &ignoreInterfaces) const
	{
		uint64_t fp = 0;
		std::vector<Entry> rtab(routingTable());
		for(std::vector<Entry>::const_iterator re(rtab.begin());re!=rtab.end();++re) {
			bool skip = false;
			for(std::vector<std::string>::const_iterator ii(ignoreInterfaces.begin());ii!=ignoreInterfaces.end();++ii) {
				if (*ii == re->interface.device) {
					skip = true;
					break;
				}
			}
			if (skip)
				continue;
			++fp;
			if (re->destination.isV4()) {
				fp = Utils::sdbmHash(re->destination.rawIpData(),4,fp);
				fp = Utils::sdbmHash((uint16_t)re->destination.netmaskBits(),fp);
			} else if (re->destination.isV6()) {
				fp = Utils::sdbmHash(re->destination.rawIpData(),16,fp);
				fp = Utils::sdbmHash((uint16_t)re->destination.netmaskBits(),fp);
			}
			if (re->gateway.isV4()) {
				fp = Utils::sdbmHash(re->gateway.rawIpData(),4,fp);
				fp = Utils::sdbmHash((uint16_t)re->gateway.netmaskBits(),fp);
			} else if (re->gateway.isV6()) {
				fp = Utils::sdbmHash(re->gateway.rawIpData(),16,fp);
				fp = Utils::sdbmHash((uint16_t)re->gateway.netmaskBits(),fp);
			}
			fp = Utils::sdbmHash(re->device,fp);
			fp = Utils::sdbmHash((uint32_t)re->metric,fp);
		}
		return fp;
	}
};

} // namespace ZeroTier

#endif
