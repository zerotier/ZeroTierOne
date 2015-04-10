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

/*
 * This defines the external C API for ZeroTier One, the core network
 * virtualization engine.
 */

#ifndef ZT_ZEROTIERONE_H
#define ZT_ZEROTIERONE_H

#include <stdint.h>

// For the struct sockaddr_storage structure
#if defined(_WIN32) || defined(_WIN64)
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#else /* not Windows */
#include <arpa/inet.h>
#include <netinet/in.h>
#endif /* Windows or not */

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************/
/* Core constants                                                           */
/****************************************************************************/

/**
 * Default port for the ZeroTier service
 */
#define ZT1_DEFAULT_PORT 9993

/**
 * Maximum MTU for ZeroTier virtual networks
 *
 * This is pretty much an unchangeable global constant. To make it change
 * across nodes would require logic to send ICMP packet too big messages,
 * which would complicate things. 1500 has been good enough on most LANs
 * for ages, so a larger MTU should be fine for the forseeable future. This
 * typically results in two UDP packets per single large frame. Experimental
 * results seem to show that this is good. Larger MTUs resulting in more
 * fragments seemed too brittle on slow/crummy links for no benefit.
 *
 * If this does change, also change it in tap.h in the tuntaposx code under
 * mac-tap.
 * 
 * Overhead for a normal frame split into two packets:
 *
 * 1414 = 1444 (typical UDP MTU) - 28 (packet header) - 2 (ethertype)
 * 1428 = 1444 (typical UDP MTU) - 16 (fragment header)
 * SUM: 2842
 *
 * We use 2800, which leaves some room for other payload in other types of
 * messages such as multicast propagation or future support for bridging.
 */
#define ZT1_MAX_MTU 2800

/**
 * Maximum length of network short name
 */
#define ZT1_MAX_NETWORK_SHORT_NAME_LENGTH 255

/**
 * Maximum number of statically assigned IP addresses per network endpoint using ZT address management (not DHCP)
 */
#define ZT1_MAX_ZT_ASSIGNED_ADDRESSES 16

/**
 * Maximum number of multicast group subscriptions per network
 */
#define ZT1_MAX_NETWORK_MULTICAST_SUBSCRIPTIONS 4096

/**
 * Maximum number of direct network paths to a given peer
 */
#define ZT1_MAX_PEER_NETWORK_PATHS 4

/**
 * Feature flag: ZeroTier One was built to be thread-safe -- concurrent processXXX() calls are okay
 */
#define ZT1_FEATURE_FLAG_THREAD_SAFE 0x00000001

/**
 * Feature flag: FIPS compliant build (not available yet, but reserved for future use if we ever do this)
 */
#define ZT1_FEATURE_FLAG_FIPS 0x00000002

/****************************************************************************/
/* Structures and other types                                               */
/****************************************************************************/

/**
 * Function return code: OK (0) or error results
 *
 * Use ZT1_ResultCode_isFatal() to check for a fatal error. If a fatal error
 * occurs, the node should be considered to not be working correctly. These
 * indicate serious problems like an inaccessible data store or a compile
 * problem.
 */
enum ZT1_ResultCode
{
	/**
	 * Operation completed normally
	 */
	ZT1_RESULT_OK = 0,

	// Fatal errors (>0, <1000)

	/**
	 * Ran out of memory
	 */
	ZT1_RESULT_FATAL_ERROR_OUT_OF_MEMORY = 1,

	/**
	 * Data store is not writable or has failed
	 */
	ZT1_RESULT_FATAL_ERROR_DATA_STORE_FAILED = 2,

	/**
	 * Internal error (e.g. unexpected exception indicating bug or build problem)
	 */
	ZT1_RESULT_FATAL_ERROR_INTERNAL = 3,

	// Non-fatal errors (>1000)

	/**
	 * Network ID not valid
	 */
	ZT1_RESULT_ERROR_NETWORK_NOT_FOUND = 1000
};

/**
 * @param x Result code
 * @return True if result code indicates a fatal error
 */
#define ZT1_ResultCode_isFatal(x) ((((int)(x)) > 0)&&(((int)(x)) < 1000))

/**
 * Status codes sent to status update callback when things happen
 */
enum ZT1_Event
{
	/**
	 * Node has been initialized
	 *
	 * This is the first event generated, and is always sent. It may occur
	 * before Node's constructor returns.
	 *
	 * Meta-data: none
	 */
	ZT1_EVENT_UP = 0,

	/**
	 * Node is offline -- network does not seem to be reachable by any available strategy
	 *
	 * Meta-data: none
	 */
	ZT1_EVENT_OFFLINE = 1,

	/**
	 * Node is online -- at least one upstream node appears reachable
	 *
	 * Meta-data: none
	 */
	ZT1_EVENT_ONLINE = 2,

	/**
	 * Node is shutting down
	 *
	 * This is generated within Node's destructor when it is being shut down.
	 * It's done for convenience, since cleaning up other state in the event
	 * handler may appear more idiomatic.
	 *
	 * Meta-data: none
	 */
	ZT1_EVENT_DOWN = 3,

	/**
	 * Your identity has collided with another node's ZeroTier address
	 *
	 * This happens if two different public keys both hash (via the algorithm
	 * in Identity::generate()) to the same 40-bit ZeroTier address.
	 *
	 * This is something you should "never" see, where "never" is defined as
	 * once per 2^39 new node initializations / identity creations. If you do
	 * see it, you're going to see it very soon after a node is first
	 * initialized.
	 *
	 * This is reported as an event rather than a return code since it's
	 * detected asynchronously via error messages from authoritative nodes.
	 *
	 * If this occurs, you must shut down and delete the node, delete the
	 * identity.secret record/file from the data store, and restart to generate
	 * a new identity. If you don't do this, you will not be able to communicate
	 * with other nodes.
	 *
	 * We'd automate this process, but we don't think silently deleting
	 * private keys or changing our address without telling the calling code
	 * is good form. It violates the principle of least surprise.
	 *
	 * You can technically get away with not handling this, but we recommend
	 * doing so in a mature reliable application. Besides, handling this
	 * condition is a good way to make sure it never arises. It's like how
	 * umbrellas prevent rain and smoke detectors prevent fires. They do, right?
	 *
	 * Meta-data: none
	 */
	ZT1_EVENT_FATAL_ERROR_IDENTITY_COLLISION = 4,

	/**
	 * A more recent version was observed on the network
	 *
	 * Right now this is only triggered if a hub or supernode reports a
	 * more recent version, and only once. It can be used to trigger a
	 * software update check.
	 *
	 * Meta-data: unsigned int[3], more recent version number
	 */
	ZT1_EVENT_SAW_MORE_RECENT_VERSION = 5,

	/**
	 * A packet failed authentication
	 *
	 * Meta-data: struct sockaddr_storage containing origin address of packet
	 */
	ZT1_EVENT_AUTHENTICATION_FAILURE = 6,

	/**
	 * A received packet was not valid
	 *
	 * Meta-data: struct sockaddr_storage containing origin address of packet
	 */
	ZT1_EVENT_INVALID_PACKET = 7,

	/**
	 * Trace (debugging) message
	 *
	 * These events are only generated if this is a TRACE-enabled build.
	 *
	 * Meta-data: C string, TRACE message
	 */
	ZT1_EVENT_TRACE = 8
};

/**
 * Current node status
 */
typedef struct
{
	/**
	 * 40-bit ZeroTier address of this node
	 */
	uint64_t address;

	/**
	 * Public identity in string-serialized form (safe to send to others)
	 *
	 * This pointer will remain valid as long as the node exists.
	 */
	const char *publicIdentity;

	/**
	 * Full identity including secret key in string-serialized form
	 *
	 * This pointer will remain valid as long as the node exists.
	 */
	const char *secretIdentity;

	/**
	 * True if some kind of connectivity appears available
	 */
	int online;
} ZT1_NodeStatus;

/**
 * Virtual network status codes
 */
enum ZT1_VirtualNetworkStatus
{
	/**
	 * Waiting for network configuration (also means revision == 0)
	 */
	ZT1_NETWORK_STATUS_REQUESTING_CONFIGURATION = 0,

	/**
	 * Configuration received and we are authorized
	 */
	ZT1_NETWORK_STATUS_OK = 1,

	/**
	 * Netconf master told us 'nope'
	 */
	ZT1_NETWORK_STATUS_ACCESS_DENIED = 2,

	/**
	 * Netconf master exists, but this virtual network does not
	 */
	ZT1_NETWORK_STATUS_NOT_FOUND = 3,

	/**
	 * Initialization of network failed or other internal error
	 */
	ZT1_NETWORK_STATUS_PORT_ERROR = 4
};

/**
 * Virtual network type codes
 */
enum ZT1_VirtualNetworkType
{
	/**
	 * Private networks are authorized via certificates of membership
	 */
	ZT1_NETWORK_TYPE_PRIVATE = 0,

	/**
	 * Public networks have no access control -- they'll always be AUTHORIZED
	 */
	ZT1_NETWORK_TYPE_PUBLIC = 1
};

/**
 * An Ethernet multicast group
 */
typedef struct
{
	/**
	 * MAC address (least significant 48 bits)
	 */
	uint64_t mac;

	/**
	 * Additional distinguishing information (usually zero)
	 */
	unsigned long adi;
} ZT1_MulticastGroup;

/**
 * Virtual network configuration update type
 */
enum ZT1_VirtualNetworkConfigOperation
{
	/**
	 * Network is coming up (either for the first time or after service restart)
	 */
	ZT1_VIRTUAL_NETWORK_CONFIG_OPERATION_UP = 1,

	/**
	 * Network configuration has been updated
	 */
	ZT1_VIRTUAL_NETWORK_CONFIG_OPERATION_CONFIG_UPDATE = 2,

	/**
	 * Network is going down (not permanently)
	 */
	ZT1_VIRTUAL_NETWORK_CONFIG_OPERATION_DOWN = 3,

	/**
	 * Network is going down permanently (leave/delete)
	 */
	ZT1_VIRTUAL_NETWORK_CONFIG_OPERATION_DESTROY = 4
};

/**
 * Virtual LAN configuration
 */
typedef struct
{
	/**
	 * 64-bit ZeroTier network ID
	 */
	uint64_t nwid;

	/**
	 * Ethernet MAC (40 bits) that should be assigned to port
	 */
	uint64_t mac;

	/**
	 * Network name (from network configuration master)
	 */
	char name[ZT1_MAX_NETWORK_SHORT_NAME_LENGTH + 1];

	/**
	 * Network configuration request status
	 */
	enum ZT1_VirtualNetworkStatus status;

	/**
	 * Network type
	 */
	enum ZT1_VirtualNetworkType type;

	/**
	 * Maximum interface MTU
	 */
	unsigned int mtu;

	/**
	 * If nonzero, the network this port belongs to indicates DHCP availability
	 *
	 * This is a suggestion. The underlying implementation is free to ignore it
	 * for security or other reasons. This is simply a netconf parameter that
	 * means 'DHCP is available on this network.'
	 */
	int dhcp;

	/**
	 * If nonzero, this port is allowed to bridge to other networks
	 *
	 * This is informational. If this is false (0), bridged packets will simply
	 * be dropped and bridging won't work.
	 */
	int bridge;

	/**
	 * If nonzero, this network supports and allows broadcast (ff:ff:ff:ff:ff:ff) traffic
	 */
	int broadcastEnabled;

	/**
	 * If the network is in PORT_ERROR state, this is the error most recently returned by the port config callback
	 */
	int portError;

	/**
	 * Network config revision as reported by netconf master
	 *
	 * If this is zero, it means we're still waiting for our netconf.
	 */
	unsigned long netconfRevision;

	/**
	 * Number of multicast group subscriptions
	 */
	unsigned int multicastSubscriptionCount;

	/**
	 * Multicast group subscriptions
	 */
	ZT1_MulticastGroup multicastSubscriptions[ZT1_MAX_NETWORK_MULTICAST_SUBSCRIPTIONS];

	/**
	 * Number of assigned addresses
	 */
	unsigned int assignedAddressCount;

	/**
	 * ZeroTier-assigned addresses (in sockaddr_storage structures)
	 *
	 * For IP, the port number of the sockaddr_XX structure contains the number
	 * of bits in the address netmask. Only the IP address and port are used.
	 * Other fields like interface number can be ignored.
	 *
	 * This is only used for ZeroTier-managed address assignments sent by the
	 * virtual network's configuration master.
	 */
	struct sockaddr_storage assignedAddresses[ZT1_MAX_ZT_ASSIGNED_ADDRESSES];
} ZT1_VirtualNetworkConfig;

/**
 * A list of networks
 */
typedef struct
{
	ZT1_VirtualNetworkConfig *networks;
	unsigned long networkCount;
} ZT1_VirtualNetworkList;

/**
 * Physical network path to a peer
 */
typedef struct
{
	/**
	 * Address of endpoint
	 */
	struct sockaddr_storage address;

	/**
	 * Time of last send in milliseconds or 0 for never
	 */
	uint64_t lastSend;

	/**
	 * Time of last receive in milliseconds or 0 for never
	 */
	uint64_t lastReceive;

	/**
	 * Is path fixed? (i.e. not learned, static)
	 */
	int fixed;
} ZT1_PeerPhysicalPath;

/**
 * What trust hierarchy role does this peer have?
 */
enum ZT1_PeerRole {
	ZT1_PEER_ROLE_LEAF = 0,     // ordinary node
	ZT1_PEER_ROLE_HUB = 1,      // locally federated hub
	ZT1_PEER_ROLE_SUPERNODE = 2 // planetary supernode
};

/**
 * Peer status result buffer
 */
typedef struct
{
	/**
	 * ZeroTier address (40 bits)
	 */
	uint64_t address;

	/**
	 * Remote major version or -1 if not known
	 */
	int versionMajor;

	/**
	 * Remote minor version or -1 if not known
	 */
	int versionMinor;

	/**
	 * Remote revision or -1 if not known
	 */
	int versionRev;

	/**
	 * Last measured latency in milliseconds or zero if unknown
	 */
	unsigned int latency;

	/**
	 * What trust hierarchy role does this device have?
	 */
	enum ZT1_PeerRole role;

	/**
	 * Number of paths (size of paths[])
	 */
	unsigned int pathCount;

	/**
	 * Known network paths to peer
	 */
	ZT1_PeerPhysicalPath paths[ZT1_MAX_PEER_NETWORK_PATHS];
} ZT1_Peer;

/**
 * List of peers
 */
typedef struct
{
	ZT1_Peer *peers;
	unsigned long peerCount;
} ZT1_PeerList;

/**
 * An instance of a ZeroTier One node (opaque)
 */
typedef void ZT1_Node;

/****************************************************************************/
/* Callbacks used by Node API                                               */
/****************************************************************************/

/**
 * Callback called to update virtual network port configuration
 *
 * This can be called at any time to update the configuration of a virtual
 * network port. The parameter after the network ID specifies whether this
 * port is being brought up, updated, brought down, or permanently deleted.
 *
 * This in turn should be used by the underlying implementation to create
 * and configure tap devices at the OS (or virtual network stack) layer.
 *
 * The supplied config pointer is not guaranteed to remain valid, so make
 * a copy if you want one.
 *
 * This must return 0 on success. It can return any OS-dependent error code
 * on failure, and this results in the network being placed into the
 * PORT_ERROR state.
 */
typedef int (*ZT1_VirtualNetworkConfigFunction)(ZT1_Node *,void *,uint64_t,enum ZT1_VirtualNetworkConfigOperation,const ZT1_VirtualNetworkConfig *);

/**
 * Callback for events
 *
 * Events are generated when the node's status changes in a significant way
 * and on certain non-fatal errors and events of interest. The final void
 * parameter points to event meta-data. The type of event meta-data (and
 * whether it is present at all) is event type dependent. See the comments
 * in the definition of ZT1_Event.
 */
typedef void (*ZT1_EventCallback)(ZT1_Node *,void *,enum ZT1_Event,const void *);

/**
 * Function to get an object from the data store
 *
 * Parameters: (1) object name, (2) buffer to fill, (3) size of buffer, (4)
 * index in object to start reading, (5) result parameter that must be set
 * to the actual size of the object if it exists.
 *
 * Object names can contain forward slash (/) path separators. They will
 * never contain .. or backslash (\), so this is safe to map as a Unix-style
 * path if the underlying storage permits. For security reasons we recommend
 * returning errors if .. or \ are used.
 *
 * The function must return the actual number of bytes read. If the object
 * doesn't exist, it should return -1. -2 should be returned on other errors
 * such as errors accessing underlying storage.
 *
 * If the read doesn't fit in the buffer, the max number of bytes should be
 * read. The caller may call the function multiple times to read the whole
 * object.
 */
typedef long (*ZT1_DataStoreGetFunction)(ZT1_Node *,void *,const char *,void *,unsigned long,unsigned long,unsigned long *);

/**
 * Function to store an object in the data store
 *
 * Parameters: (1) node, (2) user ptr, (3) object name, (4) object data,
 * (5) object size, (6) secure? (bool).
 *
 * If secure is true, the file should be set readable and writable only
 * to the user running ZeroTier One. What this means is platform-specific.
 *
 * Name semantics are the same as the get function. This must return zero on
 * success. You can return any OS-specific error code on failure, as these
 * may be visible in logs or error messages and might aid in debugging.
 *
 * A call to write 0 bytes with a null data pointer should be interpreted
 * as a delete operation. The secure flag is not meaningful in this case.
 */
typedef int (*ZT1_DataStorePutFunction)(ZT1_Node *,void *,const char *,const void *,unsigned long,int);

/**
 * Function to send a ZeroTier packet out over the wire
 *
 * Parameters: (1) node, (2) user ptr, (3) address, (4) link desperation,
 * (5) packet data, (6) packet data length.
 *
 * The function must return zero on success and may return any error code
 * on failure. Note that success does not (of course) guarantee packet
 * delivery. It only means that the packet appears to have been sent.
 */
typedef int (*ZT1_WirePacketSendFunction)(ZT1_Node *,void *,const struct sockaddr_storage *,unsigned int,const void *,unsigned int);

/**
 * Function to send a frame out to a virtual network port
 *
 * Parameters: (1) node, (2) user ptr, (3) network ID, (4) source MAC,
 * (5) destination MAC, (6) ethertype, (7) VLAN ID, (8) frame data,
 * (9) frame length.
 */
typedef void (*ZT1_VirtualNetworkFrameFunction)(ZT1_Node *,void *,uint64_t,uint64_t,uint64_t,unsigned int,unsigned int,const void *,unsigned int);

/****************************************************************************/
/* C Node API                                                               */
/****************************************************************************/

/**
 * Create a new ZeroTier One node
 *
 * Note that this can take a few seconds the first time it's called, as it
 * will generate an identity.
 *
 * @param node Result: pointer is set to new node instance on success
 * @param uptr User pointer to pass to functions/callbacks
 * @param now Current clock in milliseconds
 * @param dataStoreGetFunction Function called to get objects from persistent storage
 * @param dataStorePutFunction Function called to put objects in persistent storage
 * @param virtualNetworkConfigFunction Function to be called when virtual LANs are created, deleted, or their config parameters change
 * @param eventCallback Function to receive status updates and non-fatal error notices
 * @param overrideRootTopology If not NULL, must contain string-serialize root topology (for testing, default: NULL)
 * @return OK (0) or error code if a fatal error condition has occurred
 */
enum ZT1_ResultCode ZT1_Node_new(
	ZT1_Node **node,
	void *uptr,
	uint64_t now,
	ZT1_DataStoreGetFunction dataStoreGetFunction,
	ZT1_DataStorePutFunction dataStorePutFunction,
	ZT1_WirePacketSendFunction wirePacketSendFunction,
	ZT1_VirtualNetworkFrameFunction virtualNetworkFrameFunction,
	ZT1_VirtualNetworkConfigFunction virtualNetworkConfigFunction,
	ZT1_EventCallback eventCallback,
	const char *overrideRootTopology = (const char *)0);

/**
 * Delete a node and free all resources it consumes
 *
 * If you are using multiple threads, all other threads must be shut down
 * first. This can crash if processXXX() methods are in progress.
 *
 * @param node Node to delete
 */
void ZT1_Node_delete(ZT1_Node *node);

/**
 * Process a packet received from the physical wire
 *
 * @param node Node instance
 * @param now Current clock in milliseconds
 * @param remoteAddress Origin of packet
 * @param linkDesperation Link desperation metric for link or protocol over which packet arrived
 * @param packetData Packet data
 * @param packetLength Packet length
 * @param nextBackgroundTaskDeadline Value/result: set to deadline for next call to one of the three processXXX() methods
 * @return OK (0) or error code if a fatal error condition has occurred
 */
enum ZT1_ResultCode ZT1_Node_processWirePacket(
	ZT1_Node *node,
	uint64_t now,
	const struct sockaddr_storage *remoteAddress,
	unsigned int linkDesperation,
	const void *packetData,
	unsigned int packetLength,
	uint64_t *nextBackgroundTaskDeadline);

/**
 * Process a frame from a virtual network port (tap)
 *
 * @param node Node instance
 * @param now Current clock in milliseconds
 * @param nwid ZeroTier 64-bit virtual network ID
 * @param sourceMac Source MAC address (least significant 48 bits)
 * @param destMac Destination MAC address (least significant 48 bits)
 * @param etherType 16-bit Ethernet frame type
 * @param vlanId 10-bit VLAN ID or 0 if none
 * @param frameData Frame payload data
 * @param frameLength Frame payload length
 * @param nextBackgroundTaskDeadline Value/result: set to deadline for next call to one of the three processXXX() methods
 * @return OK (0) or error code if a fatal error condition has occurred
 */
enum ZT1_ResultCode ZT1_Node_processVirtualNetworkFrame(
	ZT1_Node *node,
	uint64_t now,
	uint64_t nwid,
	uint64_t sourceMac,
	uint64_t destMac,
	unsigned int etherType,
	unsigned int vlanId,
	const void *frameData,
	unsigned int frameLength,
	uint64_t *nextBackgroundTaskDeadline);

/**
 * Perform required periodic operations even if no new frames or packets have arrived
 *
 * @param node Node instance
 * @param now Current clock in milliseconds
 * @param nextBackgroundTaskDeadline Value/result: set to deadline for next call to one of the three processXXX() methods
 * @return OK (0) or error code if a fatal error condition has occurred
 */
enum ZT1_ResultCode ZT1_Node_processBackgroundTasks(ZT1_Node *node,uint64_t now,uint64_t *nextBackgroundTaskDeadline);

/**
 * Join a network
 *
 * This may generate calls to the port config callback before it returns,
 * or these may be deffered if a netconf is not available yet.
 *
 * @param node Node instance
 * @param nwid 64-bit ZeroTier network ID
 * @return OK (0) or error code if a fatal error condition has occurred
 */
enum ZT1_ResultCode ZT1_Node_join(ZT1_Node *node,uint64_t nwid);

/**
 * Leave a network
 *
 * If a port has been configured for this network this will generate a call
 * to the port config callback with a NULL second parameter to indicate that
 * the port is now deleted.
 *
 * @param node Node instance
 * @param nwid 64-bit network ID
 * @return OK (0) or error code if a fatal error condition has occurred
 */
enum ZT1_ResultCode ZT1_Node_leave(ZT1_Node *node,uint64_t nwid);

/**
 * Subscribe to an Ethernet multicast group
 *
 * ADI stands for additional distinguishing information. This defaults to zero
 * and is rarely used. Right now its only use is to enable IPv4 ARP to scale,
 * and this must be done.
 *
 * For IPv4 ARP, the implementation must subscribe to 0xffffffffffff (the
 * broadcast address) but with an ADI equal to each IPv4 address in host
 * byte order. This converts ARP from a non-scalable broadcast protocol to
 * a scalable multicast protocol with perfect address specificity.
 *
 * If this is not done, ARP will not work reliably.
 *
 * Multiple calls to subscribe to the same multicast address will have no
 * effect. It is perfectly safe to do this.
 *
 * This does not generate an update call to networkConfigCallback().
 *
 * @param node Node instance
 * @param nwid 64-bit network ID
 * @param multicastGroup Ethernet multicast or broadcast MAC (least significant 48 bits)
 * @param multicastAdi Multicast ADI (least significant 32 bits only, default: 0)
 * @return OK (0) or error code if a fatal error condition has occurred
 */
enum ZT1_ResultCode ZT1_Node_multicastSubscribe(ZT1_Node *node,uint64_t nwid,uint64_t multicastGroup,unsigned long multicastAdi = 0);

/**
 * Unsubscribe from an Ethernet multicast group (or all groups)
 *
 * If multicastGroup is zero (0), this will unsubscribe from all groups. If
 * you are not subscribed to a group this has no effect.
 *
 * This does not generate an update call to networkConfigCallback().
 *
 * @param node Node instance
 * @param nwid 64-bit network ID
 * @param multicastGroup Ethernet multicast or broadcast MAC (least significant 48 bits)
 * @param multicastAdi Multicast ADI (least significant 32 bits only, default: 0)
 * @return OK (0) or error code if a fatal error condition has occurred
 */
enum ZT1_ResultCode ZT1_Node_multicastUnsubscribe(ZT1_Node *node,uint64_t nwid,uint64_t multicastGroup,unsigned long multicastAdi = 0);

/**
 * Get the status of this node
 *
 * @param node Node instance
 * @param status Buffer to fill with current node status
 */
void ZT1_Node_status(ZT1_Node *node,ZT1_NodeStatus *status);

/**
 * Get a list of known peer nodes
 *
 * The pointer returned here must be freed with freeQueryResult()
 * when you are done with it.
 *
 * @param node Node instance
 * @return List of known peers or NULL on failure
 */
ZT1_PeerList *ZT1_Node_peers(ZT1_Node *node);

/**
 * Get the status of a virtual network
 *
 * The pointer returned here must be freed with freeQueryResult()
 * when you are done with it.
 *
 * @param node Node instance
 * @param nwid 64-bit network ID
 * @return Network configuration or NULL if we are not a member of this network
 */
ZT1_VirtualNetworkConfig *ZT1_Node_networkConfig(ZT1_Node *node,uint64_t nwid);

/**
 * Enumerate and get status of all networks
 *
 * @param node Node instance
 * @return List of networks or NULL on failure
 */
ZT1_VirtualNetworkList *ZT1_Node_networks(ZT1_Node *node);

/**
 * Free a query result buffer
 *
 * Use this to free the return values of listNetworks(), listPeers(), etc.
 *
 * @param node Node instance
 * @param qr Query result buffer
 */
void ZT1_Node_freeQueryResult(ZT1_Node *node,void *qr);

/**
 * Set a network configuration master instance for this node
 *
 * Normal nodes should not need to use this. This is for nodes with
 * special compiled-in support for acting as network configuration
 * masters / controllers.
 *
 * The supplied instance must be a C++ object that inherits from the
 * NetworkConfigMaster base class in node/. No type checking is performed,
 * so a pointer to anything else will result in a crash.
 *
 * @param node ZertTier One node
 * @param networkConfigMasterInstance Instance of NetworkConfigMaster C++ class or NULL to disable
 * @return OK (0) or error code if a fatal error condition has occurred
 */
void ZT1_Node_setNetconfMaster(ZT1_Node *node,void *networkConfigMasterInstance);

/**
 * Get ZeroTier One version
 *
 * @param major Result: major version
 * @param minor Result: minor version
 * @param revision Result: revision
 * @param featureFlags: Result: feature flag bitmap
 */
void ZT1_version(int *major,int *minor,int *revision,unsigned long *featureFlags);

#ifdef __cplusplus
}
#endif

#endif
