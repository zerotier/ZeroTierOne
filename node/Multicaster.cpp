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

#include <algorithm>

#include "Constants.hpp"
#include "Multicaster.hpp"
#include "Topology.hpp"
#include "Switch.hpp"
#include "Packet.hpp"
#include "RuntimeEnvironment.hpp"

namespace ZeroTier {

Multicaster::Multicaster() :
	_limit(ZT_DEFAULT_MULTICAST_LIMIT)
{
}

Multicaster::~Multicaster()
{
}

void send(const RuntimeEnvironment *RR,uint64_t nwid,unsigned int limit,uint64_t now,const MulticastGroup &mg,const MAC &src,unsigned int etherType,const void *data,unsigned int len)
{
	Mutex::Lock _l(_groups_m);
	MulticastGroupStatus &gs = _groups[mg];

	if (gs.members.size() >= limit) {
		// If we already have enough members, just send and we're done -- no need for TX queue
		OutboundMulticast out;

		out.init(now,RR->identity.address(),nwid,src,mg,etherType,data,len);
		for(std::vector<MulticastGroupMember>::const_reverse_iterator m(gs.members.rbegin());m!=gs.members.rend();++gs)
			out.sendOnly(*(RR->sw),m->address);
	} else {
		// If we don't already have enough members, send to the ones we have and then gather (if allowed within gather rate limit delay)
		gs.txQueue.push_back(OutboundMulticast());
		OutboundMulticast &out = gs.txQueue.back();

		out.init(now,RR->identity.address(),nwid,src,mg,etherType,data,len);
		for(std::vector<MulticastGroupMember>::const_reverse_iterator m(gs.members.rbegin());m!=gs.members.rend();++gs)
			out.sendAndLog(*(RR->sw),m->address);


	}
}

void Multicaster::clean(uint64_t now,const Topology &topology)
{
	Mutex::Lock _l(_groups_m);
	for(std::map< MulticastGroup,MulticastGroupStatus >::iterator mm(_groups.begin());mm!=_groups.end();) {
		// Remove expired outgoing multicasts from multicast TX queue
		for(std::list<OutboundMulticast>::iterator tx(mm->second.txQueue.begin());tx!=mm->second.txQueue.end();) {
			if (tx->expired(now))
				mm->second.txQueue.erase(tx++);
			else ++tx;
		}

		// Remove expired members from membership list, and update rank
		// so that remaining members can be sorted in ascending order of
		// transmit priority.
		std::vector<MulticastGroupMember>::iterator reader(mm->second.members.begin());
		std::vector<MulticastGroupMember>::iterator writer(mm->second.members.begin());
		unsigned int count = 0;
		while (reader != mm->second.members.end()) {
			if ((now - reader->timestamp) < ZT_MULTICAST_LIKE_EXPIRE) {
				*writer = *reader;

				/* We rank in ascending order of most recent relevant activity. For peers we've learned
				 * about by direct LIKEs, we do this in order of their own activity. For indirectly
				 * acquired peers we do this minus a constant to place these categorically below directly
				 * learned peers. For peers with no active Peer record, we use the time we last learned
				 * about them minus one day (a large constant) to put these at the bottom of the list.
				 * List is sorted in ascending order of rank and multicasts are sent last-to-first. */
				if (writer->learnedFrom) {
					SharedPtr<Peer> p(topology.getPeer(writer->learnedFrom));
					if (p)
						writer->rank = p->lastUnicastFrame() - ZT_MULTICAST_LIKE_EXPIRE;
					else writer->rank = writer->timestamp - (86400000 + ZT_MULTICAST_LIKE_EXPIRE);
				} else {
					SharedPtr<Peer> p(topology.getPeer(writer->address));
					if (p)
						writer->rank = p->lastUnicastFrame();
					else writer->rank = writer->timestamp - 86400000;
				}

				++writer;
				++count;
			}
			++reader;
		}

		if (count) {
			// There are remaining members, so re-sort them by rank and resize the vector
			std::sort(mm->second.members.begin(),writer); // sorts in ascending order of rank
			mm->second.members.resize(count); // trim off the ones we cut, after writer
			++mm;
		} else if (mm->second.txQueue.empty()) {
			// There are no remaining members and no pending multicasts, so erase the entry
			_groups.erase(mm++);
		} else ++mm;
	}
}

void Multicaster::_add(const RuntimeEnvironment *RR,const MulticastGroup &mg,const Address &learnedFrom,const Address &member)
{
	// assumes _groups_m is locked
}

unsigned int Multicaster::_want(const MulticastGroup &mg,MulticastGroupStatus &gs,uint64_t now,unsigned int limit)
{
	if (gs.members.size() >= limit) {
		// We already caught our limit, don't need to go fishing any more.
		return 0;
	} else {
		// Compute the delay between fishing expeditions from the fraction of the limit that we already have.
		const uint64_t rateDelay = (uint64_t)ZT_MULTICAST_TOPOLOGY_GATHER_DELAY_MIN + (uint64_t)(((double)gs.members.size() / (double)limit) * (double)(ZT_MULTICAST_TOPOLOGY_GATHER_DELAY_MAX - ZT_MULTICAST_TOPOLOGY_GATHER_DELAY_MIN));
		if ((now - gs.lastGatheredMembers) >= rateDelay) {
			gs.lastGatheredMembers = now;
			return (limit - (unsigned int)gs.members.size());
		} else return 0;
	}
}

} // namespace ZeroTier
