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

#ifndef ZT_CLUSTER_HPP
#define ZT_CLUSTER_HPP

#ifdef ZT_ENABLE_CLUSTER

#include <map>

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
#include "Packet.hpp"
#include "SharedPtr.hpp"

/**
 * Timeout for cluster members being considered "alive"
 *
 * A cluster member is considered dead and will no longer have peers
 * redirected to it if we have not heard a heartbeat in this long.
 */
#define ZT_CLUSTER_TIMEOUT 5000

/**
 * Desired period between doPeriodicTasks() in milliseconds
 */
#define ZT_CLUSTER_PERIODIC_TASK_PERIOD 20

/**
 * How often to flush outgoing message queues (maximum interval)
 */
#define ZT_CLUSTER_FLUSH_PERIOD ZT_CLUSTER_PERIODIC_TASK_PERIOD

/**
 * Maximum number of queued outgoing packets per sender address
 */
#define ZT_CLUSTER_MAX_QUEUE_PER_SENDER 16

/**
 * Expiration time for send queue entries
 */
#define ZT_CLUSTER_QUEUE_EXPIRATION 3000

/**
 * Chunk size for allocating queue entries
 *
 * Queue entries are allocated in chunks of this many and are added to a pool.
 * ZT_CLUSTER_MAX_QUEUE_GLOBAL must be evenly divisible by this.
 */
#define ZT_CLUSTER_QUEUE_CHUNK_SIZE 32

/**
 * Maximum number of chunks to ever allocate
 *
 * This is a global sanity limit to prevent resource exhaustion attacks. It
 * works out to about 600mb of RAM. You'll never see this on a normal edge
 * node. We're unlikely to see this on a root server unless someone is DOSing
 * us. In that case cluster relaying will be affected but other functions
 * should continue to operate normally.
 */
#define ZT_CLUSTER_MAX_QUEUE_CHUNKS 8194

/**
 * Max data per queue entry
 */
#define ZT_CLUSTER_SEND_QUEUE_DATA_MAX 1500

/**
 * We won't send WANT_PEER to other members more than every (ms) per recipient
 */
#define ZT_CLUSTER_WANT_PEER_EVERY 1000

namespace ZeroTier {

class RuntimeEnvironment;
class MulticastGroup;
class Peer;
class Identity;

// Internal class implemented inside Cluster.cpp
class _ClusterSendQueue;

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
		CLUSTER_MESSAGE_NOP = 0,

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
		 *   <[8] number of peers>
		 *   <[8] flags (currently unused, must be zero)>
		 *   <[1] number of preferred ZeroTier endpoints>
		 *   <[...] InetAddress(es) of preferred ZeroTier endpoint(s)>
		 *
		 * Cluster members constantly broadcast an alive heartbeat and will only
		 * receive peer redirects if they've done so within the timeout.
		 */
		CLUSTER_MESSAGE_ALIVE = 1,

		/**
		 * Cluster member has this peer:
		 *   <[...] serialized identity of peer>
		 *
		 * This is typically sent in response to WANT_PEER but can also be pushed
		 * to prepopulate if this makes sense.
		 */
		CLUSTER_MESSAGE_HAVE_PEER = 2,

		/**
		 * Cluster member wants this peer:
		 *   <[5] ZeroTier address of peer>
		 *
		 * Members that have a direct link to this peer will respond with
		 * HAVE_PEER.
		 */
		CLUSTER_MESSAGE_WANT_PEER = 3,

		/**
		 * A remote packet that we should also possibly respond to:
		 *   <[2] 16-bit length of remote packet>
		 *   <[...] remote packet payload>
		 *
		 * Cluster members may relay requests by relaying the request packet.
		 * These may include requests such as WHOIS and MULTICAST_GATHER. The
		 * packet must be already decrypted, decompressed, and authenticated.
		 *
		 * This can only be used for small request packets as per the cluster
		 * message size limit, but since these are the only ones in question
		 * this is fine.
		 *
		 * If a response is generated it is sent via PROXY_SEND.
		 */
		CLUSTER_MESSAGE_REMOTE_PACKET = 4,

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
		CLUSTER_MESSAGE_PROXY_UNITE = 5,

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
		CLUSTER_MESSAGE_PROXY_SEND = 6,

		/**
		 * Replicate a network config for a network we belong to:
		 *   <[...] network config chunk>
		 *
		 * This is used by clusters to avoid every member having to query
		 * for the same netconf for networks all members belong to.
		 *
		 * The first field of a network config chunk is the network ID,
		 * so this can be checked to look up the network on receipt.
		 */
		CLUSTER_MESSAGE_NETWORK_CONFIG = 7
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
	 * Broadcast that we have a given peer
	 *
	 * This should be done when new peers are first contacted.
	 *
	 * @param id Identity of peer
	 */
	void broadcastHavePeer(const Identity &id);

	/**
	 * Broadcast a network config chunk to other members of cluster
	 *
	 * @param chunk Chunk data
	 * @param len Length of chunk
	 */
	void broadcastNetworkConfigChunk(const void *chunk,unsigned int len);

	/**
	 * If the cluster has this peer, prepare the packet to send via cluster
	 *
	 * Note that outp is only armored (or modified at all) if the return value is a member ID.
	 *
	 * @param toPeerAddress Value of outp.destination(), simply to save additional lookup
	 * @param ts Result: set to time of last HAVE_PEER from the cluster
	 * @param peerSecret Result: Buffer to fill with peer secret on valid return value, must be at least ZT_PEER_SECRET_KEY_LENGTH bytes
	 * @return -1 if cluster does not know this peer, or a member ID to pass to sendViaCluster()
	 */
	int checkSendViaCluster(const Address &toPeerAddress,uint64_t &mostRecentTs,void *peerSecret);

	/**
	 * Send data via cluster front plane (packet head or fragment)
	 *
	 * @param haveMemberId Member ID that has this peer as returned by prepSendviaCluster()
	 * @param toPeerAddress Destination peer address
	 * @param data Packet or packet fragment data
	 * @param len Length of packet or fragment
	 * @return True if packet was sent (and outp was modified via armoring)
	 */
	bool sendViaCluster(int haveMemberId,const Address &toPeerAddress,const void *data,unsigned int len);

	/**
	 * Relay a packet via the cluster
	 *
	 * This is used in the outgoing packet and relaying logic in Switch to
	 * relay packets to other cluster members. It isn't PROXY_SEND-- that is
	 * used internally in Cluster to send responses to peer queries.
	 *
	 * @param fromPeerAddress Source peer address (if known, should be NULL for fragments)
	 * @param toPeerAddress Destination peer address
	 * @param data Packet or packet fragment data
	 * @param len Length of packet or fragment
	 * @param unite If true, also request proxy unite across cluster
	 */
	void relayViaCluster(const Address &fromPeerAddress,const Address &toPeerAddress,const void *data,unsigned int len,bool unite);

	/**
	 * Send a distributed query to other cluster members
	 *
	 * Some queries such as WHOIS or MULTICAST_GATHER need a response from other
	 * cluster members. Replies (if any) will be sent back to the peer via
	 * PROXY_SEND across the cluster.
	 *
	 * @param pkt Packet to distribute
	 */
	void sendDistributedQuery(const Packet &pkt);

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
	 * @param ip Address to check
	 * @return True if this is a cluster frontplane address (excluding our addresses)
	 */
	bool isClusterPeerFrontplane(const InetAddress &ip) const;

	/**
	 * Fill out ZT_ClusterStatus structure (from core API)
	 *
	 * @param status Reference to structure to hold result (anything there is replaced)
	 */
	void status(ZT_ClusterStatus &status) const;

private:
	void _send(uint16_t memberId,StateMessageType type,const void *msg,unsigned int len);
	void _flush(uint16_t memberId);

	void _doREMOTE_WHOIS(uint64_t fromMemberId,const Packet &remotep);
	void _doREMOTE_MULTICAST_GATHER(uint64_t fromMemberId,const Packet &remotep);

	// These are initialized in the constructor and remain immutable ------------
	uint16_t _masterSecret[ZT_SHA512_DIGEST_LEN / sizeof(uint16_t)];
	unsigned char _key[ZT_PEER_SECRET_KEY_LENGTH];
	const RuntimeEnvironment *RR;
	_ClusterSendQueue *const _sendQueue;
	void (*_sendFunction)(void *,unsigned int,const void *,unsigned int);
	void *_sendFunctionArg;
	int (*_addressToLocationFunction)(void *,const struct sockaddr_storage *,int *,int *,int *);
	void *_addressToLocationFunctionArg;
	const int32_t _x;
	const int32_t _y;
	const int32_t _z;
	const uint16_t _id;
	const std::vector<InetAddress> _zeroTierPhysicalEndpoints;
	// end immutable fields -----------------------------------------------------

	struct _Member
	{
		unsigned char key[ZT_PEER_SECRET_KEY_LENGTH];

		uint64_t lastReceivedAliveAnnouncement;
		uint64_t lastAnnouncedAliveTo;

		uint64_t load;
		uint64_t peers;
		int32_t x,y,z;

		std::vector<InetAddress> zeroTierPhysicalEndpoints;

		Buffer<ZT_CLUSTER_MAX_MESSAGE_LENGTH> q;

		Mutex lock;

		inline void clear()
		{
			lastReceivedAliveAnnouncement = 0;
			lastAnnouncedAliveTo = 0;
			load = 0;
			peers = 0;
			x = 0;
			y = 0;
			z = 0;
			zeroTierPhysicalEndpoints.clear();
			q.clear();
		}

		_Member() { this->clear(); }
		~_Member() { Utils::burn(key,sizeof(key)); }
	};
	_Member *const _members;

	std::vector<uint16_t> _memberIds;
	Mutex _memberIds_m;

	struct _RemotePeer
	{
		_RemotePeer() : lastHavePeerReceived(0),lastSentWantPeer(0) {}
		~_RemotePeer() { Utils::burn(key,ZT_PEER_SECRET_KEY_LENGTH); }
		uint64_t lastHavePeerReceived;
		uint64_t lastSentWantPeer;
		uint8_t key[ZT_PEER_SECRET_KEY_LENGTH]; // secret key from identity agreement
	};
	std::map< std::pair<Address,unsigned int>,_RemotePeer > _remotePeers; // we need ordered behavior and lower_bound here
	Mutex _remotePeers_m;

	uint64_t _lastFlushed;
	uint64_t _lastCleanedRemotePeers;
	uint64_t _lastCleanedQueue;
};

} // namespace ZeroTier

#endif // ZT_ENABLE_CLUSTER

#endif
