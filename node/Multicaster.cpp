/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#include <algorithm>

#include "Constants.hpp"
#include "RuntimeEnvironment.hpp"
#include "Multicaster.hpp"
#include "Topology.hpp"
#include "Switch.hpp"
#include "Packet.hpp"
#include "Peer.hpp"
#include "C25519.hpp"
#include "CertificateOfMembership.hpp"
#include "Node.hpp"
#include "Network.hpp"

namespace ZeroTier {

Multicaster::Multicaster(const RuntimeEnvironment *renv) :
	RR(renv),
	_groups(32)
{
}

Multicaster::~Multicaster()
{
}

void Multicaster::addMultiple(void *tPtr,int64_t now,uint64_t nwid,const MulticastGroup &mg,const void *addresses,unsigned int count,unsigned int totalKnown)
{
	const unsigned char *p = (const unsigned char *)addresses;
	const unsigned char *e = p + (5 * count);
	Mutex::Lock _l(_groups_m);
	MulticastGroupStatus &gs = _groups[Multicaster::Key(nwid,mg)];
	while (p != e) {
		_add(tPtr,now,nwid,mg,gs,Address(p,5));
		p += 5;
	}
}

void Multicaster::remove(uint64_t nwid,const MulticastGroup &mg,const Address &member)
{
	Mutex::Lock _l(_groups_m);
	MulticastGroupStatus *s = _groups.get(Multicaster::Key(nwid,mg));
	if (s) {
		for(std::vector<MulticastGroupMember>::iterator m(s->members.begin());m!=s->members.end();++m) {
			if (m->address == member) {
				s->members.erase(m);
				break;
			}
		}
	}
}

unsigned int Multicaster::gather(const Address &queryingPeer,uint64_t nwid,const MulticastGroup &mg,Buffer<ZT_PROTO_MAX_PACKET_LENGTH> &appendTo,unsigned int limit) const
{
	unsigned char *p;
	unsigned int added = 0,i,k,rptr,totalKnown = 0;
	uint64_t a,picked[(ZT_PROTO_MAX_PACKET_LENGTH / 5) + 2];

	if (!limit)
		return 0;
	else if (limit > 0xffff)
		limit = 0xffff;

	const unsigned int totalAt = appendTo.size();
	appendTo.addSize(4); // sizeof(uint32_t)
	const unsigned int addedAt = appendTo.size();
	appendTo.addSize(2); // sizeof(uint16_t)

	{ // Return myself if I am a member of this group
		SharedPtr<Network> network(RR->node->network(nwid));
		if ((network)&&(network->subscribedToMulticastGroup(mg,true))) {
			RR->identity.address().appendTo(appendTo);
			++totalKnown;
			++added;
		}
	}

	Mutex::Lock _l(_groups_m);

	const MulticastGroupStatus *s = _groups.get(Multicaster::Key(nwid,mg));
	if ((s)&&(!s->members.empty())) {
		totalKnown += (unsigned int)s->members.size();

		// Members are returned in random order so that repeated gather queries
		// will return different subsets of a large multicast group.
		k = 0;
		while ((added < limit)&&(k < s->members.size())&&((appendTo.size() + ZT_ADDRESS_LENGTH) <= ZT_PROTO_MAX_PACKET_LENGTH)) {
			rptr = (unsigned int)RR->node->prng();

restart_member_scan:
			a = s->members[rptr % (unsigned int)s->members.size()].address.toInt();
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

	return added;
}

std::vector<Address> Multicaster::getMembers(uint64_t nwid,const MulticastGroup &mg,unsigned int limit) const
{
	std::vector<Address> ls;
	Mutex::Lock _l(_groups_m);
	const MulticastGroupStatus *s = _groups.get(Multicaster::Key(nwid,mg));
	if (!s)
		return ls;
	for(std::vector<MulticastGroupMember>::const_reverse_iterator m(s->members.rbegin());m!=s->members.rend();++m) {
		ls.push_back(m->address);
		if (ls.size() >= limit)
			break;
	}
	return ls;
}

void Multicaster::send(
	void *tPtr,
	int64_t now,
	const SharedPtr<Network> &network,
	const Address &origin,
	const MulticastGroup &mg,
	const MAC &src,
	unsigned int etherType,
	const void *data,
	unsigned int len)
{
	unsigned long idxbuf[4096];
	unsigned long *indexes = idxbuf;

	// If we're in hub-and-spoke designated multicast replication mode, see if we
	// have a multicast replicator active. If so, pick the best and send it
	// there. If we are a multicast replicator or if none are alive, fall back
	// to sender replication. Note that bridges do not do this since this would
	// break bridge route learning. This is sort of an edge case limitation of
	// the current protocol and could be fixed, but fixing it would add more
	// complexity than the fix is probably worth. Bridges are generally high
	// bandwidth nodes.
	if (!network->config().isActiveBridge(RR->identity.address())) {
		Address multicastReplicators[ZT_MAX_NETWORK_SPECIALISTS];
		const unsigned int multicastReplicatorCount = network->config().multicastReplicators(multicastReplicators);
		if (multicastReplicatorCount) {
			if (std::find(multicastReplicators,multicastReplicators + multicastReplicatorCount,RR->identity.address()) == (multicastReplicators + multicastReplicatorCount)) {
				SharedPtr<Peer> bestMulticastReplicator;
				SharedPtr<Path> bestMulticastReplicatorPath;
				unsigned int bestMulticastReplicatorLatency = 0xffff;
				for(unsigned int i=0;i<multicastReplicatorCount;++i) {
					const SharedPtr<Peer> p(RR->topology->getPeerNoCache(multicastReplicators[i]));
					if ((p)&&(p->isAlive(now))) {
						const SharedPtr<Path> pp(p->getAppropriatePath(now,false));
						if ((pp)&&(pp->latency() < bestMulticastReplicatorLatency)) {
							bestMulticastReplicatorLatency = pp->latency();
							bestMulticastReplicatorPath = pp;
							bestMulticastReplicator = p;
						}
					}
				}
				if (bestMulticastReplicator) {
					Packet outp(bestMulticastReplicator->address(),RR->identity.address(),Packet::VERB_MULTICAST_FRAME);
					outp.append((uint64_t)network->id());
					outp.append((uint8_t)0x0c); // includes source MAC | please replicate
					((src) ? src : MAC(RR->identity.address(),network->id())).appendTo(outp);
					mg.mac().appendTo(outp);
					outp.append((uint32_t)mg.adi());
					outp.append((uint16_t)etherType);
					outp.append(data,len);
					if (!network->config().disableCompression()) outp.compress();
					outp.armor(bestMulticastReplicator->key(),true,bestMulticastReplicator->aesKeysIfSupported());
					bestMulticastReplicatorPath->send(RR,tPtr,outp.data(),outp.size(),now);
					return;
				}
			}
		}
	}

	try {
		Mutex::Lock _l(_groups_m);
		MulticastGroupStatus &gs = _groups[Multicaster::Key(network->id(),mg)];

		if (!gs.members.empty()) {
			// Allocate a memory buffer if group is monstrous
			if (gs.members.size() > (sizeof(idxbuf) / sizeof(unsigned long)))
				indexes = new unsigned long[gs.members.size()];

			// Generate a random permutation of member indexes
			for(unsigned long i=0;i<gs.members.size();++i)
				indexes[i] = i;
			for(unsigned long i=(unsigned long)gs.members.size()-1;i>0;--i) {
				unsigned long j = (unsigned long)RR->node->prng() % (i + 1);
				unsigned long tmp = indexes[j];
				indexes[j] = indexes[i];
				indexes[i] = tmp;
			}
		}

		Address activeBridges[ZT_MAX_NETWORK_SPECIALISTS];
		const unsigned int activeBridgeCount = network->config().activeBridges(activeBridges);
		const unsigned int limit = network->config().multicastLimit;

		if (gs.members.size() >= limit) {
			// Skip queue if we already have enough members to complete the send operation
			OutboundMulticast out;

			out.init(
				RR,
				now,
				network->id(),
				network->config().disableCompression(),
				limit,
				1, // we'll still gather a little from peers to keep multicast list fresh
				src,
				mg,
				etherType,
				data,
				len);

			unsigned int count = 0;

			for(unsigned int i=0;i<activeBridgeCount;++i) {
				if ((activeBridges[i] != RR->identity.address())&&(activeBridges[i] != origin)) {
					out.sendOnly(RR,tPtr,activeBridges[i]); // optimization: don't use dedup log if it's a one-pass send
					if (++count >= limit)
						break;
				}
			}

			unsigned long idx = 0;
			while ((count < limit)&&(idx < gs.members.size())) {
				const Address ma(gs.members[indexes[idx++]].address);
				if ((std::find(activeBridges,activeBridges + activeBridgeCount,ma) == (activeBridges + activeBridgeCount))&&(ma != origin)) {
					out.sendOnly(RR,tPtr,ma); // optimization: don't use dedup log if it's a one-pass send
					++count;
				}
			}
		} else {
			if (gs.txQueue.size() >= ZT_TX_QUEUE_SIZE) {
				RR->t->outgoingNetworkFrameDropped(tPtr,network,src,mg.mac(),etherType,0,len,"multicast TX queue is full");
				return;
			}

			const unsigned int gatherLimit = (limit - (unsigned int)gs.members.size()) + 1;

			if ((gs.members.empty())||((now - gs.lastExplicitGather) >= ZT_MULTICAST_EXPLICIT_GATHER_DELAY)) {
				gs.lastExplicitGather = now;

				Address explicitGatherPeers[16];
				unsigned int numExplicitGatherPeers = 0;

				SharedPtr<Peer> bestRoot(RR->topology->getUpstreamPeer());
				if (bestRoot)
					explicitGatherPeers[numExplicitGatherPeers++] = bestRoot->address();

				explicitGatherPeers[numExplicitGatherPeers++] = network->controller();

				Address ac[ZT_MAX_NETWORK_SPECIALISTS];
				const unsigned int accnt = network->config().alwaysContactAddresses(ac);
				unsigned int shuffled[ZT_MAX_NETWORK_SPECIALISTS];
				for(unsigned int i=0;i<accnt;++i)
					shuffled[i] = i;
				for(unsigned int i=0,k=accnt>>1;i<k;++i) {
					const uint64_t x = RR->node->prng();
					const unsigned int x1 = shuffled[(unsigned int)x % accnt];
					const unsigned int x2 = shuffled[(unsigned int)(x >> 32) % accnt];
					const unsigned int tmp = shuffled[x1];
					shuffled[x1] = shuffled[x2];
					shuffled[x2] = tmp;
				}
				for(unsigned int i=0;i<accnt;++i) {
					explicitGatherPeers[numExplicitGatherPeers++] = ac[shuffled[i]];
					if (numExplicitGatherPeers == 16)
						break;
				}

				std::vector<Address> anchors(network->config().anchors());
				for(std::vector<Address>::const_iterator a(anchors.begin());a!=anchors.end();++a) {
					if (*a != RR->identity.address()) {
						explicitGatherPeers[numExplicitGatherPeers++] = *a;
						if (numExplicitGatherPeers == 16)
							break;
					}
				}

				for(unsigned int k=0;k<numExplicitGatherPeers;++k) {
					const CertificateOfMembership *com = (network) ? ((network->config().com) ? &(network->config().com) : (const CertificateOfMembership *)0) : (const CertificateOfMembership *)0;
					Packet outp(explicitGatherPeers[k],RR->identity.address(),Packet::VERB_MULTICAST_GATHER);
					outp.append(network->id());
					outp.append((uint8_t)((com) ? 0x01 : 0x00));
					mg.mac().appendTo(outp);
					outp.append((uint32_t)mg.adi());
					outp.append((uint32_t)gatherLimit);
					if (com)
						com->serialize(outp);
					RR->node->expectReplyTo(outp.packetId());
					RR->sw->send(tPtr,outp,true);
				}
			}

			gs.txQueue.push_back(OutboundMulticast());
			OutboundMulticast &out = gs.txQueue.back();

			out.init(
				RR,
				now,
				network->id(),
				network->config().disableCompression(),
				limit,
				gatherLimit,
				src,
				mg,
				etherType,
				data,
				len);

			if (origin)
				out.logAsSent(origin);

			unsigned int count = 0;

			for(unsigned int i=0;i<activeBridgeCount;++i) {
				if (activeBridges[i] != RR->identity.address()) {
					out.sendAndLog(RR,tPtr,activeBridges[i]);
					if (++count >= limit)
						break;
				}
			}

			unsigned long idx = 0;
			while ((count < limit)&&(idx < gs.members.size())) {
				Address ma(gs.members[indexes[idx++]].address);
				if (std::find(activeBridges,activeBridges + activeBridgeCount,ma) == (activeBridges + activeBridgeCount)) {
					out.sendAndLog(RR,tPtr,ma);
					++count;
				}
			}
		}
	} catch ( ... ) {} // this is a sanity check to catch any failures and make sure indexes[] still gets deleted

	// Free allocated memory buffer if any
	if (indexes != idxbuf)
		delete [] indexes;
}

void Multicaster::clean(int64_t now)
{
	{
		Mutex::Lock _l(_groups_m);
		Multicaster::Key *k = (Multicaster::Key *)0;
		MulticastGroupStatus *s = (MulticastGroupStatus *)0;
		Hashtable<Multicaster::Key,MulticastGroupStatus>::Iterator mm(_groups);
		while (mm.next(k,s)) {
			for(std::list<OutboundMulticast>::iterator tx(s->txQueue.begin());tx!=s->txQueue.end();) {
				if ((tx->expired(now))||(tx->atLimit()))
					s->txQueue.erase(tx++);
				else ++tx;
			}

			unsigned long count = 0;
			{
				std::vector<MulticastGroupMember>::iterator reader(s->members.begin());
				std::vector<MulticastGroupMember>::iterator writer(reader);
				while (reader != s->members.end()) {
					if ((now - reader->timestamp) < ZT_MULTICAST_LIKE_EXPIRE) {
						*writer = *reader;
						++writer;
						++count;
					}
					++reader;
				}
			}

			if (count) {
				s->members.resize(count);
			} else if (s->txQueue.empty()) {
				_groups.erase(*k);
			} else {
				s->members.clear();
			}
		}
	}
}

void Multicaster::_add(void *tPtr,int64_t now,uint64_t nwid,const MulticastGroup &mg,MulticastGroupStatus &gs,const Address &member)
{
	// assumes _groups_m is locked

	// Do not add self -- even if someone else returns it
	if (member == RR->identity.address())
		return;

	std::vector<MulticastGroupMember>::iterator m(std::lower_bound(gs.members.begin(),gs.members.end(),member));
	if (m != gs.members.end()) {
		if (m->address == member) {
			m->timestamp = now;
			return;
		}
		gs.members.insert(m,MulticastGroupMember(member,now));
	} else {
		gs.members.push_back(MulticastGroupMember(member,now));
	}

	for(std::list<OutboundMulticast>::iterator tx(gs.txQueue.begin());tx!=gs.txQueue.end();) {
		if (tx->atLimit())
			gs.txQueue.erase(tx++);
		else {
			tx->sendIfNew(RR,tPtr,member);
			if (tx->atLimit())
				gs.txQueue.erase(tx++);
			else ++tx;
		}
	}
}

} // namespace ZeroTier
