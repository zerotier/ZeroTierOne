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
#include "MulticastTopology.hpp"
#include "Topology.hpp"

namespace ZeroTier {

MulticastTopology::MulticastTopology()
{
}

MulticastTopology::~MulticastTopology()
{
}

void MulticastTopology::add(const MulticastGroup &mg,const Address &member,const Address &learnedFrom)
{
	Mutex::Lock _l(_groups_m);
	std::vector<MulticastGroupMember> &mv = _groups[mg].members;
	for(std::vector<MulticastGroupMember>::iterator m(mv.begin());m!=mv.end();++m) {
		if (m->address == member) {
			if (m->learnedFrom) // once a member has been seen directly, we keep its status as direct
				m->learnedFrom = learnedFrom;
			m->timestamp = Utils::now();
			return;
		}
	}
	mv.push_back(MulticastGroupMember(member,learnedFrom,Utils::now()));
}

void MulticastTopology::erase(const MulticastGroup &mg,const Address &member)
{
	Mutex::Lock _l(_groups_m);
	std::map< MulticastGroup,MulticastGroupStatus >::iterator r(_groups.find(mg));
	if (r != _groups.end()) {
		for(std::vector<MulticastGroupMember>::iterator m(r->second.members.begin());m!=r->second.members.end();++m) {
			if (m->address == member) {
				r->second.members.erase(m);
				if (r->second.members.empty())
					_groups.erase(r);
				return;
			}
		}
	}
}

unsigned int MulticastTopology::shouldGather(const MulticastGroup &mg,uint64_t now,unsigned int limit,bool updateLastGatheredTimeOnNonzeroReturn)
{
	Mutex::Lock _l(_groups_m);
	MulticastGroupStatus &gs = _groups[mg];
	if ((unsigned int)gs.members.size() >= limit) {
		// We already caught our limit, don't need to go fishing any more.
		return 0;
	} else {
		// Compute the delay between fishing expeditions from the fraction of the limit that we already have.
		const uint64_t rateDelay = (uint64_t)ZT_MULTICAST_TOPOLOGY_GATHER_DELAY_MIN + (uint64_t)(((double)gs.members.size() / (double)limit) * (double)(ZT_MULTICAST_TOPOLOGY_GATHER_DELAY_MAX - ZT_MULTICAST_TOPOLOGY_GATHER_DELAY_MIN));

		if ((now - gs.lastGatheredMembers) >= rateDelay) {
			if (updateLastGatheredTimeOnNonzeroReturn)
				gs.lastGatheredMembers = now;
			return (limit - (unsigned int)gs.members.size());
		} else return 0;
	}
}

void MulticastTopology::clean(uint64_t now,const Topology &topology)
{
	Mutex::Lock _l(_groups_m);
	for(std::map< MulticastGroup,MulticastGroupStatus >::iterator mm(_groups.begin());mm!=_groups.end();) {
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
			std::sort(mm->second.members.begin(),writer); // sorts in ascending order of rank
			mm->second.members.resize(count); // trim off the ones we cut, after writer
			++mm;
		} else _groups.erase(mm++);
	}
}

} // namespace ZeroTier
