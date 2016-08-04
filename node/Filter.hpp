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

#ifndef ZT_FILTER_HPP
#define ZT_FILTER_HPP

#include <stdint.h>

#include <vector>

#include "Constants.hpp"
#include "../include/ZeroTierOne.h"
#include "Address.hpp"
#include "MAC.hpp"
#include "Tag.hpp"

namespace ZeroTier {

/**
 * Network packet filter for rules engine
 */
class Filter
{
public:
	/**
	 * Apply a list of rules to a packet
	 *
	 * This returns whether or not the packet should be accepted and may also
	 * take other actions for e.g. the TEE and REDIRECT targets.
	 *
	 * @param nwid ZeroTier network ID
	 * @param receiving True if on receiving side, false on sending side
	 * @param ztSource Source ZeroTier address
	 * @param ztDest Destination ZeroTier address
	 * @param macSource Ethernet layer source address
	 * @param macDest Ethernet layer destination address
	 * @param frameData Ethernet frame data
	 * @param frameLen Ethernet frame payload length
	 * @param etherType 16-bit ethernet type ID
	 * @param vlanId 16-bit VLAN ID
	 * @param rules Pointer to array of rules
	 * @param ruleCount Number of rules
	 * @param tags Tags associated with this node on this network
	 * @param tagCount Number of tags
	 * @param sendCopyOfPacketTo Result parameter: if non-NULL send a copy of this packet to another node
	 * @return True if packet should be accepted for send or receive
	 */
	static bool run(
		const uint64_t nwid,
		const bool receiving,
		const Address &ztSource,
		const Address &ztDest,
		const MAC &macSource,
		const MAC &macDest,
		const uint8_t *frameData,
		const unsigned int frameLen,
		const unsigned int etherType,
		const unsigned int vlanId,
		const ZT_VirtualNetworkRule *rules,
		const unsigned int ruleCount,
		const Tag *tags,
		const unsigned int tagCount,
		Address &sendCopyOfPacketTo);
};

} // namespace ZeroTier

#endif
