/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c) ZeroTier, Inc.
 * https://www.zerotier.com/
 */

#ifndef ZT_PACKET_MULTIPLEXER_HPP
#define ZT_PACKET_MULTIPLEXER_HPP

#include "../osdep/BlockingQueue.hpp"
#include "MAC.hpp"
#include "Mutex.hpp"
#include "RuntimeEnvironment.hpp"

#include <thread>
#include <vector>

namespace ZeroTier {

struct PacketRecord {
	void* tPtr;
	uint64_t nwid;
	void** nuptr;
	uint64_t source;
	uint64_t dest;
	unsigned int etherType;
	unsigned int vlanId;
	uint8_t data[ZT_MAX_MTU];
	unsigned int len;
	unsigned int flowId;
};

class PacketMultiplexer {
  public:
	const RuntimeEnvironment* RR;

	PacketMultiplexer(const RuntimeEnvironment* renv);

	void setUpPostDecodeReceiveThreads(unsigned int concurrency, bool cpuPinningEnabled);

	void putFrame(void* tPtr, uint64_t nwid, void** nuptr, const MAC& source, const MAC& dest, unsigned int etherType, unsigned int vlanId, const void* data, unsigned int len, unsigned int flowId);

	std::vector<BlockingQueue<PacketRecord*>*> _rxPacketQueues;

	unsigned int _concurrency = 0;
	// pool
	std::vector<PacketRecord*> _rxPacketVector;
	std::vector<std::thread> _rxPacketThreads;
	Mutex _rxPacketVector_m, _rxPacketThreads_m;

	std::vector<std::thread> _rxThreads;
	bool _enabled = false;
};

}	// namespace ZeroTier

#endif	 // ZT_PACKET_MULTIPLEXER_HPP
