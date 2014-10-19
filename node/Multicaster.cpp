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
#include "RuntimeEnvironment.hpp"
#include "SharedPtr.hpp"
#include "Multicaster.hpp"
#include "Topology.hpp"
#include "Switch.hpp"
#include "Packet.hpp"
#include "Peer.hpp"
#include "CMWC4096.hpp"
#include "C25519.hpp"
#include "NodeConfig.hpp"
#include "CertificateOfMembership.hpp"
#include "Logger.hpp"

namespace ZeroTier {

Multicaster::Multicaster(const RuntimeEnvironment *renv) :
	RR(renv)
{
}

Multicaster::~Multicaster()
{
}

unsigned int Multicaster::gather(const Address &queryingPeer,uint64_t nwid,const MulticastGroup &mg,Packet &appendTo,unsigned int limit) const
{
	unsigned char *p;
	unsigned int added = 0,i,k,rptr,totalKnown = 0;
	uint64_t a,picked[(ZT_PROTO_MAX_PACKET_LENGTH / 5) + 1];

	if (!limit)
		return 0;
	if (limit > 0xffff) // TODO: multiple return packets not yet supported
		limit = 0xffff;

	{ // Return myself if I am a member of this group
		SharedPtr<Network> network(RR->nc->network(nwid));
		if ((network)&&(network->subscribedToMulticastGroup(mg))) {
			RR->identity.address().appendTo(appendTo);
			++totalKnown;
			++added;
		}
	}

	Mutex::Lock _l(_groups_m);

	const unsigned int totalAt = appendTo.size();
	appendTo.addSize(4); // sizeof(uint32_t)
	const unsigned int addedAt = appendTo.size();
	appendTo.addSize(2); // sizeof(uint16_t)

	std::map< std::pair<uint64_t,MulticastGroup>,MulticastGroupStatus >::const_iterator gs(_groups.find(std::pair<uint64_t,MulticastGroup>(nwid,mg)));
	if ((gs != _groups.end())&&(!gs->second.members.empty())) {
		totalKnown += (unsigned int)gs->second.members.size();

		// Members are returned in random order so that repeated gather queries
		// will return different subsets of a large multicast group.
		k = 0;
		while ((added < limit)&&(k < gs->second.members.size())&&((appendTo.size() + ZT_ADDRESS_LENGTH) <= ZT_PROTO_MAX_PACKET_LENGTH)) {
			rptr = (unsigned int)RR->prng->next32();
restart_member_scan:
			a = gs->second.members[rptr % (unsigned int)gs->second.members.size()].address.toInt();
			for(i=0;i<k;++i) {
				if (picked[i] == a) {
					++rptr;
					goto restart_member_scan;
				}
			}
			picked[k++] = a;

			if (queryingPeer.toInt() != a) { // do not return the peer that is making the request as a result
				p = (unsigned char *)appendTo.appendField(ZT_ADDRESS_LENGTH);
				*(p++) = (unsigned char)((a >> 32) & 0xff);
				*(p++) = (unsigned char)((a >> 24) & 0xff);
				*(p++) = (unsigned char)((a >> 16) & 0xff);
				*(p++) = (unsigned char)((a >> 8) & 0xff);
				*p = (unsigned char)(a & 0xff);
				++added;
			}
		}
	}

	appendTo.setAt(totalAt,(uint32_t)totalKnown);
	appendTo.setAt(addedAt,(uint16_t)added);

	//TRACE("..MC Multicaster::gather() attached %u of %u peers for %.16llx/%s (2)",n,(unsigned int)(gs->second.members.size() - skipped),nwid,mg.toString().c_str());

	return added;
}

std::vector<Address> Multicaster::getMembers(uint64_t nwid,const MulticastGroup &mg,unsigned int limit) const
{
	std::vector<Address> ls;
	Mutex::Lock _l(_groups_m);
	std::map< std::pair<uint64_t,MulticastGroup>,MulticastGroupStatus >::const_iterator gs(_groups.find(std::pair<uint64_t,MulticastGroup>(nwid,mg)));
	if (gs == _groups.end())
		return ls;
	for(std::vector<MulticastGroupMember>::const_reverse_iterator m(gs->second.members.rbegin());m!=gs->second.members.rend();++m) {
		ls.push_back(m->address);
		if (ls.size() >= limit)
			break;
	}
	return ls;
}

void Multicaster::send(
	const CertificateOfMembership *com,
	unsigned int limit,
	uint64_t now,
	uint64_t nwid,
	const std::vector<Address> &alwaysSendTo,
	const MulticastGroup &mg,
	const MAC &src,
	unsigned int etherType,
	const void *data,
	unsigned int len)
{
	Mutex::Lock _l(_groups_m);
	MulticastGroupStatus &gs = _groups[std::pair<uint64_t,MulticastGroup>(nwid,mg)];

	if (gs.members.size() >= limit) {
		// If we already have enough members, just send and we're done. We can
		// skip the TX queue and skip the overhead of maintaining a send log by
		// using sendOnly().
		OutboundMulticast out;

		out.init(
			RR,
			now,
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

		for(std::vector<Address>::const_iterator ast(alwaysSendTo.begin());ast!=alwaysSendTo.end();++ast) {
			{ // TODO / LEGACY: don't send new multicast frame to old peers (if we know their version)
				SharedPtr<Peer> p(RR->topology->getPeer(*ast));
				if ((p)&&(p->remoteVersionKnown())&&(p->remoteVersionMajor() < 1))
					continue;
			}

			if (count++ >= limit)
				break;
			out.sendOnly(RR,*ast);
		}

		for(std::vector<MulticastGroupMember>::const_reverse_iterator m(gs.members.rbegin());m!=gs.members.rend();++m) {
			{ // TODO / LEGACY: don't send new multicast frame to old peers (if we know their version)
				SharedPtr<Peer> p(RR->topology->getPeer(m->address));
				if ((p)&&(p->remoteVersionKnown())&&(p->remoteVersionMajor() < 1))
					continue;
			}

			if (count++ >= limit)
				break;
			if (std::find(alwaysSendTo.begin(),alwaysSendTo.end(),m->address) == alwaysSendTo.end())
				out.sendOnly(RR,m->address);
		}
	} else {
		unsigned int gatherLimit = (limit - (unsigned int)gs.members.size()) + 1;

		if ((now - gs.lastExplicitGather) >= ZT_MULTICAST_EXPLICIT_GATHER_DELAY) {
			gs.lastExplicitGather = now;
			SharedPtr<Peer> sn(RR->topology->getBestSupernode());
			if (sn) {
				TRACE(">>MC GATHER up to %u in %.16llx/%s",gatherLimit,nwid,mg.toString().c_str());

				Packet outp(sn->address(),RR->identity.address(),Packet::VERB_MULTICAST_GATHER);
				outp.append(nwid);
				outp.append((uint8_t)0);
				mg.mac().appendTo(outp);
				outp.append((uint32_t)mg.adi());
				outp.append((uint32_t)gatherLimit); // +1 just means we'll have an extra in the queue if available
				outp.armor(sn->key(),true);
				sn->send(RR,outp.data(),outp.size(),now);
			}
			gatherLimit = 0; // implicit not needed
		} else if ((now - gs.lastImplicitGather) > ZT_MULTICAST_IMPLICIT_GATHER_DELAY) {
			gs.lastImplicitGather = now;
		} else {
			gatherLimit = 0;
		}

		gs.txQueue.push_back(OutboundMulticast());
		OutboundMulticast &out = gs.txQueue.back();

		out.init(
			RR,
			now,
			nwid,
			com,
			limit,
			gatherLimit,
			src,
			mg,
			etherType,
			data,
			len);

		for(std::vector<Address>::const_iterator ast(alwaysSendTo.begin());ast!=alwaysSendTo.end();++ast) {
			{ // TODO / LEGACY: don't send new multicast frame to old peers (if we know their version)
				SharedPtr<Peer> p(RR->topology->getPeer(*ast));
				if ((p)&&(p->remoteVersionKnown())&&(p->remoteVersionMajor() < 1))
					continue;
			}

			out.sendAndLog(RR,*ast);
		}

		for(std::vector<MulticastGroupMember>::const_reverse_iterator m(gs.members.rbegin());m!=gs.members.rend();++m) {
			{ // TODO / LEGACY: don't send new multicast frame to old peers (if we know their version)
				SharedPtr<Peer> p(RR->topology->getPeer(m->address));
				if ((p)&&(p->remoteVersionKnown())&&(p->remoteVersionMajor() < 1))
					continue;
			}

			if (std::find(alwaysSendTo.begin(),alwaysSendTo.end(),m->address) == alwaysSendTo.end())
				out.sendAndLog(RR,m->address);
		}
	}

	// DEPRECATED / LEGACY / TODO:
	// Currently we also always send a legacy P5_MULTICAST_FRAME packet to our
	// supernode. Our supernode then takes care of relaying it down to <1.0.0
	// nodes. This code can go away (along with support for P5_MULTICAST_FRAME)
	// once there are no more such nodes on the network.
	{
		SharedPtr<Peer> sn(RR->topology->getBestSupernode());
		if (sn) {
			uint32_t rn = RR->prng->next32();
			Packet outp(sn->address(),RR->identity.address(),Packet::VERB_P5_MULTICAST_FRAME);

			outp.append((uint16_t)0xffff); // do not forward
			outp.append((unsigned char)0,320 + 1024); // empty queue and bloom filter

			outp.append((unsigned char)((com) ? ZT_PROTO_VERB_P5_MULTICAST_FRAME_FLAGS_HAS_MEMBERSHIP_CERTIFICATE : 0));
			outp.append((uint64_t)nwid);
			outp.append((uint16_t)0);
			outp.append((unsigned char)0);
			outp.append((unsigned char)0);
			RR->identity.address().appendTo(outp);
			outp.append((const void *)&rn,3); // random multicast ID
			if (src)
				src.appendTo(outp);
			else MAC(RR->identity.address(),nwid).appendTo(outp);
			mg.mac().appendTo(outp);
			outp.append((uint32_t)mg.adi());
			outp.append((uint16_t)etherType);
			outp.append((uint16_t)len);
			outp.append(data,len);
			unsigned int signedPortionLen = outp.size() - ZT_PROTO_VERB_P5_MULTICAST_FRAME_IDX__START_OF_SIGNED_PORTION;

			C25519::Signature sig(RR->identity.sign(outp.field(ZT_PROTO_VERB_P5_MULTICAST_FRAME_IDX__START_OF_SIGNED_PORTION,signedPortionLen),signedPortionLen));

			outp.append((uint16_t)sig.size());
			outp.append(sig.data,(unsigned int)sig.size());

			if (com) com->serialize(outp);

			outp.compress();
			outp.armor(sn->key(),true);
			sn->send(RR,outp.data(),outp.size(),now);
		}
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

void Multicaster::_add(uint64_t now,uint64_t nwid,const MulticastGroup &mg,MulticastGroupStatus &gs,const Address &learnedFrom,const Address &member)
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

	//TRACE("..MC %s joined multicast group %.16llx/%s via %s",member.toString().c_str(),nwid,mg.toString().c_str(),((learnedFrom) ? learnedFrom.toString().c_str() : "(direct)"));

	// Try to send to any outgoing multicasts that are waiting for more recipients
	for(std::list<OutboundMulticast>::iterator tx(gs.txQueue.begin());tx!=gs.txQueue.end();) {
		{ // TODO / LEGACY: don't send new multicast frame to old peers (if we know their version)
			SharedPtr<Peer> p(RR->topology->getPeer(member));
			if ((p)&&(p->remoteVersionKnown())&&(p->remoteVersionMajor() < 1))
				continue;
		}

		tx->sendIfNew(RR,member);
		if (tx->atLimit())
			gs.txQueue.erase(tx++);
		else ++tx;
	}
}

} // namespace ZeroTier
