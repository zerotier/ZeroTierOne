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

void MulticastTopology::clean(const Topology &topology)
{
	uint64_t now = Utils::now();

	for(std::map< MulticastGroup,std::vector<MulticastGroupMember> >::iterator mm(_members.begin());mm!=_members.end();) {
		std::vector<MulticastGroupMember>::iterator reader(mm->second.begin());
		std::vector<MulticastGroupMember>::iterator writer(mm->second.begin());
		unsigned long count = 0;
		while (reader != mm->second.end()) {
			if ((now - reader->timestamp) < ZT_MULTICAST_LIKE_EXPIRE) {
				*writer = *reader;

				/* We sort in ascending order of most recent relevant activity. For peers we've learned
				 * about by direct LIKEs, we do this in order of their own activity. For indirectly
				 * acquired peers we do this minus a constant to place these categorically below directly
				 * learned peers. For peers with no active Peer record, we use the time we last learned
				 * about them minus one day (a large constant) to put these at the bottom of the list.
				 * List is sorted in ascending order of rank and multicasts are sent last-to-first. */
				if (writer->learnedFrom) {
					SharedPtr<Peer> p(topology.getPeer(writer->learnedFrom));
					if (p)
						writer->rank = p->lastUnicastFrame() - ZT_MULTICAST_LIKE_EXPIRE;
					else writer->rank = writer->timestamp - 86400000;
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
			mm->second.resize(count);
			std::sort(mm->second.begin(),mm->second.end()); // sorts in ascending order of rank
			++mm;
		} else _members.erase(mm++);
	}
}

} // namespace ZeroTier
