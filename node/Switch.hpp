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
#include "Demarc.hpp"
#include "Multicaster.hpp"
#include "PacketDecoder.hpp"

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
	 * @param remoteAddr IP address to send to
	 * @param dest Destination peer
	 * @return True if send appears successful
	 */
	bool sendHELLO(const SharedPtr<Peer> &dest,Demarc::Port localPort,const InetAddress &remoteAddr);

	/**
	 * Send a PROBE immediately to the indicated address
	 *
	 * @param localPort Originating local port or ANY_PORT to pick
	 * @param remoteAddr IP address to send to
	 * @param dest Destination peer
	 * @return True if send appears successful
	 */
	bool sendPROBE(const SharedPtr<Peer> &dest,Demarc::Port localPort,const InetAddress &remoteAddr);

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
	 * Send NAT traversal messages to peer at the given candidate address
	 *
	 * @param peer Peer to contact
	 * @param atAddr Address of peer
	 */
	void contact(const SharedPtr<Peer> &peer,const InetAddress &atAddr);

	/**
	 * Perform retries and other periodic timer tasks
	 * 
	 * @return Number of milliseconds until doTimerTasks() should be run again
	 */
	unsigned long doTimerTasks();

	/**
	 * Announce multicast group memberships
	 *
	 * This announces all the groups for all the networks in the supplied map to
	 * all peers with whom we have an active direct link. Only isAllowed() peers
	 * and supernodes get announcements for each given network.
	 *
	 * @param allMemberships Memberships for a number of networks
	 */
	void announceMulticastGroups(const std::map< SharedPtr<Network>,std::set<MulticastGroup> > &allMemberships);

	/**
	 * Announce multicast group memberships
	 *
	 * This announces all current multicast memberships to a single peer. Only
	 * memberships for networks where the peer isAllowed() are included, unless
	 * the peer is a supernode.
	 *
	 * @param peer Peer to announce all memberships to
	 */
	void announceMulticastGroups(const SharedPtr<Peer> &peer);

	/**
	 * Request WHOIS on a given address
	 *
	 * @param addr Address to look up
	 */
	void requestWhois(const Address &addr);

	/**
	 * Cancel WHOIS for an address
	 *
	 * @param addr Address to cancel
	 */
	void cancelWhoisRequest(const Address &addr);

	/**
	 * Run any processes that are waiting for this peer
	 *
	 * Called when we learn of a peer's identity from HELLO, OK(WHOIS), etc.
	 *
	 * @param peer New peer
	 */
	void doAnythingWaitingForPeer(const SharedPtr<Peer> &peer);

	/**
	 * @param etherType Ethernet type ID
	 * @return Human-readable name
	 */
	static const char *etherTypeName(const unsigned int etherType)
		throw();

private:
	void _handleRemotePacketFragment(
		Demarc::Port localPort,
		const InetAddress &fromAddr,
		const Buffer<4096> &data);

	void _handleRemotePacketHead(
		Demarc::Port localPort,
		const InetAddress &fromAddr,
		const Buffer<4096> &data);

	Address _sendWhoisRequest(
		const Address &addr,
		const Address *peersAlreadyConsulted,
		unsigned int numPeersAlreadyConsulted);

	bool _trySend(
		const Packet &packet,
		bool encrypt);

	const RuntimeEnvironment *const _r;
	volatile unsigned int _multicastIdCounter;

	struct WhoisRequest
	{
		uint64_t lastSent;
		Address peersConsulted[ZT_MAX_WHOIS_RETRIES]; // by retry
		unsigned int retries; // 0..ZT_MAX_WHOIS_RETRIES
	};
	std::map< Address,WhoisRequest > _outstandingWhoisRequests;
	Mutex _outstandingWhoisRequests_m;

	std::list< SharedPtr<PacketDecoder> > _rxQueue;
	Mutex _rxQueue_m;

	struct TXQueueEntry
	{
		TXQueueEntry() {}
		TXQueueEntry(uint64_t ct,const Packet &p,bool enc) :
			creationTime(ct),
			packet(p),
			encrypt(enc) {}

		uint64_t creationTime;
		Packet packet; // unencrypted/untagged for TX queue
		bool encrypt;
	};
	std::multimap< Address,TXQueueEntry > _txQueue;
	Mutex _txQueue_m;

	struct DefragQueueEntry
	{
		uint64_t creationTime;
		SharedPtr<PacketDecoder> frag0;
		Packet::Fragment frags[ZT_MAX_PACKET_FRAGMENTS - 1];
		unsigned int totalFragments; // 0 if only frag0 received, waiting for frags
		uint32_t haveFragments; // bit mask, LSB to MSB
	};
	std::map< uint64_t,DefragQueueEntry > _defragQueue;
	Mutex _defragQueue_m;

	std::map< Array< Address,2 >,uint64_t > _lastUniteAttempt; // key is always sorted in ascending order, for set-like behavior
	Mutex _lastUniteAttempt_m;

	struct ContactQueueEntry
	{
		ContactQueueEntry() {}
		ContactQueueEntry(const SharedPtr<Peer> &p,uint64_t ft,Demarc::Port lp,const InetAddress &a) :
			peer(p),
			fireAtTime(ft),
			localPort(lp),
			inaddr(a) {}

		SharedPtr<Peer> peer;
		uint64_t fireAtTime;
		Demarc::Port localPort;
		InetAddress inaddr;
	};
	std::list<ContactQueueEntry> _contactQueue;
	Mutex _contactQueue_m;
};

} // namespace ZeroTier

#endif
