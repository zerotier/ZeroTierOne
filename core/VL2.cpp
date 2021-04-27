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

#include "VL2.hpp"

#include "Context.hpp"
#include "MAC.hpp"
#include "Network.hpp"
#include "Path.hpp"
#include "Peer.hpp"
#include "Topology.hpp"
#include "VL1.hpp"

namespace ZeroTier {

VL2::VL2(const Context& ctx) : m_ctx(ctx)
{
}

void VL2::onLocalEthernet(CallContext& cc, const SharedPtr<Network>& network, const MAC& from, const MAC& to, const unsigned int etherType, unsigned int vlanId, SharedPtr<Buf>& data, unsigned int len)
{
}

bool VL2::m_FRAME(CallContext& cc, const uint64_t packetId, const unsigned int auth, const SharedPtr<Path>& path, SharedPtr<Peer>& peer, Buf& pkt, int packetSize)
{
}

bool VL2::m_EXT_FRAME(CallContext& cc, const uint64_t packetId, const unsigned int auth, const SharedPtr<Path>& path, SharedPtr<Peer>& peer, Buf& pkt, int packetSize)
{
}

bool VL2::m_MULTICAST_LIKE(CallContext& cc, const uint64_t packetId, const unsigned int auth, const SharedPtr<Path>& path, SharedPtr<Peer>& peer, Buf& pkt, int packetSize)
{
}

bool VL2::m_NETWORK_CREDENTIALS(CallContext& cc, const uint64_t packetId, const unsigned int auth, const SharedPtr<Path>& path, SharedPtr<Peer>& peer, Buf& pkt, int packetSize)
{
}

bool VL2::m_NETWORK_CONFIG_REQUEST(CallContext& cc, const uint64_t packetId, const unsigned int auth, const SharedPtr<Path>& path, SharedPtr<Peer>& peer, Buf& pkt, int packetSize)
{
}

bool VL2::m_NETWORK_CONFIG(CallContext& cc, const uint64_t packetId, const unsigned int auth, const SharedPtr<Path>& path, SharedPtr<Peer>& peer, Buf& pkt, int packetSize)
{
}

bool VL2::m_MULTICAST_GATHER(CallContext& cc, const uint64_t packetId, const unsigned int auth, const SharedPtr<Path>& path, SharedPtr<Peer>& peer, Buf& pkt, int packetSize)
{
}

bool VL2::m_MULTICAST_FRAME_deprecated(CallContext& cc, const uint64_t packetId, const unsigned int auth, const SharedPtr<Path>& path, SharedPtr<Peer>& peer, Buf& pkt, int packetSize)
{
}

bool VL2::m_MULTICAST(CallContext& cc, const uint64_t packetId, const unsigned int auth, const SharedPtr<Path>& path, SharedPtr<Peer>& peer, Buf& pkt, int packetSize)
{
}

}   // namespace ZeroTier
