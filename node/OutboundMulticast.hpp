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

#ifndef ZT_OUTBOUNDMULTICAST_HPP
#define ZT_OUTBOUNDMULTICAST_HPP

#include <stdint.h>

#include <vector>
#include <algorithm>

#include "Constants.hpp"
#include "MAC.hpp"
#include "MulticastGroup.hpp"
#include "Address.hpp"
#include "Packet.hpp"
#include "Switch.hpp"

namespace ZeroTier {

/**
 * An outbound multicast packet
 *
 * This object isn't guarded by a mutex; caller must synchronize access.
 */
class OutboundMulticast
{
public:
	/**
	 * Create an uninitialized outbound multicast
	 *
	 * It must be initialized with init().
	 */
	OutboundMulticast() {}

	/**
	 * Initialize outbound multicast
	 *
	 * @param timestamp Creation time
	 * @param self My ZeroTier address
	 * @param nwid Network ID
	 * @param gatherLimit Number to lazily/implicitly gather with this frame or 0 for none
	 * @param src Source MAC address of frame
	 * @param dest Destination multicast group (MAC + ADI)
	 * @param etherType 16-bit Ethernet type ID
	 * @param payload Data
	 * @param len Length of data
	 * @throws std::out_of_range Data too large to fit in a MULTICAST_FRAME
	 */
	inline void init(uint64_t timestamp,const Address &self,uint64_t nwid,unsigned int gatherLimit,const MAC &src,const MulticastGroup &dest,unsigned int etherType,const void *payload,unsigned int len)
	{
		_timestamp = timestamp;
		_nwid = nwid;
		_source = src;
		_destination = dest;
		_etherType = etherType;
		_packet.setSource(self);
		_packet.setVerb(Packet::VERB_MULTICAST_FRAME);
		_packet.append((uint64_t)nwid);
		_packet.append((char)0); // 0 flags
		_packet.append((uint32_t)gatherLimit); // gather limit -- set before send, start with 0
		_packet.append((uint32_t)dest.adi());
		dest.mac().appendTo(_packet);
		src.appendTo(_packet);
		_packet.append((uint16_t)etherType);
		_packet.append(payload,len);
		_packet.compress();
	}

	/**
	 * @return Multicast creation time
	 */
	inline uint64_t timestamp() const throw() { return _timestamp; }

	/**
	 * @param now Current time
	 * @return True if this multicast is expired (has exceeded transmit timeout)
	 */
	inline bool expired(uint64_t now) const throw() { return ((now - _timestamp) >= ZT_MULTICAST_TRANSMIT_TIMEOUT); }

	/**
	 * @return Number of unique recipients to which this packet has already been sent
	 */
	inline unsigned int sentToCount() const throw() { return (unsigned int)_alreadySentTo.size(); }

	/**
	 * Just send without checking log
	 *
	 * @param sw Switch instance to send packets
	 * @param toAddr Destination address
	 */
	inline void sendOnly(Switch &sw,const Address &toAddr)
	{
		sw.send(Packet(_packet,toAddr),true);
	}

	/**
	 * Just send and log but do not check sent log
	 *
	 * @param sw Switch instance to send packets
	 * @param toAddr Destination address
	 */
	inline void sendAndLog(Switch &sw,const Address &toAddr)
	{
		_alreadySentTo.push_back(toAddr);
		sendOnly(sw,toAddr,gatherLimit);
	}

	/**
	 * Try to send this to a given peer if it hasn't been sent to them already
	 *
	 * @param sw Switch instance to send packets
	 * @param toAddr Destination address
	 * @return True if address is new and packet was sent to switch, false if duplicate
	 */
	inline bool sendIfNew(Switch &sw,const Address &toAddr)
	{
		for(std::vector<Address>::iterator a(_alreadySentTo.begin());a!=_alreadySentTo.end();++a) {
			if (*a == toAddr)
				return false;
		}
		sendAndLog(sw,toAddr,gatherLimit);
		return true;
	}

private:
	uint64_t _timestamp;
	uint64_t _nwid;
	MAC _source;
	MulticastGroup _destination;
	unsigned int _etherType;
	Packet _packet; // packet contains basic structure of MULTICAST_FRAME and payload, is re-used with new IV and addressing each time
	std::vector<Address> _alreadySentTo;
};

} // namespace ZeroTier

#endif
