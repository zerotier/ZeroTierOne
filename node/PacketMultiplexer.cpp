/*
 * Copyright (c)2013-2021 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#include "PacketMultiplexer.hpp"

#include "Node.hpp"
#include "RuntimeEnvironment.hpp"

#include <stdio.h>
#include <stdlib.h>

namespace ZeroTier {

void PacketMultiplexer::putFrame(void* tPtr, uint64_t nwid, void** nuptr, const MAC& source, const MAC& dest, unsigned int etherType, unsigned int vlanId, const void* data, unsigned int len, unsigned int flowId)
{
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
	//fprintf(stderr, "bucket=%d\n", bucket);
	_rxPacketQueues[bucket]->postLimit(packet, 2048);
}

PacketMultiplexer::PacketMultiplexer(const RuntimeEnvironment* renv)
{
	RR = renv;
	bool _enablePinning = false;
	char* pinningVar = std::getenv("ZT_CPU_PINNING");
	if (pinningVar) {
		int tmp = atoi(pinningVar);
		if (tmp > 0) {
			_enablePinning = true;
		}
	}

    _concurrency = 1;
	char* concurrencyVar = std::getenv("ZT_PACKET_PROCESSING_CONCURRENCY");
	if (concurrencyVar) {
		int tmp = atoi(concurrencyVar);
		if (tmp > 0) {
			_concurrency = tmp;
		}
		else {
			_concurrency = std::max((unsigned int)1, std::thread::hardware_concurrency() / 2);
		}
	}
	else {
		_concurrency = std::max((unsigned int)1, std::thread::hardware_concurrency() / 2);
	}

	for (unsigned int i = 0; i < _concurrency; ++i) {
		fprintf(stderr, "reserved queue for thread %d\n", i);
		_rxPacketQueues.push_back(new BlockingQueue<PacketRecord*>());
	}

	// Each thread picks from its own queue to feed into the core
	for (unsigned int i = 0; i < _concurrency; ++i) {
		_rxThreads.push_back(std::thread([this, i, _enablePinning]() {
			fprintf(stderr, "created post-decode packet ingestion thread %d\n", i);

			PacketRecord* packet = nullptr;
			for (;;) {
				if (! _rxPacketQueues[i]->get(packet)) {
					break;
				}
				if (! packet) {
					break;
				}

                //fprintf(stderr, "popped packet from queue %d\n", i);

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
};

}	// namespace ZeroTier