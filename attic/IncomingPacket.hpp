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

#ifndef ZT_INCOMINGPACKET_HPP
#define ZT_INCOMINGPACKET_HPP

#include "Path.hpp"
#include "Utils.hpp"
#include "MulticastGroup.hpp"
#include "Peer.hpp"
#include "Buf.hpp"
#include "Protocol.hpp"

/*
 * The big picture:
 *
 * tryDecode gets called for a given fully-assembled packet until it returns
 * true or the packet's time to live has been exceeded, in which case it is
 * discarded as failed decode. Any exception thrown by tryDecode also causes
 * the packet to be discarded.
 *
 * Thus a return of false from tryDecode() indicates that it should be called
 * again. Logic is very simple as to when, and it's in doAnythingWaitingForPeer
 * in Switch. This might be expanded to be more fine grained in the future.
 *
 * A return value of true indicates that the packet is done. tryDecode must
 * never be called again after that.
 */

namespace ZeroTier {

class RuntimeEnvironment;
class Network;

class IncomingPacket
{
public:
	ZT_ALWAYS_INLINE IncomingPacket() {}

	template<typename X>
	ZT_ALWAYS_INLINE void set(const SharedPtr< Buf<X> > &pkt_,const unsigned int pktSize_,const SharedPtr<Path> &path_,const int64_t now_)
	{
		idBE = 0; // initially zero, set when decryption/auth occurs
		receiveTime = now_;
		path = path_;
		pkt = reinterpret_cast< SharedPtr< Buf< Protocol::Header > > >(pkt_);
		size = pktSize_;
		hops = Protocol::packetHops(pkt->data.fields);
	}

	/**
	 * Attempt to decode this packet
	 *
	 * Note that this returns 'true' if processing is complete. This says nothing
	 * about whether the packet was valid. A rejection is 'complete.'
	 *
	 * Once true is returned, this must not be called again. The packet's state
	 * may no longer be valid.
	 *
	 * @param RR Runtime environment
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @return True if decoding and processing is complete, false if caller should try again
	 */
	bool tryDecode(const RuntimeEnvironment *RR,void *tPtr);

	/**
	 * Packet ID in big-endian byte order or 0 if not decrypted/dearmored yet
	 */
	uint64_t idBE;

	/**
	 * Time packet was received
	 */
	int64_t receiveTime;

	/**
	 * Path over which packet was received
	 */
	SharedPtr< Path > path;

	/**
	 * Packet itself
	 */
	SharedPtr< Buf< Protocol::Header > > pkt;

	/**
	 * Size of packet in bytes
	 */
	unsigned int size;

	/**
	 * Hop count for received packet
	 */
	uint8_t hops;
};

} // namespace ZeroTier

#endif
