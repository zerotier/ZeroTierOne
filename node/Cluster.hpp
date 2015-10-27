/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2015  ZeroTier, Inc.
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

#ifndef ZT_CLUSTER_HPP
#define ZT_CLUSTER_HPP

#ifdef ZT_ENABLE_CLUSTER

#include <vector>
#include <algorithm>

#include "Constants.hpp"
#include "../include/ZeroTierOne.h"
#include "Address.hpp"
#include "InetAddress.hpp"
#include "SHA512.hpp"
#include "Utils.hpp"
#include "Buffer.hpp"
#include "Mutex.hpp"
#include "SharedPtr.hpp"
#include "Hashtable.hpp"

/**
 * Timeout for cluster members being considered "alive"
 */
#define ZT_CLUSTER_TIMEOUT 10000

/**
 * How often should we announce that we have a peer?
 */
#define ZT_CLUSTER_HAVE_PEER_ANNOUNCE_PERIOD 60000

/**
 * Desired period between doPeriodicTasks() in milliseconds
 */
#define ZT_CLUSTER_PERIODIC_TASK_PERIOD 250

namespace ZeroTier {

class RuntimeEnvironment;
class CertificateOfMembership;
class MulticastGroup;
class Peer;
class Identity;

/**
 * Multi-homing cluster state replication and packet relaying
 *
 * Multi-homing means more than one node sharing the same ZeroTier identity.
 * There is nothing in the protocol to prevent this, but to make it work well
 * requires the devices sharing an identity to cooperate and share some
 * information.
 *
 * There are three use cases we want to fulfill:
 *
 * (1) Multi-homing of root servers with handoff for efficient routing,
 *     HA, and load balancing across many commodity nodes.
 * (2) Multi-homing of network controllers for the same reason.
 * (3) Multi-homing of nodes on virtual networks, such as domain servers
 *     and other important endpoints.
 *
 * These use cases are in order of escalating difficulty. The initial
 * version of Cluster is aimed at satisfying the first, though you are
 * free to try #2 and #3.
 */
class Cluster
{
public:
	/**
	 * State message types
	 */
	enum StateMessageType
	{
		STATE_MESSAGE_NOP = 0,

		/**
		 * This cluster member is alive:
		 *   <[2] version minor>
		 *   <[2] version major>
		 *   <[2] version revision>
		 *   <[1] protocol version>
		 *   <[4] X location (signed 32-bit)>
		 *   <[4] Y location (signed 32-bit)>
		 *   <[4] Z location (signed 32-bit)>
		 *   <[8] local clock at this member>
		 *   <[8] load average>
		 *   <[8] flags (currently unused, must be zero)>
		 *   <[1] number of preferred ZeroTier endpoints>
		 *   <[...] InetAddress(es) of preferred ZeroTier endpoint(s)>
		 */
		STATE_MESSAGE_ALIVE = 1,

		/**
		 * Cluster member has this peer:
		 *   <[...] binary serialized peer identity>
		 */
		STATE_MESSAGE_HAVE_PEER = 2,

		/**
		 * Peer subscription to multicast group:
		 *   <[8] network ID>
		 *   <[5] peer ZeroTier address>
		 *   <[6] MAC address of multicast group>
		 *   <[4] 32-bit multicast group ADI>
		 */
		STATE_MESSAGE_MULTICAST_LIKE = 3,

		/**
		 * Certificate of network membership for a peer:
		 *   <[...] serialized COM>
		 */
		STATE_MESSAGE_COM = 4,

		/**
		 * Request that VERB_RENDEZVOUS be sent to a peer that we have:
		 *   <[5] ZeroTier address of peer on recipient's side>
		 *   <[5] ZeroTier address of peer on sender's side>
		 *   <[1] 8-bit number of sender's peer's active path addresses>
		 *   <[...] series of serialized InetAddresses of sender's peer's paths>
		 *
		 * This requests that we perform NAT-t introduction between a peer that
		 * we have and one on the sender's side. The sender furnishes contact
		 * info for its peer, and we send VERB_RENDEZVOUS to both sides: to ours
		 * directly and with PROXY_SEND to theirs.
		 */
		STATE_MESSAGE_PROXY_UNITE = 5,

		/**
		 * Request that a cluster member send a packet to a locally-known peer:
		 *   <[5] ZeroTier address of recipient>
		 *   <[1] packet verb>
		 *   <[2] length of packet payload>
		 *   <[...] packet payload>
		 *
		 * This differs from RELAY in that it requests the receiving cluster
		 * member to actually compose a ZeroTier Packet from itself to the
		 * provided recipient. RELAY simply says "please forward this blob."
		 * RELAY is used to implement peer-to-peer relaying with RENDEZVOUS,
		 * while PROXY_SEND is used to implement proxy sending (which right
		 * now is only used to send RENDEZVOUS).
		 */
		STATE_MESSAGE_PROXY_SEND = 6,

		/**
		 * Replicate a network config for a network we belong to:
		 *   <[8] 64-bit network ID>
		 *   <[2] 16-bit length of network config>
		 *   <[...] serialized network config>
		 *
		 * This is used by clusters to avoid every member having to query
		 * for the same netconf for networks all members belong to.
		 *
		 * TODO: not implemented yet!
		 */
		STATE_MESSAGE_NETWORK_CONFIG = 7
	};

	/**
	 * Construct a new cluster
	 */
	Cluster(
		const RuntimeEnvironment *renv,
		uint16_t id,
		const std::vector<InetAddress> &zeroTierPhysicalEndpoints,
		int32_t x,
		int32_t y,
		int32_t z,
		void (*sendFunction)(void *,unsigned int,const void *,unsigned int),
		void *sendFunctionArg,
		int (*addressToLocationFunction)(void *,const struct sockaddr_storage *,int *,int *,int *),
		void *addressToLocationFunctionArg);

	~Cluster();

	/**
	 * @return This cluster member's ID
	 */
	inline uint16_t id() const throw() { return _id; }

	/**
	 * Handle an incoming intra-cluster message
	 *
	 * @param data Message data
	 * @param len Message length (max: ZT_CLUSTER_MAX_MESSAGE_LENGTH)
	 */
	void handleIncomingStateMessage(const void *msg,unsigned int len);

	/**
	 * Send this packet via another node in this cluster if another node has this peer
	 *
	 * @param fromPeerAddress Source peer address (if known, should be NULL for fragments)
	 * @param toPeerAddress Destination peer address
	 * @param data Packet or packet fragment data
	 * @param len Length of packet or fragment
	 * @param unite If true, also request proxy unite across cluster
	 * @return True if this data was sent via another cluster member, false if none have this peer
	 */
	bool sendViaCluster(const Address &fromPeerAddress,const Address &toPeerAddress,const void *data,unsigned int len,bool unite);

	/**
	 * Advertise to the cluster that we have this peer
	 *
	 * @param peerId Identity of peer that we have
	 */
	void replicateHavePeer(const Identity &peerId);

	/**
	 * Advertise a multicast LIKE to the cluster
	 *
	 * @param nwid Network ID
	 * @param peerAddress Peer address that sent LIKE
	 * @param group Multicast group
	 */
	void replicateMulticastLike(uint64_t nwid,const Address &peerAddress,const MulticastGroup &group);

	/**
	 * Advertise a network COM to the cluster
	 *
	 * @param com Certificate of network membership (contains peer and network ID)
	 */
	void replicateCertificateOfNetworkMembership(const CertificateOfMembership &com);

	/**
	 * Call every ~ZT_CLUSTER_PERIODIC_TASK_PERIOD milliseconds.
	 */
	void doPeriodicTasks();

	/**
	 * Add a member ID to this cluster
	 *
	 * @param memberId Member ID
	 */
	void addMember(uint16_t memberId);

	/**
	 * Remove a member ID from this cluster
	 *
	 * @param memberId Member ID to remove
	 */
	void removeMember(uint16_t memberId);

	/**
	 * Find a better cluster endpoint for this peer (if any)
	 *
	 * @param redirectTo InetAddress to be set to a better endpoint (if there is one)
	 * @param peerAddress Address of peer to (possibly) redirect
	 * @param peerPhysicalAddress Physical address of peer's current best path (where packet was most recently received or getBestPath()->address())
	 * @param offload Always redirect if possible -- can be used to offload peers during shutdown
	 * @return True if redirectTo was set to a new address, false if redirectTo was not modified
	 */
	bool findBetterEndpoint(InetAddress &redirectTo,const Address &peerAddress,const InetAddress &peerPhysicalAddress,bool offload);

	/**
	 * Fill out ZT_ClusterStatus structure (from core API)
	 *
	 * @param status Reference to structure to hold result (anything there is replaced)
	 */
	void status(ZT_ClusterStatus &status) const;

private:
	void _send(uint16_t memberId,StateMessageType type,const void *msg,unsigned int len);
	void _flush(uint16_t memberId);

	// These are initialized in the constructor and remain static
	uint16_t _masterSecret[ZT_SHA512_DIGEST_LEN / sizeof(uint16_t)];
	unsigned char _key[ZT_PEER_SECRET_KEY_LENGTH];
	const RuntimeEnvironment *RR;
	void (*_sendFunction)(void *,unsigned int,const void *,unsigned int);
	void *_sendFunctionArg;
	int (*_addressToLocationFunction)(void *,const struct sockaddr_storage *,int *,int *,int *);
	void *_addressToLocationFunctionArg;
	const int32_t _x;
	const int32_t _y;
	const int32_t _z;
	const uint16_t _id;
	const std::vector<InetAddress> _zeroTierPhysicalEndpoints;

	struct _Member
	{
		unsigned char key[ZT_PEER_SECRET_KEY_LENGTH];

		uint64_t lastReceivedAliveAnnouncement;
		uint64_t lastAnnouncedAliveTo;

		uint64_t load;
		int32_t x,y,z;

		std::vector<InetAddress> zeroTierPhysicalEndpoints;

		Buffer<ZT_CLUSTER_MAX_MESSAGE_LENGTH> q;

		Mutex lock;

		inline void clear()
		{
			lastReceivedAliveAnnouncement = 0;
			lastAnnouncedAliveTo = 0;
			load = 0;
			x = 0;
			y = 0;
			z = 0;
			zeroTierPhysicalEndpoints.clear();
			q.clear();
		}

		_Member() { this->clear(); }
		~_Member() { Utils::burn(key,sizeof(key)); }
	};

	_Member *const _members; // cluster IDs can be from 0 to 65535 (16-bit)

	std::vector<uint16_t> _memberIds;
	Mutex _memberIds_m;

	// Record tracking which members have which peers and how recently they claimed this -- also used to track our last claimed time
	struct _PeerAffinity
	{
		_PeerAffinity(const Address &a,uint16_t mid,uint64_t ts) :
			key((a.toInt() << 16) | (uint64_t)mid),
			timestamp(ts) {}

		uint64_t key;
		uint64_t timestamp;

		inline Address address() const throw() { return Address(key >> 16); }
		inline uint16_t clusterMemberId() const throw() { return (uint16_t)(key & 0xffff); }

		inline bool operator<(const _PeerAffinity &pi) const throw() { return (key < pi.key); }
	};

	// A memory-efficient packed map of _PeerAffinity records searchable with std::binary_search() and std::lower_bound()
	std::vector<_PeerAffinity> _peerAffinities;
	Mutex _peerAffinities_m;
};

} // namespace ZeroTier

#endif // ZT_ENABLE_CLUSTER

#endif
