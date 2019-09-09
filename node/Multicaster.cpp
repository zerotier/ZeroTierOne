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
#include "Topology.hpp"
#include "Switch.hpp"

namespace ZeroTier {

Multicaster::Multicaster(const RuntimeEnvironment *renv) :
	RR(renv),
	_groups(32) {}

Multicaster::~Multicaster() {}

void Multicaster::send(
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
	static const unsigned int PRIMES[16] = { 2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53 };

	if (unlikely(len > ZT_MAX_MTU)) return; // sanity check

	const NetworkConfig &config = network->config();
	if (config.multicastLimit == 0) return; // multicast disabled
	Address bridges[ZT_MAX_NETWORK_SPECIALISTS],multicastReplicators[ZT_MAX_NETWORK_SPECIALISTS];
	unsigned int bridgeCount = 0,multicastReplicatorCount = 0;
	for(unsigned int i=0;i<config.specialistCount;++i) {
		if ((config.specialists[i] & ZT_NETWORKCONFIG_SPECIALIST_TYPE_ACTIVE_BRIDGE) != 0)
			bridges[bridgeCount++] = config.specialists[i];
		if ((config.specialists[i] & ZT_NETWORKCONFIG_SPECIALIST_TYPE_MULTICAST_REPLICATOR) != 0)
			multicastReplicators[multicastReplicatorCount++] = config.specialists[i];
	}

	std::vector< std::pair<int64_t,Address> > recipients;
	bool needMoar = false;
	for(unsigned int i=0;i<bridgeCount;++i)
		recipients.push_back(std::pair<int64_t,Address>(9223372036854775807LL,bridges[i]));
	{
		Mutex::Lock l2(_groups_l);
		_getMembersByTime(network->id(),mg,recipients);
	}
	std::sort(recipients.begin() + bridgeCount,recipients.end(),std::greater< std::pair<int64_t,Address> >());
	recipients.erase(std::unique(recipients.begin(),recipients.end()),recipients.end());
	if (recipients.size() > config.multicastLimit) {
		recipients.resize(config.multicastLimit);
	} else if (recipients.size() < config.multicastLimit) {
		needMoar = true;
	}

	_txQueue_l.lock();
	_OM *om = &(_txQueue[_txQueuePtr++ % ZT_TX_QUEUE_SIZE]);
	Mutex::Lock ql(om->lock);
	_txQueue_l.unlock();

	om->nwid = network->id();
	om->src = src;
	om->mg = mg;
	om->etherType = etherType;
	om->dataSize = len;
	memcpy(om->data,data,len);

	if (existingBloom) {
		om->bloomFilterMultiplier = existingBloomMultiplier;
		memcpy(om->bloomFilter,existingBloom,sizeof(om->bloomFilter));
	} else {
		om->bloomFilterMultiplier = 1;
		memset(om->bloomFilter,0,sizeof(om->bloomFilter));

		if (recipients.size() > 1) {
			unsigned int mult = 1;
			unsigned int bestMultColl = 0xffffffff;
			for(int k=0;k<16;++k) { // 16 == arbitrary limit on iterations for this search, also must be <= size of PRIMES
				unsigned int coll = 0;
				for(std::vector< std::pair<int64_t,Address> >::const_iterator r(recipients.begin());r!=recipients.end();++r) {
					const unsigned int bfi = mult * (unsigned int)r->second.toInt();
					const unsigned int byte = (bfi >> 3) % sizeof(om->bloomFilter);
					const uint8_t bit = 1 << (bfi & 7);
					coll += ((om->bloomFilter[byte] & bit) != 0);
					om->bloomFilter[byte] |= bit;
				}
				memset(om->bloomFilter,0,sizeof(om->bloomFilter));

				if (coll <= bestMultColl) {
					om->bloomFilterMultiplier = mult;
					if (coll == 0) // perfect score, no need to continue searching
						break;
					bestMultColl = coll;
				}

				mult = PRIMES[k];
			}
		}
	}

	if (multicastReplicatorCount > 0) {
		// SEND
		return;
	}

	SharedPtr<Peer> nextHops[2]; // these by definition are protocol version >= 11
	unsigned int nextHopsBestLatency[2] = { 0xffff,0xffff };
	for(std::vector< std::pair<int64_t,Address> >::const_iterator r(recipients.begin());r!=recipients.end();++r) {
		const unsigned int bfi = om->bloomFilterMultiplier * (unsigned int)r->second.toInt();
		const unsigned int bfbyte = (bfi >> 3) % sizeof(om->bloomFilter);
		const uint8_t bfbit = 1 << (bfi & 7);
		if ((om->bloomFilter[bfbyte] & bfbit) != 0) {
			continue;
		} else {
			SharedPtr<Peer> peer(RR->topology->get(r->second));
			if (peer) {
				if (peer->remoteVersionProtocol() < 11) {
					// SEND

					om->bloomFilter[bfbyte] |= bfbit;
					continue;
				} else {
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

	for(unsigned int nh=0;nh<2;++nh) {
		if (nextHops[nh]) {
			const unsigned int bfi = om->bloomFilterMultiplier * (unsigned int)nextHops[nh]->address().toInt();
			om->bloomFilter[(bfi >> 3) % sizeof(om->bloomFilter)] |= 1 << (bfi & 7);
		}
	}

	for(unsigned int nh=0;nh<2;++nh) {
		if (nextHops[nh]) {
		}
	}
}

void Multicaster::clean(int64_t now)
{
}

} // namespace ZeroTier
