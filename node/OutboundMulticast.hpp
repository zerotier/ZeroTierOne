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

namespace ZeroTier {

class CertificateOfMembership;
class RuntimeEnvironment;

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
	ZT_ALWAYS_INLINE OutboundMulticast() {}

	/**
	 * Initialize outbound multicast
	 *
	 * @param RR Runtime environment
	 * @param timestamp Creation time
	 * @param nwid Network ID
	 * @param disableCompression Disable compression of frame payload
	 * @param limit Multicast limit for desired number of packets to send
	 * @param src Source MAC address of frame or NULL to imply compute from sender ZT address
	 * @param dest Destination multicast group (MAC + ADI)
	 * @param etherType 16-bit Ethernet type ID
	 * @param payload Data
	 * @param len Length of data
	 * @throws std::out_of_range Data too large to fit in a MULTICAST_FRAME
	 */
	void init(
		const RuntimeEnvironment *RR,
		uint64_t timestamp,
		uint64_t nwid,
		bool disableCompression,
		const MAC &src,
		const MulticastGroup &dest,
		unsigned int etherType,
		const void *payload,
		unsigned int len);

	/**
	 * @return Multicast creation time
	 */
	ZT_ALWAYS_INLINE uint64_t timestamp() const { return _timestamp; }

	/**
	 * @param now Current time
	 * @return True if this multicast is expired (has exceeded transmit timeout)
	 */
	ZT_ALWAYS_INLINE bool expired(int64_t now) const { return ((now - _timestamp) >= ZT_MULTICAST_TRANSMIT_TIMEOUT); }

	/**
	 * @return True if this outbound multicast has been sent to enough peers
	 */
	ZT_ALWAYS_INLINE bool atLimit() const { return (_alreadySentTo.size() >= _limit); }

	/**
	 * Just send without checking log
	 *
	 * @param RR Runtime environment
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param toAddr Destination address
	 */
	void sendOnly(const RuntimeEnvironment *RR,void *tPtr,const Address &toAddr);

	/**
	 * Just send and log but do not check sent log
	 *
	 * @param RR Runtime environment
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param toAddr Destination address
	 */
	ZT_ALWAYS_INLINE void sendAndLog(const RuntimeEnvironment *RR,void *tPtr,const Address &toAddr)
	{
		_alreadySentTo.insert(std::upper_bound(_alreadySentTo.begin(),_alreadySentTo.end(),toAddr),toAddr); // sorted insert
		sendOnly(RR,tPtr,toAddr);
	}

	/**
	 * Log an address as having been used so we will not send there in the future
	 *
	 * @param toAddr Address to log as sent
	 */
	ZT_ALWAYS_INLINE void logAsSent(const Address &toAddr)
	{
		_alreadySentTo.insert(std::upper_bound(_alreadySentTo.begin(),_alreadySentTo.end(),toAddr),toAddr); // sorted insert
	}

	/**
	 * Try to send this to a given peer if it hasn't been sent to them already
	 *
	 * @param RR Runtime environment
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param toAddr Destination address
	 * @return True if address is new and packet was sent to switch, false if duplicate
	 */
	ZT_ALWAYS_INLINE bool sendIfNew(const RuntimeEnvironment *RR,void *tPtr,const Address &toAddr)
	{
		if (!std::binary_search(_alreadySentTo.begin(),_alreadySentTo.end(),toAddr)) {
			sendAndLog(RR,tPtr,toAddr);
			return true;
		} else {
			return false;
		}
	}

private:
	uint64_t _timestamp;
	uint64_t _nwid;
	MAC _macSrc;
	MAC _macDest;
	unsigned int _frameLen;
	unsigned int _etherType;
	Packet _packet,_tmp;
	std::vector<Address> _alreadySentTo;
	uint8_t _frameData[ZT_MAX_MTU];
};

} // namespace ZeroTier

#endif
