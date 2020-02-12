/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2024-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#include "VL2.hpp"
#include "RuntimeEnvironment.hpp"
#include "Node.hpp"
#include "VL1.hpp"
#include "Topology.hpp"
#include "Peer.hpp"
#include "Path.hpp"
#include "Network.hpp"
#include "MAC.hpp"

namespace ZeroTier {

VL2::VL2(const RuntimeEnvironment *renv)
{
}

VL2::~VL2()
{
}

bool VL2::onLocalEthernet(void *tPtr,const SharedPtr<Network> &network,const MAC &from,const MAC &to,unsigned int etherType,unsigned int vlanId,const void *data,unsigned int len)
{
}

bool VL2::_FRAME(void *tPtr,const SharedPtr<Path> &path,SharedPtr<Peer> &peer,Buf &pkt,int packetSize)
{
}

bool VL2::_EXT_FRAME(void *tPtr,const SharedPtr<Path> &path,SharedPtr<Peer> &peer,Buf &pkt,int packetSize)
{
}

bool VL2::_MULTICAST_LIKE(void *tPtr,const SharedPtr<Path> &path,SharedPtr<Peer> &peer,Buf &pkt,int packetSize)
{
}

bool VL2::_NETWORK_CREDENTIALS(void *tPtr,const SharedPtr<Path> &path,SharedPtr<Peer> &peer,Buf &pkt,int packetSize)
{
}

bool VL2::_NETWORK_CONFIG_REQUEST(void *tPtr,const SharedPtr<Path> &path,SharedPtr<Peer> &peer,Buf &pkt,int packetSize)
{
}

bool VL2::_NETWORK_CONFIG(void *tPtr,const SharedPtr<Path> &path,SharedPtr<Peer> &peer,Buf &pkt,int packetSize)
{
}

bool VL2::_MULTICAST_GATHER(void *tPtr,const SharedPtr<Path> &path,SharedPtr<Peer> &peer,Buf &pkt,int packetSize)
{
}

bool VL2::_MULTICAST_FRAME_deprecated(void *tPtr,const SharedPtr<Path> &path,SharedPtr<Peer> &peer,Buf &pkt,int packetSize)
{
}

bool VL2::_MULTICAST(void *tPtr,const SharedPtr<Path> &path,SharedPtr<Peer> &peer,Buf &pkt,int packetSize)
{
}

} // namespace ZeroTier
