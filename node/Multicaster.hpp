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
#include "Utils.hpp"
#include "Mutex.hpp"

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * Database of known multicast peers within a network
 */
class Multicaster
{
private:
	struct MulticastGroupMember
	{
		MulticastGroupMember() {}
		MulticastGroupMember(const Address &a,const Address &lf,uint64_t ts) : address(a),learnedFrom(lf),timestamp(ts),rank(0) {}

		Address address;
		Address learnedFrom; // NULL/0 for addresses directly learned from LIKE
		uint64_t timestamp; // time of last LIKE/OK(GATHER)
		uint64_t rank; // used by sorting algorithm in clean()

		// for sorting in ascending order of rank
		inline bool operator<(const MulticastGroupMember &m) const throw() { return (rank < m.rank); }
	};

	struct MulticastGroupStatus
	{
		MulticastGroupStatus() : lastExplicitGather(0) {}

		uint64_t lastExplicitGather; // time we last gathered members explicitly
		std::list<OutboundMulticast> txQueue; // pending outbound multicasts
		std::vector<MulticastGroupMember> members; // members of this group
	};

public:
	Multicaster();
	~Multicaster();

	/**
	 * Add or update a member in a multicast group and send any pending multicasts
	 *
	 * @param RR Runtime environment
	 * @param now Current time
	 * @param mg Multicast group
	 * @param learnedFrom Address from which we learned this member or NULL/0 Address if direct
	 * @param member New member address
	 */
	inline void add(const RuntimeEnvironment *RR,uint64_t now,const MulticastGroup &mg,const Address &learnedFrom,const Address &member)
	{
		Mutex::Lock _l(_groups_m);
		_add(RR,now,_groups[mg],learnedFrom,member);
	}

	/**
	 * Send a multicast
	 *
	 * @param RR Runtime environment
	 * @param nwid Network ID
	 * @param limit Multicast limit
	 * @param now Current time
	 * @param mg Multicast group
	 * @param from Source Ethernet MAC address
	 * @param etherType Ethernet frame type
	 * @param data Packet data
	 * @param len Length of packet data
	 */
	void send(const RuntimeEnvironment *RR,uint64_t nwid,unsigned int limit,uint64_t now,const MulticastGroup &mg,const MAC &src,unsigned int etherType,const void *data,unsigned int len);

	/**
	 * Clean up and resort database
	 *
	 * @param RR Runtime environment
	 * @param now Current time
	 * @param limit Multicast limit
	 */
	void clean(const RuntimeEnvironment *RR,uint64_t now,unsigned int limit);

private:
	void _add(const RuntimeEnvironment *RR,uint64_t now,MulticastGroupStatus &gs,const Address &learnedFrom,const Address &member);

	std::map< MulticastGroup,MulticastGroupStatus > _groups;
	Mutex _groups_m;
};

} // namespace ZeroTier

#endif
