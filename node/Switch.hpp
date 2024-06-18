/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
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

/* Ethernet frame types that might be relevant to us */
#define ZT_ETHERTYPE_IPV4 0x0800
#define ZT_ETHERTYPE_ARP 0x0806
#define ZT_ETHERTYPE_RARP 0x8035
#define ZT_ETHERTYPE_ATALK 0x809b
#define ZT_ETHERTYPE_AARP 0x80f3
#define ZT_ETHERTYPE_IPX_A 0x8137
#define ZT_ETHERTYPE_IPX_B 0x8138
#define ZT_ETHERTYPE_IPV6 0x86dd

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
	struct ManagedQueue;
	struct TXQueueEntry;

	friend class SharedPtr<Peer>;

	typedef struct {
		TXQueueEntry *p;
		bool ok_to_drop;
	} dqr;

public:
	Switch(const RuntimeEnvironment *renv);

	/**
	 * Called when a packet is received from the real network
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param localSocket Local I/O socket as supplied by external code
	 * @param fromAddr Internet IP address of origin
	 * @param data Packet data
	 * @param len Packet length
	 */
	void onRemotePacket(void *tPtr,const int64_t localSocket,const InetAddress &fromAddr,const void *data,unsigned int len);

	/**
	 * Returns whether our bonding or balancing policy is aware of flows.
	 */
	bool isFlowAware();

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
	 * Determines the next drop schedule for packets in the TX queue
	 *
	 * @param t Current time
	 * @param count Number of packets dropped this round
	 */
	uint64_t control_law(uint64_t t, int count);

	/**
	 * Selects a packet eligible for transmission from a TX queue. According to the control law, multiple packets
	 * may be intentionally dropped before a packet is returned to the AQM scheduler.
	 *
	 * @param q The TX queue that is being dequeued from
	 * @param now Current time
	 */
	dqr dodequeue(ManagedQueue *q, uint64_t now);

	/**
	 * Presents a packet to the AQM scheduler.
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param network Network that the packet shall be sent over
	 * @param packet Packet to be sent
	 * @param encrypt Encrypt packet payload? (always true except for HELLO)
	 * @param qosBucket Which bucket the rule-system determined this packet should fall into
	 */
	void aqm_enqueue(void *tPtr, const SharedPtr<Network> &network, Packet &packet,bool encrypt,int qosBucket,int32_t flowId = ZT_QOS_NO_FLOW);

	/**
	 * Performs a single AQM cycle and dequeues and transmits all eligible packets on all networks
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 */
	void aqm_dequeue(void *tPtr);

	/**
	 * Calls the dequeue mechanism and adjust queue state variables
	 *
	 * @param q The TX queue that is being dequeued from
	 * @param isNew Whether or not this queue is in the NEW list
	 * @param now Current time
	 */
	Switch::TXQueueEntry * CoDelDequeue(ManagedQueue *q, bool isNew, uint64_t now);

	/**
	 * Removes QoS Queues and flow state variables for a specific network. These queues are created
	 * automatically upon the transmission of the first packet from this peer to another peer on the
	 * given network.
	 *
	 * The reason for existence of queues and flow state variables specific to each network is so that
	 * each network's QoS rules function independently.
	 *
	 * @param nwid Network ID
	 */
	void removeNetworkQoSControlBlock(uint64_t nwid);

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
	void send(void *tPtr,Packet &packet,bool encrypt,int32_t flowId = ZT_QOS_NO_FLOW);

	/**
	 * Request WHOIS on a given address
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param now Current time
	 * @param addr Address to look up
	 */
	void requestWhois(void *tPtr,const int64_t now,const Address &addr);

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
	bool _shouldUnite(const int64_t now,const Address &source,const Address &destination);
	bool _trySend(void *tPtr,Packet &packet,bool encrypt,int32_t flowId = ZT_QOS_NO_FLOW); // packet is modified if return is true
	void _sendViaSpecificPath(void *tPtr,SharedPtr<Peer> peer,SharedPtr<Path> viaPath,uint16_t userSpecifiedMtu, int64_t now,Packet &packet,bool encrypt,int32_t flowId);
	void _recordOutgoingPacketMetrics(const Packet &p);

	const RuntimeEnvironment *const RR;
	int64_t _lastBeaconResponse;
	volatile int64_t _lastCheckedQueues;

	// Time we last sent a WHOIS request for each address
	Hashtable< Address,int64_t > _lastSentWhoisRequest;
	Mutex _lastSentWhoisRequest_m;

	// Packets waiting for WHOIS replies or other decode info or missing fragments
	struct RXQueueEntry
	{
		RXQueueEntry() : timestamp(0) {}
		volatile int64_t timestamp; // 0 if entry is not in use
		volatile uint64_t packetId;
		IncomingPacket frag0; // head of packet
		Packet::Fragment frags[ZT_MAX_PACKET_FRAGMENTS - 1]; // later fragments (if any)
		unsigned int totalFragments; // 0 if only frag0 received, waiting for frags
		uint32_t haveFragments; // bit mask, LSB to MSB
		volatile bool complete; // if true, packet is complete
		volatile int32_t flowId;
		Mutex lock;
	};
	RXQueueEntry _rxQueue[ZT_RX_QUEUE_SIZE];
	AtomicCounter _rxQueuePtr;

	// Returns matching or next available RX queue entry
	inline RXQueueEntry *_findRXQueueEntry(uint64_t packetId)
	{
		const unsigned int current = static_cast<unsigned int>(_rxQueuePtr.load());
		for(unsigned int k=1;k<=ZT_RX_QUEUE_SIZE;++k) {
			RXQueueEntry *rq = &(_rxQueue[(current - k) % ZT_RX_QUEUE_SIZE]);
			if ((rq->packetId == packetId)&&(rq->timestamp)) {
				return rq;
			}
		}
		++_rxQueuePtr;
		return &(_rxQueue[static_cast<unsigned int>(current) % ZT_RX_QUEUE_SIZE]);
	}

	// Returns current entry in rx queue ring buffer and increments ring pointer
	inline RXQueueEntry *_nextRXQueueEntry()
	{
		return &(_rxQueue[static_cast<unsigned int>((++_rxQueuePtr) - 1) % ZT_RX_QUEUE_SIZE]);
	}

	// ZeroTier-layer TX queue entry
	struct TXQueueEntry
	{
		TXQueueEntry() {}
		TXQueueEntry(Address d,uint64_t ct,const Packet &p,bool enc,int32_t fid) :
			dest(d),
			creationTime(ct),
			packet(p),
			encrypt(enc),
			flowId(fid) {}

		Address dest;
		uint64_t creationTime;
		Packet packet; // unencrypted/unMAC'd packet -- this is done at send time
		bool encrypt;
		int32_t flowId;
	};
	std::list< TXQueueEntry > _txQueue;
	Mutex _txQueue_m;
	Mutex _aqm_m;

	// Tracks sending of VERB_RENDEZVOUS to relaying peers
	struct _LastUniteKey
	{
		_LastUniteKey() : x(0),y(0) {}
		_LastUniteKey(const Address &a1,const Address &a2)
		{
			if (a1 > a2) {
				x = a2.toInt();
				y = a1.toInt();
			} else {
				x = a1.toInt();
				y = a2.toInt();
			}
		}
		inline unsigned long hashCode() const { return ((unsigned long)x ^ (unsigned long)y); }
		inline bool operator==(const _LastUniteKey &k) const { return ((x == k.x)&&(y == k.y)); }
		uint64_t x,y;
	};
	Hashtable< _LastUniteKey,uint64_t > _lastUniteAttempt; // key is always sorted in ascending order, for set-like behavior
	Mutex _lastUniteAttempt_m;

	// Queue with additional flow state variables
	struct ManagedQueue
	{
		ManagedQueue(int id) :
			id(id),
			byteCredit(ZT_AQM_QUANTUM),
			byteLength(0),
			dropping(false)
		{}
		int id;
		int byteCredit;
		int byteLength;
		uint64_t first_above_time;
		uint32_t count;
		uint64_t drop_next;
		bool dropping;
		uint64_t drop_next_time;
		std::list< TXQueueEntry *> q;
	};
	// To implement fq_codel we need to maintain a queue of queues
	struct NetworkQoSControlBlock
	{
		int _currEnqueuedPackets;
		std::vector<ManagedQueue *> newQueues;
		std::vector<ManagedQueue *> oldQueues;
		std::vector<ManagedQueue *> inactiveQueues;
	};
	std::map<uint64_t,NetworkQoSControlBlock*> _netQueueControlBlock;
};

} // namespace ZeroTier

#endif
