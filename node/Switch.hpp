/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2012-2013  ZeroTier Networks LLC
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

#ifndef _ZT_N_SWITCH_HPP
#define _ZT_N_SWITCH_HPP

#include <map>
#include <set>
#include <vector>

#include "Mutex.hpp"
#include "MAC.hpp"
#include "NonCopyable.hpp"
#include "Constants.hpp"
#include "Packet.hpp"
#include "Utils.hpp"
#include "InetAddress.hpp"
#include "Topology.hpp"
#include "Array.hpp"
#include "Network.hpp"
#include "SharedPtr.hpp"
#include "Demarc.hpp"

namespace ZeroTier {

class RuntimeEnvironment;
class EthernetTap;
class Logger;
class Node;
class Peer;

/**
 * Core of the distributed Ethernet switch and protocol implementation
 */
class Switch : NonCopyable
{
public:
	Switch(const RuntimeEnvironment *renv);
	~Switch();

	/**
	 * Called when a packet is received from the real network
	 *
	 * @param localPort Local port on which packet was received
	 * @param fromAddr Internet IP address of origin
	 * @param data Packet data
	 */
	void onRemotePacket(Demarc::Port localPort,const InetAddress &fromAddr,const Buffer<4096> &data);

	/**
	 * Called when a packet comes from a local Ethernet tap
	 *
	 * @param network Which network's TAP did this packet come from?
	 * @param from Originating MAC address
	 * @param to Destination MAC address
	 * @param etherType Ethernet packet type
	 * @param data Ethernet payload
	 */
	void onLocalEthernet(const SharedPtr<Network> &network,const MAC &from,const MAC &to,unsigned int etherType,const Buffer<4096> &data);

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
	 * @param packet Packet to send
	 * @param encrypt Encrypt packet payload? (always true except for HELLO)
	 */
	void send(const Packet &packet,bool encrypt);

	/**
	 * Send a HELLO announcement
	 *
	 * @param dest Address of destination
	 */
	void sendHELLO(const Address &dest);

	/**
	 * Send a HELLO announcement immediately to the indicated address
	 *
	 * @param localPort Originating local port or ANY_PORT to pick
	 * @param addr IP address to send to
	 * @param dest Destination peer
	 * @return True if send appears successful
	 */
	bool sendHELLO(const SharedPtr<Peer> &dest,Demarc::Port localPort,const InetAddress &addr);

	/**
	 * Send RENDEZVOUS to two peers to permit them to directly connect
	 *
	 * This only works if both peers are known, with known working direct
	 * links to this peer. The best link for each peer is sent to the other.
	 *
	 * A rate limiter is in effect via the _lastUniteAttempt map. If force
	 * is true, a unite attempt is made even if one has been made less than
	 * ZT_MIN_UNITE_INTERVAL milliseconds ago.
	 *
	 * @param p1 One of two peers (order doesn't matter)
	 * @param p2 Second of pair
	 * @param force If true, send now regardless of interval
	 */
	bool unite(const Address &p1,const Address &p2,bool force);

	/**
	 * Perform retries and other periodic timer tasks
	 * 
	 * @return Number of milliseconds until doTimerTasks() should be run again
	 */
	unsigned long doTimerTasks();

	/**
	 * Announce multicast group memberships
	 *
	 * This efficiently announces memberships, sending single packets with
	 * many LIKEs.
	 *
	 * @param allMemberships Memberships for a number of networks
	 */
	void announceMulticastGroups(const std::map< SharedPtr<Network>,std::set<MulticastGroup> > &allMemberships);

private:
	// Returned by _send() and _processRemotePacket() to indicate what happened
	enum PacketServiceAttemptResult
	{
		PACKET_SERVICE_ATTEMPT_OK,
		PACKET_SERVICE_ATTEMPT_PEER_UNKNOWN,
		PACKET_SERVICE_ATTEMPT_SEND_FAILED
	};

	struct _CBaddPeerFromHello_Data
	{
		Switch *parent;
		Address source;
		InetAddress fromAddr;
		int localPort;
		unsigned int vMajor,vMinor,vRevision;
		uint64_t helloPacketId;
		uint64_t helloTimestamp;
	};
	static void _CBaddPeerFromHello(void *arg,const SharedPtr<Peer> &p,Topology::PeerVerifyResult result);
	static void _CBaddPeerFromWhois(void *arg,const SharedPtr<Peer> &p,Topology::PeerVerifyResult result); // arg == this

	void _propagateMulticast(const SharedPtr<Network> &network,unsigned char *bloom,const MulticastGroup &mg,unsigned int mcHops,unsigned int mcLoadFactor,const MAC &from,unsigned int etherType,const void *data,unsigned int len);
	PacketServiceAttemptResult _tryHandleRemotePacket(Demarc::Port localPort,const InetAddress &fromAddr,Packet &packet);
	void _doHELLO(Demarc::Port localPort,const InetAddress &fromAddr,Packet &packet);
	void _requestWhois(const Address &addr);
	Address _sendWhoisRequest(const Address &addr,const Address *peersAlreadyConsulted,unsigned int numPeersAlreadyConsulted);
	PacketServiceAttemptResult _trySend(const Packet &packet,bool encrypt);
	void _retryPendingFor(const Address &addr);

	// Updates entry for crc in multicast history, returns true if already
	// present in history and not expired.
	inline bool _checkAndUpdateMulticastHistory(const MAC &fromMac,const MAC &toMulticastMac,const void *payload,unsigned int len,const uint64_t nwid,const uint64_t now)
	{
		uint64_t crc = Utils::crc64(0,fromMac.data,6);
		crc = Utils::crc64(crc,toMulticastMac.data,6);
		crc = Utils::crc64(crc,payload,len);
		crc += nwid; // also include network ID

		uint64_t earliest = 0xffffffffffffffffULL;
		unsigned long earliestIdx = 0;
		for(unsigned int i=0;i<ZT_MULTICAST_DEDUP_HISTORY_LENGTH;++i) {
			if (_multicastHistory[i][0] == crc) {
				uint64_t then = _multicastHistory[i][1];
				_multicastHistory[i][1] = now;
				return ((now - then) < ZT_MULTICAST_DEDUP_HISTORY_EXPIRE);
			} else if (_multicastHistory[i][1] < earliest) {
				earliest = _multicastHistory[i][1];
				earliestIdx = i;
			}
		}

		_multicastHistory[earliestIdx][0] = crc; // replace oldest entry
		_multicastHistory[earliestIdx][1] = now;

		return false;
	}

	const RuntimeEnvironment *const _r;

	// Multicast packet CRC64's for packets we've received recently, to reject
	// duplicates during propagation. [0] is CRC64, [1] is time.
	uint64_t _multicastHistory[ZT_MULTICAST_DEDUP_HISTORY_LENGTH][2];

	struct WhoisRequest
	{
		uint64_t lastSent;
		Address peersConsulted[ZT_MAX_WHOIS_RETRIES]; // by retry
		unsigned int retries; // 0..ZT_MAX_WHOIS_RETRIES
	};
	std::map< Address,WhoisRequest > _outstandingWhoisRequests;
	Mutex _outstandingWhoisRequests_m;

	struct TXQueueEntry
	{
		uint64_t creationTime;
		Packet packet; // unencrypted/untagged for TX queue
		bool encrypt;
	};
	std::multimap< Address,TXQueueEntry > _txQueue; // by destination address
	Mutex _txQueue_m;

	struct RXQueueEntry
	{
		uint64_t creationTime;
		Demarc::Port localPort;
		Packet packet; // encrypted/tagged
		InetAddress fromAddr;
	};
	std::multimap< Address,RXQueueEntry > _rxQueue; // by source address
	Mutex _rxQueue_m;

	struct DefragQueueEntry
	{
		uint64_t creationTime;
		Packet frag0;
		Packet::Fragment frags[ZT_MAX_PACKET_FRAGMENTS - 1];
		unsigned int totalFragments; // 0 if only frag0 received, waiting for frags
		uint32_t haveFragments; // bit mask, LSB to MSB
	};
	std::map< uint64_t,DefragQueueEntry > _defragQueue;
	Mutex _defragQueue_m;

	std::map< Array< Address,2 >,uint64_t > _lastUniteAttempt; // key is always sorted in ascending order, for set-like behavior
	Mutex _lastUniteAttempt_m;

	struct RendezvousQueueEntry
	{
		InetAddress inaddr;
		uint64_t fireAtTime;
		Demarc::Port localPort;
	};
	std::map< Address,RendezvousQueueEntry > _rendezvousQueue;
	Mutex _rendezvousQueue_m;
};

} // namespace ZeroTier

#endif
