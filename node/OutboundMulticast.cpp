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
#include "OutboundMulticast.hpp"
#include "Switch.hpp"
#include "CertificateOfMembership.hpp"

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
	_source = src;
	_destination = dest;
	_limit = limit;
	_etherType = etherType;

	_packet.setSource(self);
	_packet.setVerb(Packet::VERB_MULTICAST_FRAME);

	self.appendTo(_packet);
	_packet.append((uint64_t)nwid);
	_packet.append((uint8_t)((com) ? 0x01 : 0x00));
	_packet.append((uint32_t)gatherLimit);
	if (com) com->serialize(_packet);
	_packet.append((uint32_t)dest.adi());
	dest.mac().appendTo(_packet);
	src.appendTo(_packet);
	_packet.append((uint16_t)etherType);
	_packet.append(payload,len);

	_packet.compress();
}

void OutboundMulticast::sendOnly(Switch &sw,const Address &toAddr)
{
	sw.send(Packet(_packet,toAddr),true);
}

} // namespace ZeroTier
