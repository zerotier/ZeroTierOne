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

#ifndef ZT_N_SWITCH_HPP
#define ZT_N_SWITCH_HPP

#include <map>
#include <set>
#include <vector>
#include <list>

#include "Constants.hpp"
#include "Mutex.hpp"
#include "MAC.hpp"
#include "Packet.hpp"
#include "Utils.hpp"
#include "InetAddress.hpp"
#include "Topology.hpp"
#include "Network.hpp"
#include "SharedPtr.hpp"
#include "IncomingPacket.hpp"
#include "Hashtable.hpp"

namespace ZeroTier {

class RuntimeEnvironment;
class Peer;

/**
 * Core of the distributed Ethernet switch and protocol implementation
 *
 * This class is perhaps a bit misnamed, but it's basically where everything
 * meets. Transport-layer ZT packets come in here, as do virtual network
 * packets from tap devices, and this sends them where they need to go and
 * wraps/unwraps accordingly. It also handles queues and timeouts and such.
 */
class Switch
{
public:
	explicit Switch(const RuntimeEnvironment *renv);

	/**
	 * Called when a packet is received from the real network
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param localSocket Local I/O socket as supplied by external code
	 * @param fromAddr Internet IP address of origin
	 * @param data Packet data
	 * @param len Packet length
	 */
	void onRemotePacket(void *tPtr,int64_t localSocket,const InetAddress &fromAddr,const void *data,unsigned int len);

	/**
	 * Called when a packet comes from a local Ethernet tap
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param network Which network's TAP did this packet come from?
	 * @param from Originating MAC address
	 * @param to Destination MAC address
	 * @param etherType Ethernet packet type
	 * @param vlanId VLAN ID or 0 if none
	 * @param data Ethernet payload
	 * @param len Frame length
	 */
	void onLocalEthernet(void *tPtr,const SharedPtr<Network> &network,const MAC &from,const MAC &to,unsigned int etherType,unsigned int vlanId,const void *data,unsigned int len);

	/**
	 * Send a packet to a ZeroTier address (destination in packet)
	 *
	 * The packet must be fully composed with source and destination but not
	 * yet encrypted. If the destination peer is known the packet
	 * is sent immediately. Otherwise it is queued and a WHOIS is dispatched.
	 *
	 * The packet may be compressed. Compression isn't done here.
	 *
	 * Needless to say, the packet's source must be this node. Otherwise it
	 * won't be encrypted right. (This is not used for relaying.)
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param packet Packet to send (buffer may be modified)
	 * @param encrypt Encrypt packet payload? (always true except for HELLO)
	 */
	void send(void *tPtr,Packet &packet,bool encrypt);

	/**
	 * Request WHOIS on a given address
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param now Current time
	 * @param addr Address to look up
	 */
	void requestWhois(void *tPtr,int64_t now,const Address &addr);

	/**
	 * Run any processes that are waiting for this peer's identity
	 *
	 * Called when we learn of a peer's identity from HELLO, OK(WHOIS), etc.
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param peer New peer
	 */
	void doAnythingWaitingForPeer(void *tPtr,const SharedPtr<Peer> &peer);

	/**
	 * Perform retries and other periodic timer tasks
	 *
	 * This can return a very long delay if there are no pending timer
	 * tasks. The caller should cap this comparatively vs. other values.
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param now Current time
	 * @return Number of milliseconds until doTimerTasks() should be run again
	 */
	unsigned long doTimerTasks(void *tPtr,int64_t now);

private:
	bool _trySend(void *tPtr,Packet &packet,bool encrypt); // packet is modified if return is true

	const RuntimeEnvironment *const RR;
	volatile int64_t _lastCheckedQueues;

	// Time we last sent a WHOIS request for each address
	Hashtable< Address,int64_t > _lastSentWhoisRequest;
	Mutex _lastSentWhoisRequest_m;

	// Packets waiting for WHOIS replies or other decode info or missing fragments
	struct RXQueueEntry
	{
		ZT_ALWAYS_INLINE RXQueueEntry() : timestamp(0) {}
		volatile int64_t timestamp; // 0 if entry is not in use
		volatile uint64_t packetId;
		IncomingPacket frag0; // head of packet
		Packet::Fragment frags[ZT_MAX_PACKET_FRAGMENTS - 1]; // later fragments (if any)
		unsigned int totalFragments; // 0 if only frag0 received, waiting for frags
		uint32_t haveFragments; // bit mask, LSB to MSB
		volatile bool complete; // if true, packet is complete
		Mutex lock;
	};
	RXQueueEntry _rxQueue[ZT_RX_QUEUE_SIZE];
	AtomicCounter _rxQueuePtr;

	// Returns matching or next available RX queue entry
	ZT_ALWAYS_INLINE RXQueueEntry *_findRXQueueEntry(uint64_t packetId)
	{
		const unsigned int current = static_cast<unsigned int>(_rxQueuePtr.load());
		for(unsigned int k=1;k<=ZT_RX_QUEUE_SIZE;++k) {
			RXQueueEntry *rq = &(_rxQueue[(current - k) % ZT_RX_QUEUE_SIZE]);
			if ((rq->packetId == packetId)&&(rq->timestamp))
				return rq;
		}
		++_rxQueuePtr;
		return &(_rxQueue[static_cast<unsigned int>(current) % ZT_RX_QUEUE_SIZE]);
	}

	// Returns current entry in rx queue ring buffer and increments ring pointer
	ZT_ALWAYS_INLINE RXQueueEntry *_nextRXQueueEntry()
	{
		return &(_rxQueue[static_cast<unsigned int>((++_rxQueuePtr) - 1) % ZT_RX_QUEUE_SIZE]);
	}

	// ZeroTier-layer TX queue entry
	struct TXQueueEntry
	{
		ZT_ALWAYS_INLINE TXQueueEntry() {}
		ZT_ALWAYS_INLINE TXQueueEntry(Address d,uint64_t ct,const Packet &p,bool enc) :
			dest(d),
			creationTime(ct),
			packet(p),
			encrypt(enc) {}

		Address dest;
		uint64_t creationTime;
		Packet packet; // unencrypted/unMAC'd packet -- this is done at send time
		bool encrypt;
	};
	std::list< TXQueueEntry > _txQueue;
	Mutex _txQueue_m;
};

} // namespace ZeroTier

#endif
