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

    unsigned int _concurrency;
    // pool
    std::vector<PacketRecord*> _rxPacketVector;
    std::vector<std::thread> _rxPacketThreads;
    Mutex _rxPacketVector_m, _rxPacketThreads_m;

    std::vector<std::thread> _rxThreads;
    unsigned int _rxThreadCount;
    bool _enabled;
};

}   // namespace ZeroTier

#endif   // ZT_PACKET_MULTIPLEXER_HPP