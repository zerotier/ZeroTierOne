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

#include "Constants.hpp"
#include "RuntimeEnvironment.hpp"
#include "OutboundMulticast.hpp"
#include "Switch.hpp"
#include "Network.hpp"
#include "Node.hpp"
#include "Peer.hpp"
#include "Topology.hpp"

namespace ZeroTier {

void OutboundMulticast::init(
	const RuntimeEnvironment *RR,
	uint64_t timestamp,
	uint64_t nwid,
	bool disableCompression,
	const MAC &src,
	const MulticastGroup &dest,
	unsigned int etherType,
	const void *payload,
	unsigned int len)
{
	uint8_t flags = 0;

	_timestamp = timestamp;
	_nwid = nwid;
	if (src) {
		_macSrc = src;
		flags |= 0x04;
	} else {
		_macSrc.fromAddress(RR->identity.address(),nwid);
	}
	_macDest = dest.mac();
	_frameLen = (len < ZT_MAX_MTU) ? len : ZT_MAX_MTU;
	_etherType = etherType;

	_packet.setSource(RR->identity.address());
	_packet.setVerb(Packet::VERB_MULTICAST_FRAME);
	_packet.append((uint64_t)nwid);
	_packet.append(flags);
	if (src) src.appendTo(_packet);
	dest.mac().appendTo(_packet);
	_packet.append((uint32_t)dest.adi());
	_packet.append((uint16_t)etherType);
	_packet.append(payload,_frameLen);
	if (!disableCompression)
		_packet.compress();

	memcpy(_frameData,payload,_frameLen);
}

void OutboundMulticast::sendOnly(const RuntimeEnvironment *RR,void *tPtr,const Address &toAddr)
{
	const SharedPtr<Network> nw(RR->node->network(_nwid));
	uint8_t QoSBucket = 255; // Dummy value
	if ((nw)&&(nw->filterOutgoingPacket(tPtr,true,RR->identity.address(),toAddr,_macSrc,_macDest,_frameData,_frameLen,_etherType,0,QoSBucket))) {
		nw->pushCredentialsIfNeeded(tPtr,toAddr,RR->node->now());
		_packet.newInitializationVector();
		_packet.setDestination(toAddr);
		RR->node->expectReplyTo(_packet.packetId());
		_tmp = _packet;
		RR->sw->send(tPtr,_tmp,true);
	}
}

} // namespace ZeroTier
