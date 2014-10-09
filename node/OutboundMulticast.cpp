/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2011-2014  ZeroTier Networks LLC
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
 *
 * --
 *
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

#include "Constants.hpp"
#include "RuntimeEnvironment.hpp"
#include "OutboundMulticast.hpp"
#include "Switch.hpp"
#include "NodeConfig.hpp"
#include "Network.hpp"
#include "CertificateOfMembership.hpp"
#include "Utils.hpp"

namespace ZeroTier {

void OutboundMulticast::init(
	uint64_t timestamp,
	const Address &self,
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

	_packetNoCom.setSource(self);
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

		_packetWithCom.setSource(self);
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
		SharedPtr<Network> network(RR->nc->network(_nwid));
		if (network->peerNeedsOurMembershipCertificate(toAddr,Utils::now())) {
			_packetWithCom.newInitializationVector();
			_packetWithCom.setDestination(toAddr);
			RR->sw->send(_packetWithCom,true);
			return;
		}
	}
	_packetNoCom.newInitializationVector();
	_packetNoCom.setDestination(toAddr);
	RR->sw->send(_packetNoCom,true);
}

} // namespace ZeroTier
