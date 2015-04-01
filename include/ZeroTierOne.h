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
 * Maximum frame MTU
 */
#define ZT1_MAX_MTU 2800

/**
 * Maximum length of a wire message packet in bytes
 */
#define ZT1_MAX_WIRE_MESSAGE_LENGTH 1500

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
	ZT1_RESULT_ERROR_DATA_STORE_FAILED = 3
};

/**
 * Status codes sent to status update callback when things happen
 */
enum ZT1_NodeStatusCode
{
	/**
	 * Node is online
	 */
	ZT1_NODE_STATUS_ONLINE = 1,

	/**
	 * Node is offline -- nothing is reachable
	 */
	ZT1_NODE_STATUS_OFFLINE = 2,

	/**
	 * The desperation level has changed
	 *
	 * 'extra' will point to an int containing the new level.
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
 * A message to or from a physical address (e.g. IP or physical Ethernet)
 */
typedef struct
{
	/**
	 * Socket address
	 */
	struct sockaddr_storage address;

	/**
	 * Link desperation -- higher equals "worse" or "slower"
	 *
	 * This is very similar to an interface metric. Higher values indicate
	 * worse links. For incoming wire messages, it should be sent to the
	 * desperation metric for the originating socket. For outgoing wire
	 * messages, ZeroTier will increment this from zero as it grows more
	 * and more desperate to communicate.
	 *
	 * In other words, this value controls fallback to things like TCP
	 * tunnels to relays. As desperation increases, ZeroTier becomes
	 * more and more willing to use these links.
	 *
	 * Desperation values shouldn't be arbitrary. They should be tied to
	 * specific transport types. For example: 0 might be UDP, 1 might be
	 * TCP, and 2 might be HTTP relay via a ZeroTier relay server. There
	 * should be no gaps. Negative values are permitted and may refer to
	 * better-than-normal links such as direct raw Ethernet framing over
	 * a trusted backplane.
	 */
	int desperation;

	/**
	 * If nonzero (true), spam this message across paths up to 'desperation'
	 *
	 * This works with 'desperation' to allow fall-forward to less desperate
	 * paths. When this flag is set, this message should be sent across all
	 * applicable transports up to and including the specified level of
	 * desperation.
	 *
	 * For example, if spam==1 and desperation==2 the packet might be sent
	 * via both UDP and HTTP tunneling.
	 */
	int spam;

	/**
	 * Packet data
	 */
	const char packetData[ZT1_MAX_WIRE_MESSAGE_LENGTH];

	/**
	 * Length of packet
	 */
	unsigned int packetLength;
} ZT1_WireMessage;

/**
 * A message to or from a virtual LAN port
 */
typedef struct
{
	/**
	 * ZeroTier network ID of virtual LAN port
	 */
	uint64_t nwid;

	/**
	 * Source MAC address
	 */
	uint64_t sourceMac;

	/**
	 * Destination MAC address
	 */
	uint64_t destMac;

	/**
	 * 16-bit Ethernet frame type
	 */
	unsigned int etherType;

	/**
	 * 10-bit VLAN ID or 0 for none
	 */
	unsigned int vlanId;

	/**
	 * Ethernet frame data
	 */
	const char frameData[ZT1_MAX_MTU];

	/**
	 * Ethernet frame length
	 */
	unsigned int frameLength;
} ZT1_VirtualNetworkFrame;

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
 * What trust hierarchy role does this device have?
 */
enum ZT1_PeerRole {
	ZT1_PEER_ROLE_SUPERNODE = 0, // planetary supernode
	ZT1_PEER_ROLE_HUB = 1,       // locally federated hub
	ZT1_PEER_ROLE_NODE = 2       // ordinary node
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
 * Parameters: (1) object name, (2) object data, (3) object size. Naming
 * semantics are the same as the get function. This must return zero on
 * success. You can return any OS-specific error code on failure, as these
 * may be visible in logs or error messages and might aid in debugging.
 *
 * A call to write 0 bytes can safely be interpreted as a delete operation.
 */
typedef int (*ZT1_DataStorePutFunction)(ZT1_Node *,const char *,const void *,unsigned long);

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
 * @param dataStoreGetFunction Function called to get objects from persistent storage
 * @param dataStorePutFunction Function called to put objects in persistent storage
 * @param networkConfigCallback Function to be called when virtual LANs are created, deleted, or their config parameters change
 * @param statusCallback Function to receive status updates and non-fatal error notices
 * @return OK (0) or error code if a fatal error condition has occurred
 */
enum ZT1_ResultCode ZT1_Node_new(
	ZT1_Node **node,
	ZT1_DataStoreGetFunction *dataStoreGetFunction,
	ZT1_DataStorePutFunction *dataStorePutFunction,
	ZT1_VirtualNetworkConfigCallback *networkConfigCallback,
	ZT1_StatusCallback *statusCallback);

/**
 * Process wire messages and/or LAN frames
 *
 * This runs the ZeroTier core loop once with input packets and frames and
 * returns zero or more resulting packets or frames. It also sets a max
 * interval value. The calling code must call run() again after no more
 * than this many milliseconds of inactivity. If no packets have been
 * received, it's fine to call run() with no inputs after the inactivity
 * timeout.
 *
 * In addition to normal inputs and outputs, any callbacks registered
 * with the ZeroTier One core may also be called such as virtual network
 * endpoint configuration update or diagnostic message handlers.
 *
 * The supplied time must be at millisecond resolution and must increment
 * monotonically from the time the Node is created. Other than that, there
 * are no other restrictions. On normal systems this is usually the system
 * clock measured in milliseconds since the epoch.
 *
 * @param node Node instance
 * @param now Current time at millisecond resolution (typically since epoch)
 * @param inputWireMessages ZeroTier transport packets from the wire
 * @param inputWireMessageCount Number of packets received
 * @param inputLanFrames Frames read from virtual LAN tap device
 * @param inputLanFrameCount Number of frames read
 * @param outputWireMessages Result: set to array of wire messages to be sent
 * @param outputWireMessageCount Result: set to size of *outputWireMessages[]
 * @param outputLanFrames Result: set to array of LAN frames to post to tap device
 * @param outputLanFrameCount Result: set to size of outputLanFrames[]
 * @param maxNextInterval Result: maximum number of milliseconds before next call to run() is needed
 * @return OK (0) or error code if a fatal error condition has occurred
 */
enum ZT1_ResultCode ZT1_Node_run(
	ZT1_Node *node,
	uint64_t now,
	const ZT1_WireMessage *inputWireMessages,
	unsigned int inputWireMessageCount,
	const ZT1_VirtualNetworkFrame *inputFrames,
	unsigned int inputFrameCount,
	const ZT1_WireMessage **outputWireMessages,
	unsigned int *outputWireMessageCount,
	const ZT1_VirtualNetworkFrame **outputFrames,
	unsigned int *outputLanFrameCount,
	unsigned long *maxNextInterval);

/**
 * Join a network
 *
 * This may generate calls to the port config callback before it returns,
 * or these may be deffered if a netconf is not available yet.
 *
 * @param node Node instance
 * @param nwid 64-bit ZeroTIer network ID
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
 * @param qr Query result buffer
 */
void ZT1_Node_freeQueryResult(void *qr);

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
enum ZT1_ResultCode ZT1_Node_setNetconfMaster(
	ZT1_Node *node,
	void *networkConfigMasterInstance);

/**
 * Get ZeroTier One version
 *
 * @param major Result: major version
 * @param minor Result: minor version
 * @param revision Result: revision
 */
void ZT1_version(int *major,int *minor,int *revision);

#ifdef __cplusplus
}
#endif

#endif
