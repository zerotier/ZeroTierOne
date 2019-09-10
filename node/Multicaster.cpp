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
#include "Network.hpp"
#include "Membership.hpp"
#include "Topology.hpp"
#include "Switch.hpp"

namespace ZeroTier {

Multicaster::Multicaster(const RuntimeEnvironment *renv) :
	RR(renv),
	_groups(32) {}

Multicaster::~Multicaster() {}

unsigned int Multicaster::send(
	void *tPtr,
	int64_t now,
	const SharedPtr<Network> &network,
	const MulticastGroup &mg,
	const MAC &src,
	unsigned int etherType,
	const unsigned int existingBloomMultiplier,
	const uint8_t existingBloom[ZT_MULTICAST_BLOOM_FILTER_SIZE_BITS / 8],
	const void *const data,
	unsigned int len)
{
	static const unsigned int PRIMES[16] = { 3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59 }; // 2 is skipped as it's even

	std::vector< std::pair<int64_t,Address> > recipients;

	const NetworkConfig &config = network->config();
	if (config.multicastLimit == 0) return; // multicast disabled

	Address specialists[ZT_MAX_NETWORK_SPECIALISTS],multicastReplicators[ZT_MAX_NETWORK_SPECIALISTS];
	unsigned int specialistCount = 0,multicastReplicatorCount = 0,bridgeCount = 0;
	bool amMulticastReplicator = false;
	for(unsigned int i=0;i<config.specialistCount;++i) {
		if (RR->identity.address() == config.specialists[i]) {
			amMulticastReplicator |= ((config.specialists[i] & ZT_NETWORKCONFIG_SPECIALIST_TYPE_MULTICAST_REPLICATOR) != 0);
		} else {
			specialists[specialistCount++] = config.specialists[i];
			if ((config.specialists[i] & ZT_NETWORKCONFIG_SPECIALIST_TYPE_ACTIVE_BRIDGE) != 0) {
				recipients.push_back(std::pair<int64_t,Address>(0,config.specialists[i]));
				++bridgeCount;
			} if ((config.specialists[i] & ZT_NETWORKCONFIG_SPECIALIST_TYPE_MULTICAST_REPLICATOR) != 0) {
				multicastReplicators[multicastReplicatorCount++] = config.specialists[i];
			}
		}
	}
	std::sort(&(specialists[0]),&(specialists[specialistCount])); // for binary search

	int64_t lastGather = 0;
	_K groupKey(network->id(),mg);
	{
		Mutex::Lock l(_groups_l);
		const _G *const g = _groups.get(groupKey);
		if (g) {
			lastGather = g->lastGather;
			recipients.reserve(recipients.size() + g->members.size());
			Hashtable< Address,int64_t >::Iterator mi(const_cast<_G *>(g)->members);
			Address *mik = nullptr;
			int64_t *miv = nullptr;
			while (mi.next(mik,miv)) {
				if (!std::binary_search(&(specialists[0]),&(specialists[specialistCount]),*mik))
					recipients.push_back(std::pair<int64_t,Address>(*miv,*mik));
			}
		}
	}

	// Sort recipients, maintaining bridges first in list
	std::sort(recipients.begin() + bridgeCount,recipients.end(),std::greater< std::pair<int64_t,Address> >());

	// Gather new recipients periodically, being more aggressive if we have none.
	if ((now - lastGather) > (recipients.empty() ? 5000 : ZT_MULTICAST_GATHER_PERIOD)) {
		{
			Mutex::Lock l(_groups_l);
			_groups[groupKey].lastGather = now;
		}

		Packet outp(network->controller(),RR->identity.address(),Packet::VERB_MULTICAST_GATHER);
		outp.append(network->id());
		outp.append((uint8_t)0);
		mg.mac().appendTo(outp);
		outp.append(mg.adi());
		outp.append((uint32_t)0xffffffff);
		RR->sw->send(tPtr,outp,true);

		for(unsigned int i=0;i<specialistCount;++i) {
			outp.newInitializationVector();
			outp.setDestination(specialists[i]);
			RR->sw->send(tPtr,outp,true);
		}

		// LEGACY: roots may know about older versions' multicast subscriptions but
		// the root's role here is being phased out.
		SharedPtr<Peer> root(RR->topology->root(now));
		if (root) {
			outp.newInitializationVector();
			outp.setDestination(root->address());
			outp.armor(root->key(),true);
			root->sendDirect(tPtr,outp.data(),outp.size(),now,true);
		}
	}

	if (recipients.empty())
		return 0;

	unsigned int sentCount = 0;

	uint64_t bloomFilter[ZT_MULTICAST_BLOOM_FILTER_SIZE_BITS / 64];
	unsigned int bloomMultiplier;
	if (existingBloom) {
		memcpy(bloomFilter,existingBloom,sizeof(bloomFilter));
		bloomMultiplier = existingBloomMultiplier;
	} else {
		memset(bloomFilter,0,sizeof(bloomFilter));
		bloomMultiplier = 1;

		// Iteratively search for a bloom multiplier that results in no collisions
		// among known recipients. Usually the first iteration is good unless
		// the recipient set is quite large.
		if (recipients.size() > 1) {
			unsigned long bestMultColl = 0xffffffff;
			for(int k=0;k<16;++k) { // 16 == arbitrary limit on iterations for this search, also must be <= size of PRIMES
				const unsigned int mult = PRIMES[k];
				unsigned long coll = 0;
				for(std::vector< std::pair<int64_t,Address> >::const_iterator r(recipients.begin());r!=recipients.end();++r) {
					const unsigned int bfi = mult * (unsigned int)r->second.toInt();
					const unsigned int byte = (bfi >> 3) % sizeof(bloomFilter);
					const uint8_t bit = 1 << (bfi & 7);
					coll += ((((uint8_t *)bloomFilter)[byte] & bit) != 0);
					((uint8_t *)bloomFilter)[byte] |= bit;
				}
				memset(bloomFilter,0,sizeof(bloomFilter));

				if (coll <= bestMultColl) {
					bloomMultiplier = mult;
					if (coll == 0) // perfect score, no need to continue searching
						break;
					bestMultColl = coll;
				}
			}
		}
	}

	// See if there is a multicast replicator, trying to pick the fastest/best one.
	Address bestReplicator;
	if (multicastReplicatorCount > 0) {
		unsigned int bestReplicatorLatency = 0xffff;
		for(unsigned int i=0;i<multicastReplicatorCount;++i) {
			const unsigned int bfi = bloomMultiplier * (unsigned int)multicastReplicators[i].toInt();
			if ((((uint8_t *)bloomFilter)[(bfi >> 3) % sizeof(bloomFilter)] & (1 << (bfi & 7))) == 0) {
				SharedPtr<Peer> peer(RR->topology->get(multicastReplicators[i]));
				if (peer) {
					const unsigned int lat = peer->latency(now);
					if (lat <= bestReplicatorLatency) {
						bestReplicator = peer->address();
						bestReplicatorLatency = lat;
					}
				} else if (!bestReplicator) {
					bestReplicator = multicastReplicators[i];
				}
			}
		}
	}

	// If this is a multicast replicator, aggressively replicate. Multicast
	// replicators are not subject to send count limits.
	if (amMulticastReplicator) {
		std::vector< std::pair< int,Address > > byLatency;
		for(std::vector< std::pair<int64_t,Address> >::const_iterator r(recipients.begin());r!=recipients.end();++r) {
			const unsigned int bfi = bloomMultiplier * (unsigned int)r->second.toInt();
			if ((((uint8_t *)bloomFilter)[(bfi >> 3) % sizeof(bloomFilter)] & (1 << (bfi & 7))) == 0) {
				SharedPtr<Peer> peer(RR->topology->get(r->second));
				byLatency.push_back(std::pair< int,Address >((peer) ? (int)peer->latency(now) : 0xffff,r->second));
			}
		}
		std::sort(byLatency.begin(),byLatency.end());

		unsigned long cnt = byLatency.size();
		if (bestReplicator)
			cnt /= 2; // send to only the best half of the latency-sorted population if there are more replicators
		for(unsigned long i=0;i<cnt;++i) {
			const unsigned int bfi = bloomMultiplier * (unsigned int)byLatency[i].second.toInt();
			((uint8_t *)bloomFilter)[(bfi >> 3) % sizeof(bloomFilter)] |= 1 << (bfi & 7);

			Packet outp(byLatency[i].second,RR->identity.address(),Packet::VERB_MULTICAST_FRAME);
			outp.append(network->id());
			outp.append((uint8_t)0x04);
			src.appendTo(outp);
			mg.mac().appendTo(outp);
			outp.append(mg.adi());
			outp.append((uint16_t)etherType);
			outp.append(data,len);
			outp.compress();
			RR->sw->send(tPtr,outp,true);

			++sentCount;
		}
	}

	// Forward to the next multicast replicator, if any.
	if (bestReplicator) {
		const unsigned int bfi = bloomMultiplier * (unsigned int)bestReplicator.toInt();
		((uint8_t *)bloomFilter)[(bfi >> 3) % sizeof(bloomFilter)] |= 1 << (bfi & 7);

		Packet outp(bestReplicator,RR->identity.address(),Packet::VERB_MULTICAST_FRAME);
		outp.append((uint8_t)(0x04 | 0x08));
		RR->identity.address().appendTo(outp);
		outp.append((uint16_t)bloomMultiplier);
		outp.append((uint16_t)sizeof(bloomFilter));
		outp.append(((uint8_t *)bloomFilter),sizeof(bloomFilter));
		src.appendTo(outp);
		mg.mac().appendTo(outp);
		outp.append(mg.adi());
		outp.append((uint16_t)etherType);
		outp.append(data,len);
		outp.compress();
		RR->sw->send(tPtr,outp,true);

		++sentCount;
	}

	// If this is a multicast replicator, we've already replicated.
	if (amMulticastReplicator)
		return (unsigned int)recipients.size();

	// Find the two best next hops (that have never seen this multicast)
	// that are newer version nodes.
	SharedPtr<Peer> nextHops[2];
	unsigned int nextHopsBestLatency[2] = { 0xffff,0xffff };
	for(std::vector< std::pair<int64_t,Address> >::iterator r(recipients.begin());r!=recipients.end();++r) {
		if (r->first >= 0) {
			const unsigned int bfi = bloomMultiplier * (unsigned int)r->second.toInt();
			if ((((uint8_t *)bloomFilter)[(bfi >> 3) % sizeof(bloomFilter)] & (1 << (bfi & 7))) == 0) {
				const SharedPtr<Peer> peer(RR->topology->get(r->second));
				if ((peer)&&(peer->remoteVersionProtocol() >= 11)) {
					r->first = -1; // use this field now to flag as non-legacy
					const unsigned int lat = peer->latency(now);
					for(unsigned int nh=0;nh<2;++nh) {
						if (lat <= nextHopsBestLatency[nh]) {
							nextHopsBestLatency[nh] = lat;
							nextHops[nh] = peer;
							break;
						}
					}
				}
			}
		}
	}

	// Set bits for next hops in bloom filter
	for(unsigned int nh=0;nh<2;++nh) {
		if (nextHops[nh]) {
			const unsigned int bfi = bloomMultiplier * (unsigned int)nextHops[nh]->address().toInt();
			((uint8_t *)bloomFilter)[(bfi >> 3) % sizeof(bloomFilter)] |= 1 << (bfi & 7);
			++sentCount;
		}
	}

	// Send to legacy peers and flag these in bloom filter
	const unsigned int limit = config.multicastLimit + bridgeCount;
	for(std::vector< std::pair<int64_t,Address> >::const_iterator r(recipients.begin());(r!=recipients.end())&&(sentCount<limit);++r) {
		if (r->first >= 0) {
			const unsigned int bfi = bloomMultiplier * (unsigned int)r->second.toInt();
			((uint8_t *)bloomFilter)[(bfi >> 3) % sizeof(bloomFilter)] |= 1 << (bfi & 7);

			Packet outp(r->second,RR->identity.address(),Packet::VERB_MULTICAST_FRAME);
			outp.append(network->id());
			outp.append((uint8_t)0x04);
			src.appendTo(outp);
			mg.mac().appendTo(outp);
			outp.append(mg.adi());
			outp.append((uint16_t)etherType);
			outp.append(data,len);
			outp.compress();
			RR->sw->send(tPtr,outp,true);

			++sentCount;
		}
	}

	// Send to next hops for P2P propagation
	for(unsigned int nh=0;nh<2;++nh) {
		if (nextHops[nh]) {
			Packet outp(nextHops[nh]->address(),RR->identity.address(),Packet::VERB_MULTICAST_FRAME);
			outp.append((uint8_t)(0x04 | 0x08));
			RR->identity.address().appendTo(outp);
			outp.append((uint16_t)bloomMultiplier);
			outp.append((uint16_t)sizeof(bloomFilter));
			outp.append(((uint8_t *)bloomFilter),sizeof(bloomFilter));
			src.appendTo(outp);
			mg.mac().appendTo(outp);
			outp.append(mg.adi());
			outp.append((uint16_t)etherType);
			outp.append(data,len);
			outp.compress();
			RR->sw->send(tPtr,outp,true);
		}
	}

	return (unsigned int)recipients.size();
}

void Multicaster::clean(int64_t now)
{
}

} // namespace ZeroTier
