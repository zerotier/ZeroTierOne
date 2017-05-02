/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2016  ZeroTier, Inc.  https://www.zerotier.com/
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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
	unsigned int limit,
	unsigned int gatherLimit,
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
	_limit = limit;
	_frameLen = (len < ZT_MAX_MTU) ? len : ZT_MAX_MTU;
	_etherType = etherType;

	if (gatherLimit) flags |= 0x02;

	/*
	TRACE(">>MC %.16llx INIT %.16llx/%s limit %u gatherLimit %u from %s to %s length %u",
		(unsigned long long)this,
		nwid,
		dest.toString().c_str(),
		limit,
		gatherLimit,
		(src) ? src.toString().c_str() : MAC(RR->identity.address(),nwid).toString().c_str(),
		dest.toString().c_str(),
		len);
	*/

	_packet.setSource(RR->identity.address());
	_packet.setVerb(Packet::VERB_MULTICAST_FRAME);
	_packet.append((uint64_t)nwid);
	_packet.append(flags);
	if (gatherLimit) _packet.append((uint32_t)gatherLimit);
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
	const Address toAddr2(toAddr);
	if ((nw)&&(nw->filterOutgoingPacket(tPtr,true,RR->identity.address(),toAddr2,_macSrc,_macDest,_frameData,_frameLen,_etherType,0))) {
		//TRACE(">>MC %.16llx -> %s",(unsigned long long)this,toAddr.toString().c_str());
		_packet.newInitializationVector();
		_packet.setDestination(toAddr2);
		RR->node->expectReplyTo(_packet.packetId());

		Packet tmp(_packet); // make a copy of packet so as not to garble the original -- GitHub issue #461
		RR->sw->send(tPtr,tmp,true);
	}
}

} // namespace ZeroTier
