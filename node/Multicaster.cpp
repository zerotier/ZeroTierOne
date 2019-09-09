/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2023-01-01
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
	_groups(32) {}

Multicaster::~Multicaster() {}

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
#if 0
	unsigned long idxbuf[4096];
	unsigned long *indexes = idxbuf;

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
				unsigned long j = (unsigned long)Utils::random() % (i + 1);
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

				explicitGatherPeers[numExplicitGatherPeers++] = network->controller();

				/*
				Address ac[ZT_MAX_NETWORK_SPECIALISTS];
				const unsigned int accnt = network->config().alwaysContactAddresses(ac);
				unsigned int shuffled[ZT_MAX_NETWORK_SPECIALISTS];
				for(unsigned int i=0;i<accnt;++i)
					shuffled[i] = i;
				for(unsigned int i=0,k=accnt>>1;i<k;++i) {
					const uint64_t x = Utils::random();
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
				*/

				/*
				std::vector<Address> anchors(network->config().anchors());
				for(std::vector<Address>::const_iterator a(anchors.begin());a!=anchors.end();++a) {
					if (*a != RR->identity.address()) {
						explicitGatherPeers[numExplicitGatherPeers++] = *a;
						if (numExplicitGatherPeers == 16)
							break;
					}
				}
				*/

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
#endif
}

void Multicaster::clean(int64_t now)
{
#if 0
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
#endif
}

} // namespace ZeroTier
