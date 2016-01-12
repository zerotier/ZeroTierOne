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
#include "CertificateOfMembership.hpp"
#include "Node.hpp"

namespace ZeroTier {

void OutboundMulticast::init(
	const RuntimeEnvironment *RR,
	uint64_t timestamp,
	uint64_t nwid,
	const CertificateOfMembership *com,
	unsigned int limit,
	unsigned int gatherLimit,
	const MAC &src,
	const MulticastGroup &dest,
	unsigned int etherType,
	const void *payload,
	unsigned int len)
{
	_timestamp = timestamp;
	_nwid = nwid;
	_limit = limit;

	uint8_t flags = 0;
	if (gatherLimit) flags |= 0x02;
	if (src) flags |= 0x04;

	/*
	TRACE(">>MC %.16llx INIT %.16llx/%s limit %u gatherLimit %u from %s to %s length %u com==%d",
		(unsigned long long)this,
		nwid,
		dest.toString().c_str(),
		limit,
		gatherLimit,
		(src) ? src.toString().c_str() : MAC(RR->identity.address(),nwid).toString().c_str(),
		dest.toString().c_str(),
		len,
		(com) ? 1 : 0);
	*/

	_packetNoCom.setSource(RR->identity.address());
	_packetNoCom.setVerb(Packet::VERB_MULTICAST_FRAME);
	_packetNoCom.append((uint64_t)nwid);
	_packetNoCom.append(flags);
	if (gatherLimit) _packetNoCom.append((uint32_t)gatherLimit);
	if (src) src.appendTo(_packetNoCom);
	dest.mac().appendTo(_packetNoCom);
	_packetNoCom.append((uint32_t)dest.adi());
	_packetNoCom.append((uint16_t)etherType);
	_packetNoCom.append(payload,len);
	_packetNoCom.compress();

	if (com) {
		_haveCom = true;
		flags |= 0x01;

		_packetWithCom.setSource(RR->identity.address());
		_packetWithCom.setVerb(Packet::VERB_MULTICAST_FRAME);
		_packetWithCom.append((uint64_t)nwid);
		_packetWithCom.append(flags);
		com->serialize(_packetWithCom);
		if (gatherLimit) _packetWithCom.append((uint32_t)gatherLimit);
		if (src) src.appendTo(_packetWithCom);
		dest.mac().appendTo(_packetWithCom);
		_packetWithCom.append((uint32_t)dest.adi());
		_packetWithCom.append((uint16_t)etherType);
		_packetWithCom.append(payload,len);
		_packetWithCom.compress();
	} else _haveCom = false;
}

void OutboundMulticast::sendOnly(const RuntimeEnvironment *RR,const Address &toAddr)
{
	if (_haveCom) {
		SharedPtr<Peer> peer(RR->topology->getPeer(toAddr));
		if ( (!peer) || (peer->needsOurNetworkMembershipCertificate(_nwid,RR->node->now(),true)) ) {
			//TRACE(">>MC %.16llx -> %s (with COM)",(unsigned long long)this,toAddr.toString().c_str());
			_packetWithCom.newInitializationVector();
			_packetWithCom.setDestination(toAddr);
			RR->sw->send(_packetWithCom,true,_nwid);
			return;
		}
	}

	//TRACE(">>MC %.16llx -> %s (without COM)",(unsigned long long)this,toAddr.toString().c_str());
	_packetNoCom.newInitializationVector();
	_packetNoCom.setDestination(toAddr);
	RR->sw->send(_packetNoCom,true,_nwid);
}

} // namespace ZeroTier
