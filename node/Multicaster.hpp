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

#ifndef ZT_MULTICASTER_HPP
#define ZT_MULTICASTER_HPP

#include <stdint.h>
#include <string.h>

#include <map>
#include <vector>
#include <list>

#include "Constants.hpp"
#include "Address.hpp"
#include "MAC.hpp"
#include "MulticastGroup.hpp"
#include "OutboundMulticast.hpp"
#include "Switch.hpp"
#include "Utils.hpp"
#include "Mutex.hpp"

namespace ZeroTier {

class Topology;

/**
 * Database of known multicast peers within a network
 */
class Multicaster
{
private:
	struct MulticastGroupMember
	{
		MulticastGroupMember() {}
		MulticastGroupMember(const Address &a,const Address &lf,uint64_t ts) : address(a),learnedFrom(lf),timestamp(ts) {}

		Address address;
		Address learnedFrom; // NULL/0 for addresses directly learned from LIKE
		uint64_t timestamp; // time of last LIKE or OK response to MULTICAST_LONELY
		uint64_t rank; // used by sorting algorithm in clean()

		// for sorting in ascending order of rank
		inline bool operator<(const MulticastGroupMember &m) const throw() { return (rank < m.rank); }
	};

	struct MulticastGroupStatus
	{
		MulticastGroupStatus() : lastGatheredMembers(0) {}

		uint64_t lastGatheredMembers; // time we last gathered members
		std::vector<MulticastGroupMember> members; // members of this group
		std::list<OutboundMulticast> txQueue; // pending outbound multicasts
	};

public:
	Multicaster();
	~Multicaster();

	/**
	 * Add or update a member in a multicast group
	 *
	 * @param mg Multicast group
	 * @param learnedFrom Address from which we learned this member or NULL/0 Address if direct
	 * @param member New member address
	 */
	void add(const MulticastGroup &mg,const Address &learnedFrom,const Address &member);

	/**
	 * Erase a member from a multicast group (if present)
	 *
	 * @param mg Multicast group
	 * @param member Member to erase
	 */
	void erase(const MulticastGroup &mg,const Address &member);

	/**
	 * Send a multicast
	 *
	 * @param nwid Network ID
	 * @param now Current time
	 * @param sw Switch to use for sending packets
	 * @param self This node's address
	 * @param mg Multicast group
	 * @param from Source Ethernet MAC address
	 * @param etherType Ethernet frame type
	 * @param data Packet data
	 * @param len Length of packet data
	 */
	void send(uint64_t nwid,uint64_t now,const Switch &sw,const Address &self,const MulticastGroup &mg,const MAC &from,unsigned int etherType,const void *data,unsigned int len);

	/**
	 * @param mg Multicast group
	 * @return Tuple of: time we last gathered members (or 0 for never) and number of known members
	 */
	inline std::pair<uint64_t,unsigned int> groupStatus(const MulticastGroup &mg) const
	{
		Mutex::Lock _l(_groups_m);
		std::map< MulticastGroup,MulticastGroupStatus >::const_iterator r(_groups.find(mg));
		return ((r != _groups.end()) ? std::pair<uint64_t,unsigned int>(r->second.lastGatheredMembers,r->second.members.size()) : std::pair<uint64_t,unsigned int>(0,0));
	}

	/**
	 * Return the number of new members we should want to gather or 0 for none
	 *
	 * @param mg Multicast group
	 * @param now Current time
	 * @param limit The maximum number we want per multicast group on this network
	 * @param updateLastGatheredTimeOnNonzeroReturn If true, reset group's last gathered time to 'now' on non-zero return
	 */
	unsigned int shouldGather(const MulticastGroup &mg,uint64_t now,unsigned int limit,bool updateLastGatheredTimeOnNonzeroReturn);

	/**
	 * Update last gathered members time for a group
	 *
	 * @param mg Multicast group
	 * @param now Current time
	 */
	inline void gatheringMembersNow(const MulticastGroup &mg,uint64_t now)
	{
		Mutex::Lock _l(_groups_m);
		_groups[mg].lastGatheredMembers = now;
	}

	/**
	 * Clean up and resort database
	 *
	 * @param now Current time
	 * @param topology Global peer topology
	 * @param trim Trim lists to a maximum of this many members per multicast group
	 */
	void clean(uint64_t now,const Topology &topology);

private:
	std::map< MulticastGroup,MulticastGroupStatus > _groups;
	Mutex _groups_m;
};

} // namespace ZeroTier

#endif
