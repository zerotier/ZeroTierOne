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
 * Feature flag: this is an official ZeroTier, Inc. binary build (built with ZT_OFFICIAL_RELEASE)
 */
#define ZT1_FEATURE_FLAG_OFFICIAL 0x00000001

/**
 * Feature flag: ZeroTier One was built to be thread-safe -- concurrent processXXX() calls are okay
 */
#define ZT1_FEATURE_FLAG_THREAD_SAFE 0x00000002

/**
 * Feature flag: FIPS compliant build (not available yet, but reserved for future use if we ever do this)
 */
#define ZT1_FEATURE_FLAG_FIPS 0x00000004

/****************************************************************************/
/* Structures and other types                                               */
/****************************************************************************/

/**
 * Function return values: OK or various error conditions
 */
enum ZT1_ResultCode
{
	/**
	 * Operation completed normally
	 */
	ZT1_RESULT_OK = 0,

	/**
	 * Our identity collides with another on the network
	 *
	 * This is profoundly unlikely: once in about 2^39 identities. If this
	 * happens to you, delete identity.public and identity.secret from your
	 * data store / home path and restart. You might also avoid shark infested
	 * waters, hide during thunderstorms, and consider playing the lottery.
	 */
	ZT1_RESULT_ERROR_IDENTITY_COLLISION = 1,

	/**
	 * Ran out of memory
	 */
	ZT1_RESULT_ERROR_OUT_OF_MEMORY = 2,

	/**
	 * Data store is not writable or has failed
	 */
	ZT1_RESULT_ERROR_DATA_STORE_FAILED = 3,

	/**
	 * Internal error (e.g. unexpected exception, build problem, link problem, etc.)
	 */
	ZT1_RESULT_ERROR_INTERNAL = 4,

	/**
	 * Invalid packet or failed authentication
	 */
	ZT1_RESULT_PACKET_INVALID = 5
};

/**
 * Status codes sent to status update callback when things happen
 */
enum ZT1_NodeStatusCode
{
	/**
	 * Node is offline -- nothing is reachable
	 */
	ZT1_NODE_STATUS_OFFLINE = 0,

	/**
	 * Node is online -- at least one upstream is reachable
	 */
	ZT1_NODE_STATUS_ONLINE = 1,

	/**
	 * Link desperation level has changed
	 */
	ZT1_NODE_STATUS_DESPERATION_CHANGE = 3
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

	/**
	 * Current maximum link desperation metric
	 */
	int desperation;
} ZT1_NodeStatus;

/**
 * Virtual network status codes
 */
enum ZT1_VirtualNetworkStatus
{
	/**
	 * Waiting for network configuration (also means revision == 0)
	 */
	ZT1_NETWORK_STATUS_WAITING = 0,

	/**
	 * Configuration received and we are authorized
	 */
	ZT1_NETWORK_STATUS_AUTHORIZED = 1,

	/**
	 * Netconf master told us 'nope'
	 */
	ZT1_NETWORK_STATUS_ACCESS_DENIED = 2,

	/**
	 * Netconf master exists, but this virtual network does not
	 */
	ZT1_NETWORK_STATUS_NOT_FOUND = 3
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
	 * Network config revision as reported by netconf master
	 *
	 * If this is zero, it means we're still waiting for our netconf.
	 */
	unsigned long netconfRevision;

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
	const struct sockaddr_storage *assignedAddresses;

	/**
	 * Number of assigned addresses
	 */
	unsigned int assignedAddressCount;

	/**
	 * Multicast group subscriptions
	 */
	ZT1_MulticastGroup *multicastSubscriptions;

	/**
	 * Number of multicast group subscriptions
	 */
	unsigned int multicastSubscriptionCount;

	/**
	 * Network name (from network configuration master)
	 */
	const char *networkName;
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
	 * Time since last send in milliseconds or -1 for never
	 */
	long lastSend;

	/**
	 * Time since last receive in milliseconds or -1 for never
	 */
	long lastReceive;

	/**
	 * Time since last ping sent in milliseconds or -1 for never
	 */
	long lastPing;

	/**
	 * Time since last firewall opener sent in milliseconds or -1 for never
	 */
	long lastFirewallOpener;

	/**
	 * Total bytes sent
	 */
	uint64_t bytesSent;

	/**
	 * Total bytes received
	 */
	uint64_t bytesReceived;

	/**
	 * This path's desperation metric (higher == worse)
	 */
	int desperation;

	/**
	 * Is path fixed? (i.e. not learned, static)
	 */
	int fixed;
} ZT1_PeerPhysicalPath;

/**
 * What trust hierarchy role does this peer have?
 */
enum ZT1_PeerRole {
	ZT1_PEER_ROLE_NODE = 0,     // ordinary node
	ZT1_PEER_ROLE_HUB = 1,      // locally federated hub
	ZT1_PEER_ROLE_SUPERNODE = 2 // planetary supernode
};

/**
 * Peer status result buffer
 */
typedef struct
{
	/**
	 * ZeroTier binary address (40 bits)
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
	 * Array of network paths to peer
	 */
	ZT1_PeerPhysicalPath *paths;

	/**
	 * Number of paths (size of paths[])
	 */
	unsigned long pathCount;
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
 * Callback called to update virtual port configuration
 *
 * This can be called at any time to update the configuration of a virtual
 * network port. If a port is deleted (via leave() or otherwise) this is
 * called with a NULL config parameter.
 *
 * This in turn should be used by the underlying implementation to create
 * and configure tap devices to handle frames, etc.
 *
 * The supplied config pointer is not guaranteed to remain valid, so make
 * a copy if you want one.
 */
typedef void (*ZT1_VirtualNetworkConfigCallback)(ZT1_Node *,uint64_t,const ZT1_VirtualNetworkConfig *);

/**
 * Callback for status messages
 *
 * This is called whenever the node's status changes in some significant way.
 */
typedef void (*ZT1_StatusCallback)(ZT1_Node *,enum ZT1_NodeStatusCode);

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
typedef long (*ZT1_DataStoreGetFunction)(ZT1_Node *,const char *,void *,unsigned long,unsigned long,unsigned long *);

/**
 * Function to store an object in the data store
 *
 * Parameters: (1) node, (2) object name, (3) object data, (4) object size,
 * and (5) secure? (bool). If secure is true, the file should be set readable
 * and writable only to the user running ZeroTier One. What this means is
 * platform-specific.
 *
 * Name semantics are the same as the get function. This must return zero on
 * success. You can return any OS-specific error code on failure, as these
 * may be visible in logs or error messages and might aid in debugging.
 *
 * A call to write 0 bytes with a null data pointer should be interpreted
 * as a delete operation. The secure flag is not meaningful in this case.
 */
typedef int (*ZT1_DataStorePutFunction)(ZT1_Node *,const char *,const void *,unsigned long,int);

/**
 * Function to send a ZeroTier packet out over the wire
 *
 * Parameters: (1) node, (2) address, (3) link desperation,
 * (4) packet data, (5) packet data length.
 *
 * The function must return zero on success and may return any error code
 * on failure. Note that success does not (of course) guarantee packet
 * delivery. It only means that the packet appears to have been sent.
 */
typedef int (*ZT1_WirePacketSendFunction)(ZT1_Node *,const struct sockaddr_storage *,int,const void *,unsigned int);

/**
 * Function to send a frame out to a virtual network port
 *
 * Parameters: (1) node, (2) network ID, (3) source MAC, (4) destination MAC,
 * (5) ethertype, (6) VLAN ID, (7) frame data, (8) frame length.
 */
typedef void (*ZT1_VirtualNetworkFrameFunction)(ZT1_Node *,uint64_t,uint64_t,uint64_t,unsigned int,unsigned int,const void *,unsigned int);

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
 * @param now Current clock in milliseconds
 * @param dataStoreGetFunction Function called to get objects from persistent storage
 * @param dataStorePutFunction Function called to put objects in persistent storage
 * @param virtualNetworkConfigCallback Function to be called when virtual LANs are created, deleted, or their config parameters change
 * @param statusCallback Function to receive status updates and non-fatal error notices
 * @return OK (0) or error code if a fatal error condition has occurred
 */
enum ZT1_ResultCode ZT1_Node_new(
	ZT1_Node **node,
	uint64_t now,
	ZT1_DataStoreGetFunction dataStoreGetFunction,
	ZT1_DataStorePutFunction dataStorePutFunction,
	ZT1_WirePacketSendFunction wirePacketSendFunction,
	ZT1_VirtualNetworkFrameFunction virtualNetworkFrameFunction,
	ZT1_VirtualNetworkConfigCallback virtualNetworkConfigCallback,
	ZT1_StatusCallback statusCallback);

/**
 * Process a packet received from the physical wire
 *
 * @param node Node instance
 * @param now Current clock in milliseconds
 * @param remoteAddress Origin of packet
 * @param linkDesperation Link desperation metric for link or protocol over which packet arrived
 * @param packetData Packet data
 * @param packetLength Packet length
 * @param nextCallDeadline Result: set to deadline for next call to one of the three processXXX() methods
 * @return OK (0) or error code if a fatal error condition has occurred
 */
enum ZT1_ResultCode ZT1_Node_processWirePacket(
	ZT1_Node *node,
	uint64_t now,
	const struct sockaddr_storage *remoteAddress,
	int linkDesperation,
	const void *packetData,
	unsigned int packetLength,
	uint64_t *nextCallDeadline);

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
 * @param nextCallDeadline Result: set to deadline for next call to one of the three processXXX() methods
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
	uint64_t *nextCallDeadline);

/**
 * Perform required periodic operations even if no new frames or packets have arrived
 *
 * If the nextCallDeadline arrives and nothing has happened, call this method
 * to do required background tasks like pinging and cleanup.
 *
 * @param node Node instance
 * @param now Current clock in milliseconds
 * @param nextCallDeadline Result: set to deadline for next call to one of the three processXXX() methods
 * @return OK (0) or error code if a fatal error condition has occurred
 */
enum ZT1_ResultCode ZT1_Node_processNothing(ZT1_Node *node,uint64_t now,uint64_t *nextCallDeadline);

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
 * effect.
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
ZT1_VirtualNetworkList *ZT1_Node_listNetworks(ZT1_Node *node);

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
