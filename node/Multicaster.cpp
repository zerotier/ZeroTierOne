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
#include "SharedPtr.hpp"
#include "Multicaster.hpp"
#include "Topology.hpp"
#include "Switch.hpp"
#include "Packet.hpp"
#include "Peer.hpp"
#include "CMWC4096.hpp"
#include "CertificateOfMembership.hpp"
#include "RuntimeEnvironment.hpp"

namespace ZeroTier {

Multicaster::Multicaster(const RuntimeEnvironment *renv) :
	RR(renv)
{
}

Multicaster::~Multicaster()
{
}

unsigned int Multicaster::gather(const RuntimeEnvironment *RR,const Address &queryingPeer,uint64_t nwid,MulticastGroup &mg,Packet &appendTo,unsigned int limit) const
{
	unsigned char *p;
	unsigned int n = 0,i,rptr,skipped = 0;
	uint64_t a,done[(ZT_PROTO_MAX_PACKET_LENGTH / 5) + 1];

	Mutex::Lock _l(_groups_m);

	std::map< std::pair<uint64_t,MulticastGroup>,MulticastGroupStatus >::const_iterator gs(_groups.find(std::pair<uint64_t,MulticastGroup>(nwid,mg)));
	if ((gs == _groups.end())||(gs->second.members.empty())) {
		appendTo.append((uint32_t)0);
		appendTo.append((uint16_t)0);
		return 0;
	}

	if (limit > gs->second.members.size())
		limit = (unsigned int)gs->second.members.size();
	if (limit > ((ZT_PROTO_MAX_PACKET_LENGTH / 5) + 1))
		limit = (ZT_PROTO_MAX_PACKET_LENGTH / 5) + 1;

	unsigned int totalAt = appendTo.size();
	appendTo.addSize(4); // sizeof(uint32_t)
	unsigned int nAt = appendTo.size();
	appendTo.addSize(2); // sizeof(uint16_t)

	while ((n < limit)&&((appendTo.size() + ZT_ADDRESS_LENGTH) <= ZT_PROTO_MAX_PACKET_LENGTH)) {
		// Pick a member at random -- if we've already picked it,
		// keep circling the buffer until we find one we haven't.
		// This won't loop forever since limit <= members.size().
		rptr = (unsigned int)RR->prng->next32();
restart_member_scan:
		a = gs->second.members[rptr % (unsigned int)gs->second.members.size()].address.toInt();
		for(i=0;i<n;++i) {
			if (done[i] == a) {
				++rptr;
				goto restart_member_scan;
			}
		}

		// Log that we've picked this one
		done[n++] = a;

		if (queryingPeer.toInt() == a) {
			++skipped;
		} else {
			// Append to packet
			p = (unsigned char *)appendTo.appendField(ZT_ADDRESS_LENGTH);
			*(p++) = (unsigned char)((a >> 32) & 0xff);
			*(p++) = (unsigned char)((a >> 24) & 0xff);
			*(p++) = (unsigned char)((a >> 16) & 0xff);
			*(p++) = (unsigned char)((a >> 8) & 0xff);
			*p = (unsigned char)(a & 0xff);
		}
	}

	appendTo.setAt(totalAt,(uint32_t)(gs->second.members.size() - skipped));
	appendTo.setAt(nAt,(uint16_t)(n - skipped));

	return n;
}

void Multicaster::send(
	const CertificateOfMembership *com,
	unsigned int limit,
	uint64_t now,
	uint64_t nwid,
	const MulticastGroup &mg,
	const MAC &src,
	unsigned int etherType,
	const void *data,
	unsigned int len)
{
	Mutex::Lock _l(_groups_m);
	MulticastGroupStatus &gs = _groups[std::pair<uint64_t,MulticastGroup>(nwid,mg)];

	if (gs.members.size() >= limit) {
		// If we already have enough members, just send and we're done -- no need for TX queue
		OutboundMulticast out;

		out.init(
			now,
			RR->identity.address(),
			nwid,
			com,
			limit,
			0,
			src,
			mg,
			etherType,
			data,
			len);

		unsigned int count = 0;
		for(std::vector<MulticastGroupMember>::const_reverse_iterator m(gs.members.rbegin());m!=gs.members.rend();++m) {
			out.sendOnly(*(RR->sw),m->address); // sendOnly() avoids overhead of creating sent log since we're going to discard this immediately
			if (++count >= limit)
				break;
		}
	} else {
		unsigned int gatherLimit = (limit - (unsigned int)gs.members.size()) + 1;

		if ((now - gs.lastExplicitGather) >= ZT_MULTICAST_GATHER_DELAY) {
			gs.lastExplicitGather = now;

			// TODO / INPROGRESS: right now supernodes track multicast LIKEs, a relic
			// from the old algorithm. The next step will be to devolve this duty
			// somewhere else, such as node(s) nominated by netconf masters. But
			// we'll keep announcing LIKEs to supernodes for the near future to
			// gradually migrate from old multicast to new without losing old nodes.
			SharedPtr<Peer> sn(RR->topology->getBestSupernode());
			if (sn) {
				Packet outp(sn->address(),RR->identity.address(),Packet::VERB_MULTICAST_GATHER);
				outp.append(nwid);
				outp.append((uint8_t)0);
				mg.mac().appendTo(outp);
				outp.append((uint32_t)mg.adi());
				outp.append((uint32_t)gatherLimit); // +1 just means we'll have an extra in the queue if available
				outp.armor(sn->key(),true);
				sn->send(RR,outp.data(),outp.size(),now);
			}

			gatherLimit = 0; // once we've done this we don't need to do it implicitly
		}

		gs.txQueue.push_back(OutboundMulticast());
		OutboundMulticast &out = gs.txQueue.back();

		out.init(
			now,
			RR->identity.address(),
			nwid,
			com,
			limit,
			gatherLimit,
			src,
			mg,
			etherType,
			data,
			len);

		for(std::vector<MulticastGroupMember>::const_reverse_iterator m(gs.members.rbegin());m!=gs.members.rend();++m)
			out.sendAndLog(*(RR->sw),m->address);
	}
}

void Multicaster::clean(uint64_t now)
{
	Mutex::Lock _l(_groups_m);
	for(std::map< std::pair<uint64_t,MulticastGroup>,MulticastGroupStatus >::iterator mm(_groups.begin());mm!=_groups.end();) {
		// Remove expired outgoing multicasts from multicast TX queue
		for(std::list<OutboundMulticast>::iterator tx(mm->second.txQueue.begin());tx!=mm->second.txQueue.end();) {
			if ((tx->expired(now))||(tx->atLimit()))
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
					SharedPtr<Peer> p(RR->topology->getPeer(writer->learnedFrom));
					if (p)
						writer->rank = p->lastUnicastFrame() - ZT_MULTICAST_LIKE_EXPIRE;
					else writer->rank = writer->timestamp - (86400000 + ZT_MULTICAST_LIKE_EXPIRE);
				} else {
					SharedPtr<Peer> p(RR->topology->getPeer(writer->address));
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

void Multicaster::_add(uint64_t now,uint64_t nwid,MulticastGroupStatus &gs,const Address &learnedFrom,const Address &member)
{
	// assumes _groups_m is locked

	// Do not add self -- even if someone else returns it
	if (member == RR->identity.address())
		return;

	// Update timestamp and learnedFrom if existing
	for(std::vector<MulticastGroupMember>::iterator m(gs.members.begin());m!=gs.members.end();++m) {
		if (m->address == member) {
			// learnedFrom is NULL (zero) if we've learned this directly via MULTICAST_LIKE, at which
			// point this becomes a first-order connection.
			if (m->learnedFrom)
				m->learnedFrom = learnedFrom;
			m->timestamp = now;
			return;
		}
	}

	// If not existing, add to end of list (highest priority) -- these will
	// be resorted on next clean(). In the future we might want to insert
	// this somewhere else but we'll try this for now.
	gs.members.push_back(MulticastGroupMember(member,learnedFrom,now));

	// Try to send to any outgoing multicasts that are waiting for more recipients
	for(std::list<OutboundMulticast>::iterator tx(gs.txQueue.begin());tx!=gs.txQueue.end();) {
		tx->sendIfNew(*(RR->sw),member);
		if (tx->atLimit())
			gs.txQueue.erase(tx++);
		else ++tx;
	}
}

} // namespace ZeroTier
