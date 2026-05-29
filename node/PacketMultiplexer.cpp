/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c) ZeroTier, Inc.
 * https://www.zerotier.com/
 */

#include "PacketMultiplexer.hpp"

#include "Constants.hpp"
#include "Node.hpp"
#include "RuntimeEnvironment.hpp"

#include <stdio.h>
#include <stdlib.h>

namespace ZeroTier {

PacketMultiplexer::PacketMultiplexer(const RuntimeEnvironment* renv)
{
	RR = renv;
};

void PacketMultiplexer::putFrame(void* tPtr, uint64_t nwid, void** nuptr, const MAC& source, const MAC& dest, unsigned int etherType, unsigned int vlanId, const void* data, unsigned int len, unsigned int flowId)
{
#if defined(__APPLE__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__WINDOWS__)
	RR->node->putFrame(tPtr, nwid, nuptr, source, dest, etherType, vlanId, (const void*)data, len);
	return;
#endif

	if (! _enabled) {
		RR->node->putFrame(tPtr, nwid, nuptr, source, dest, etherType, vlanId, (const void*)data, len);
		return;
	}

	PacketRecord* packet;
	_rxPacketVector_m.lock();
	if (_rxPacketVector.empty()) {
		packet = new PacketRecord;
	}
	else {
		packet = _rxPacketVector.back();
		_rxPacketVector.pop_back();
	}
	_rxPacketVector_m.unlock();

	packet->tPtr = tPtr;
	packet->nwid = nwid;
	packet->nuptr = nuptr;
	packet->source = source.toInt();
	packet->dest = dest.toInt();
	packet->etherType = etherType;
	packet->vlanId = vlanId;
	packet->len = len;
	packet->flowId = flowId;
	memcpy(packet->data, data, len);

	int bucket = flowId % _concurrency;
	_rxPacketQueues[bucket]->postLimit(packet, 2048);
}

void PacketMultiplexer::setUpPostDecodeReceiveThreads(unsigned int concurrency, bool cpuPinningEnabled)
{
#if defined(__APPLE__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__WINDOWS__)
	return;
#endif
	_enabled = true;
	_concurrency = concurrency;

	for (unsigned int i = 0; i < _concurrency; ++i) {
		fprintf(stderr, "Reserved queue for thread %d\n", i);
		_rxPacketQueues.push_back(new BlockingQueue<PacketRecord*>());
	}

	// Each thread picks from its own queue to feed into the core
	for (unsigned int i = 0; i < _concurrency; ++i) {
		_rxThreads.push_back(std::thread([this, i]() {
			fprintf(stderr, "Created post-decode packet ingestion thread %d\n", i);

			PacketRecord* packet = nullptr;
			for (;;) {
				if (! _rxPacketQueues[i]->get(packet)) {
					break;
				}
				if (! packet) {
					break;
				}

				// fprintf(stderr, "popped packet from queue %d\n", i);

				MAC sourceMac = MAC(packet->source);
				MAC destMac = MAC(packet->dest);

				RR->node->putFrame(packet->tPtr, packet->nwid, packet->nuptr, sourceMac, destMac, packet->etherType, 0, (const void*)packet->data, packet->len);
				{
					Mutex::Lock l(_rxPacketVector_m);
					_rxPacketVector.push_back(packet);
				}
				/*
				if (ZT_ResultCode_isFatal(err)) {
					char tmp[256];
					OSUtils::ztsnprintf(tmp, sizeof(tmp), "error processing packet: %d", (int)err);
					Mutex::Lock _l(_termReason_m);
					_termReason = ONE_UNRECOVERABLE_ERROR;
					_fatalErrorMessage = tmp;
					this->terminate();
					break;
				}
				*/
			}
		}));
	}
}

}	// namespace ZeroTier
