/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2017  ZeroTier, Inc.  https://www.zerotier.com/
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
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

#ifndef ZT_N_SWITCH_HPP
#define ZT_N_SWITCH_HPP

#include <map>
#include <set>
#include <vector>
#include <list>

#include "Constants.hpp"
#include "Mutex.hpp"
#include "MAC.hpp"
#include "NonCopyable.hpp"
#include "Packet.hpp"
#include "Utils.hpp"
#include "InetAddress.hpp"
#include "Topology.hpp"
#include "Array.hpp"
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
class Switch : NonCopyable
{
public:
	Switch(const RuntimeEnvironment *renv);

	/**
	 * Called when a packet is received from the real network
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param localAddr Local interface address
	 * @param fromAddr Internet IP address of origin
	 * @param data Packet data
	 * @param len Packet length
	 */
	void onRemotePacket(void *tPtr,const InetAddress &localAddr,const InetAddress &fromAddr,const void *data,unsigned int len);

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
	 * @param addr Address to look up
	 */
	void requestWhois(void *tPtr,const Address &addr);

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
	unsigned long doTimerTasks(void *tPtr,uint64_t now);

private:
	bool _shouldUnite(const uint64_t now,const Address &source,const Address &destination);
	Address _sendWhoisRequest(void *tPtr,const Address &addr,const Address *peersAlreadyConsulted,unsigned int numPeersAlreadyConsulted);
	bool _trySend(void *tPtr,Packet &packet,bool encrypt); // packet is modified if return is true

	const RuntimeEnvironment *const RR;
	uint64_t _lastBeaconResponse;

	// Outstanding WHOIS requests and how many retries they've undergone
	struct WhoisRequest
	{
		WhoisRequest() : lastSent(0),retries(0) {}
		uint64_t lastSent;
		Address peersConsulted[ZT_MAX_WHOIS_RETRIES]; // by retry
		unsigned int retries; // 0..ZT_MAX_WHOIS_RETRIES
	};
	Hashtable< Address,WhoisRequest > _outstandingWhoisRequests;
	Mutex _outstandingWhoisRequests_m;

	// Packets waiting for WHOIS replies or other decode info or missing fragments
	struct RXQueueEntry
	{
		RXQueueEntry() : timestamp(0) {}
		uint64_t timestamp; // 0 if entry is not in use
		uint64_t packetId;
		IncomingPacket frag0; // head of packet
		Packet::Fragment frags[ZT_MAX_PACKET_FRAGMENTS - 1]; // later fragments (if any)
		unsigned int totalFragments; // 0 if only frag0 received, waiting for frags
		uint32_t haveFragments; // bit mask, LSB to MSB
		bool complete; // if true, packet is complete
	};
	RXQueueEntry _rxQueue[ZT_RX_QUEUE_SIZE];
	Mutex _rxQueue_m;

	/* Returns the matching or oldest entry. Caller must check timestamp and
	 * packet ID to determine which. */
	inline RXQueueEntry *_findRXQueueEntry(uint64_t now,uint64_t packetId)
	{
		RXQueueEntry *rq;
		RXQueueEntry *oldest = &(_rxQueue[ZT_RX_QUEUE_SIZE - 1]);
		unsigned long i = ZT_RX_QUEUE_SIZE;
		while (i) {
			rq = &(_rxQueue[--i]);
			if ((rq->packetId == packetId)&&(rq->timestamp))
				return rq;
			if ((now - rq->timestamp) >= ZT_RX_QUEUE_EXPIRE)
				rq->timestamp = 0;
			if (rq->timestamp < oldest->timestamp)
				oldest = rq;
		}
		return oldest;
	}

	// ZeroTier-layer TX queue entry
	struct TXQueueEntry
	{
		TXQueueEntry() {}
		TXQueueEntry(Address d,uint64_t ct,const Packet &p,bool enc) :
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
		inline unsigned long hashCode() const throw() { return ((unsigned long)x ^ (unsigned long)y); }
		inline bool operator==(const _LastUniteKey &k) const throw() { return ((x == k.x)&&(y == k.y)); }
		uint64_t x,y;
	};
	Hashtable< _LastUniteKey,uint64_t > _lastUniteAttempt; // key is always sorted in ascending order, for set-like behavior
	Mutex _lastUniteAttempt_m;
};

} // namespace ZeroTier

#endif
