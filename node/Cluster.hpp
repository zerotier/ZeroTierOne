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
#include "Address.hpp"
#include "InetAddress.hpp"
#include "SHA512.hpp"
#include "Utils.hpp"
#include "Buffer.hpp"
#include "Mutex.hpp"

/**
 * Timeout for cluster members being considered "alive"
 */
#define ZT_CLUSTER_TIMEOUT ZT_PEER_ACTIVITY_TIMEOUT

/**
 * Maximum cluster message length in bytes
 *
 * Cluster nodes speak via TCP, with data encapsulated into individually
 * encrypted and authenticated messages. The maximum message size is
 * 65535 (0xffff) since the TCP stream uses 16-bit message size headers
 * (and this is a reasonable chunk size anyway).
 */
#define ZT_CLUSTER_MAX_MESSAGE_LENGTH 65535

/**
 * Maximum number of physical addresses we will cache for a cluster member
 */
#define ZT_CLUSTER_MEMBER_MAX_PHYSICAL_ADDRS 8

/**
 * How frequently should doPeriodicTasks() be ideally called? (ms)
 */
#define ZT_CLUSTER_PERIODIC_TASK_DEADLINE 10

namespace ZeroTier {

class RuntimeEnvironment;
class CertificateOfMembership;
class MulticastGroup;

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
	 * Which distance algorithm is this cluster using?
	 */
	enum DistanceAlgorithm
	{
		/**
		 * Simple linear distance in three dimensions
		 */
		DISTANCE_SIMPLE = 0,

		/**
		 * Haversine formula using X,Y as lat,long and ignoring Z
		 */
		DISTANCE_HAVERSINE = 1
	};

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
		 * Relay a packet to a peer:
		 *   <[1] 8-bit number of sending peer active path addresses>
		 *   <[...] series of serialized InetAddresses of sending peer's paths>
		 *   <[2] 16-bit packet length>
		 *   <[...] packet or packet fragment>
		 */
		STATE_MESSAGE_RELAY = 5,

		/**
		 * Request to send a packet to a locally-known peer:
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
		STATE_MESSAGE_PROXY_SEND = 6
	};

	/**
	 * Construct a new cluster
	 *
	 * @param renv Runtime environment
	 * @param id This member's ID in the cluster
	 * @param da Distance algorithm this cluster uses to compute distance and hand off peers
	 * @param x My X
	 * @param y My Y
	 * @param z My Z
	 * @param sendFunction Function to call to send messages to other cluster members
	 * @param arg First argument to sendFunction
	 */
	Cluster(
		const RuntimeEnvironment *renv,
		uint16_t id,
		DistanceAlgorithm da,
		int32_t x,
		int32_t y,
		int32_t z,
		void (*sendFunction)(void *,uint16_t,const void *,unsigned int),
		void *arg);

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
	 * Advertise to the cluster that we have this peer
	 *
	 * @param peerAddress Peer address that we have
	 */
	void replicateHavePeer(const Address &peerAddress);

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
	 * Call every ~ZT_CLUSTER_PERIODIC_TASK_DEADLINE milliseconds.
	 */
	void doPeriodicTasks();

	/**
	 * Add a member ID to this cluster
	 *
	 * @param memberId Member ID
	 */
	void addMember(uint16_t memberId);

private:
	void _send(uint16_t memberId,const void *msg,unsigned int len);
	void _flush(uint16_t memberId);

	// These are initialized in the constructor and remain static
	uint16_t _masterSecret[ZT_SHA512_DIGEST_LEN / sizeof(uint16_t)];
	unsigned char _key[ZT_PEER_SECRET_KEY_LENGTH];
	const RuntimeEnvironment *RR;
	void (*_sendFunction)(void *,uint16_t,const void *,unsigned int);
	void *_arg;
	const int32_t _x;
	const int32_t _y;
	const int32_t _z;
	const DistanceAlgorithm _da;
	const uint16_t _id;

	struct _Member
	{
		unsigned char key[ZT_PEER_SECRET_KEY_LENGTH];

		uint64_t lastReceivedFrom;
		uint64_t lastReceivedAliveAnnouncement;
		uint64_t lastSentTo;
		uint64_t lastAnnouncedAliveTo;

		uint64_t load;
		int32_t x,y,z;

		InetAddress physicalAddresses[ZT_CLUSTER_MEMBER_MAX_PHYSICAL_ADDRS];
		unsigned int physicalAddressCount;

		Buffer<ZT_CLUSTER_MAX_MESSAGE_LENGTH> q;

		Mutex lock;

		_Member() :
			lastReceivedFrom(0),
			lastReceivedAliveAnnouncement(0),
			lastSentTo(0),
			lastAnnouncedAliveTo(0),
			load(0),
			x(0),
			y(0),
			z(0),
			physicalAddressCount(0) {}

		~_Member() { Utils::burn(key,sizeof(key)); }
	};

	_Member *const _members; // cluster IDs can be from 0 to 65535 (16-bit)

	std::vector<uint16_t> _memberIds;
	Mutex _memberIds_m;

	// Record tracking which members have which peers and how recently they claimed this
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
