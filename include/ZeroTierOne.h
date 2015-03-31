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

#ifndef ZT_SOCKADDR_STORAGE

#if defined(_WIN32) || defined(_WIN64)

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>

#else // not Windows

#include <netinet/in.h>

#endif // Windows or not

#define ZT_SOCKADDR_STORAGE struct sockaddr_storage

#endif // !ZT_SOCKADDR_STORAGE

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Function return values: OK or various error conditions
 */
enum ZT1_ReturnValue
{
	ZT1_OK = 0,

	/**
	 * Our identity collides with another on the network
	 *
	 * This is profoundly unlikely: once in about 2^39 identities. If this
	 * happens to you, delete identity.public and identity.secret from your
	 * data store / home path and restart. You might also avoid shark infested
	 * waters, hide during thunderstorms, and consider playing the lottery.
	 */
	ZT1_FATAL_ERROR_IDENTITY_COLLISION = 1,

	/**
	 * Ran out of memory
	 */
	ZT1_FATAL_ERROR_OUT_OF_MEMORY = 2,

	/**
	 * Home path is not writable
	 */
	ZT1_FATAL_ERROR_DATA_STORE_NOT_WRITABLE = 3
};

/**
 * Status codes
 */
enum ZT1_StatusCode
{
	/**
	 * Node is online
	 */
	ZT1_STATUS_ONLINE = 1,

	/**
	 * Node is offline -- nothing is reachable
	 */
	ZT1_STATUS_OFFLINE = 2,

	/**
	 * The desperation level has changed
	 *
	 * 'extra' will point to an int containing the new level.
	 */
	ZT1_STATUS_DESPERATION_CHANGE = 3
};

/**
 * A message to or from a physical address (e.g. IP or physical Ethernet)
 */
struct ZT1_WireMessage
{
	/**
	 * Remote socket address
	 */
	ZT_SOCKADDR_STORAGE remoteAddress;

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
	const void *packetData;

	/**
	 * Length of packet
	 */
	unsigned int packetLength;
};

/**
 * A message to or from a virtual LAN port
 */
struct ZT1_VirtualLanFrame
{
	/**
	 * ZeroTier network ID of virtual LAN port
	 */
	uint64_t networkId;

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
	const void *frameData;

	/**
	 * Ethernet frame length
	 */
	unsigned int frameLength;
};

/**
 * Virtual LAN configuration
 */
struct ZT1_VirtualPortConfig
{
	/**
	 * 64-bit ZeroTier network ID
	 */
	uint64_t networkId;

	/**
	 * Ethernet MAC (40 bits) that should be assigned to port
	 */
	uint64_t mac;

	/**
	 * Network configuration request status
	 */
	enum {
		/**
		 * Waiting for network configuration
		 */
		ZT1_VirtualPortConfig_STATUS_WAITING = 0,

		/**
		 * Configuration received and we are authorized
		 */
		ZT1_VirtualPortConfig_STATUS_AUTHORIZED = 1,

		/**
		 * Netconf master told us 'nope'
		 */
		ZT1_VirtualPortConfig_STATUS_ACCESS_DENIED = 2,

		/**
		 * Netconf master exists, but this virtual network does not
		 */
		ZT1_VirtualPortConfig_STATUS_NOT_FOUND = 3
	} status;

	/**
	 * Network type
	 */
	enum {
		/**
		 * Private networks are authorized via certificates of membership
		 */
		ZT1_VirtualPortConfig_NETWORK_TYPE_PRIVATE = 0,

		/**
		 * Public networks have no access control -- they'll always be AUTHORIZED
		 */
		ZT1_VirtualPortConfig_NETWORK_TYPE_PUBLIC = 1
	} type;

	/**
	 * Desired interface MTU
	 */
	unsigned int mtu;

	/**
	 * If nonzero, DHCP should be used if allowed by security constraints
	 */
	int dhcp;

	/**
	 * If nonzero, this port is allowed to bridge to other networks
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
	const ZT_SOCKADDR_STORAGE *assignedAddresses;

	/**
	 * Number of assigned addresses
	 */
	unsigned int assignedAddressCount;

	/**
	 * Network name (from network configuration master)
	 */
	const char *networkName;
};

/**
 * Opaque instance of ZeroTier One node
 */
typedef void ZT1_Node;

/**
 * Callback called to update virtual port configuration
 *
 * This can be called at any time to update the configuration of a virtual
 * network port. If a port is deleted (via leave() or otherwise) this is
 * called with a NULL config parameter.
 */
typedef void (*ZT1_VirtualPortConfigCallback)(uint64_t,const struct ZT1_VirtualPortConfig *);

/**
 * Callback for status messages
 *
 * Status messages indicate changes in network status, minor problems or
 * errors, and other events. The second parameter is a human-readable
 * detail message, and can be NULL. The third parameter is reserved for any
 * special structures that might be attached to certain message types, and
 * is usually NULL.
 */
typedef void (*ZT1_StatusCallback)(enum ZT1_StatusCode,const char *,const void *);

/**
 * Create a new ZeroTier One node
 *
 * Note that this can take a few seconds the first time it's called, as it
 * will generate an identity.
 *
 * @param node Result parameter: pointer to set to new node instance
 * @param homePath ZeroTier home path for storing state information
 * @param lanConfigUpdateCallback Function to be called when virtual LANs are created, deleted, or their config parameters change
 * @param statusCallback Function to receive status updates and non-fatal error notices
 * @return OK (0) or error code if a fatal error condition has occurred
 */
enum ZT1_ReturnValue ZT1_Node_new(
	ZT1_Node **node,
	const char *homePath,
	ZT1_VirtualLanConfigCallback *lanConfigUpdateCallback,
	ZT1_StatusCallback *statusCallback);

/**
 * Enable network configuration master services for this node
 *
 * The supplied instance must be a C++ object that inherits from the
 * NetworkConfigMaster base class in node/. No type checking is performed,
 * so a pointer to anything else will result in a crash.
 *
 * Normal nodes should not need to use this.
 *
 * @param node ZertTier One node
 * @param networkConfigMasterInstance Instance of NetworkConfigMaster C++ class or NULL to disable
 * @return OK (0) or error code if a fatal error condition has occurred
 */
enum ZT1_ReturnValue ZT1_Node_enableNetconfMaster(
	ZT1_Node *node,
	void *networkConfigMasterInstance);

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
enum ZT1_ReturnValue ZT1_Node_run(
	const struct ZT1_WireMessage *inputWireMessages,
	unsigned int inputWireMessageCount,
	const struct ZT1_VirtualLanFrame *inputLanFrames,
	unsigned int inputLanFrameCount,
	const struct ZT1_WireMessage **outputWireMessages,
	unsigned int *outputWireMessageCount,
	const struct ZT1_VirtualLanFrame **outputLanFrames,
	unsigned int *outputLanFrameCount,
	unsigned long *maxNextInterval);

/* ------------------------------------------------------------------------ */
/* Query result buffers                                                     */
/* ------------------------------------------------------------------------ */

/**
 * Node status result buffer
 */
struct ZT1_Node_Status
{
	/**
	 * Public identity in string form
	 */
	char publicIdentity[256];

	/**
	 * ZeroTier address in 10-digit hex form
	 */
	char address[16];

	/**
	 * ZeroTier address (in least significant 40 bits of 64-bit integer)
	 */
	uint64_t rawAddress;

	/**
	 * Number of known peers (including supernodes)
	 */
	unsigned int knownPeers;

	/**
	 * Number of upstream supernodes
	 */
	unsigned int supernodes;

	/**
	 * Number of peers with active direct links
	 */
	unsigned int directlyConnectedPeers;

	/**
	 * Number of peers that have recently communicated with us
	 */
	unsigned int alivePeers;

	/**
	 * Success rate at establishing direct links (0.0 to 1.0, approximate)
	 */
	float directLinkSuccessRate;

	/**
	 * True if connectivity appears good
	 */
	bool online;

	/**
	 * True if running; all other fields are technically undefined if this is false
	 */
	bool running;

	/**
	 * True if initialization is complete
	 */
	bool initialized;
};

/**
 * Physical address type
 */
enum ZT1_Node_PhysicalAddressType {
	ZT1_Node_PhysicalAddress_TYPE_NULL = 0,     /* none/invalid */
	ZT1_Node_PhysicalAddress_TYPE_IPV4 = 1,     /* 32-bit IPv4 address (and port) */
	ZT1_Node_PhysicalAddress_TYPE_IPV6 = 2,     /* 128-bit IPv6 address (and port) */
	ZT1_Node_PhysicalAddress_TYPE_ETHERNET = 3  /* 48-bit Ethernet MAC address */
};

/**
 * Physical address result buffer
 */
struct ZT1_Node_PhysicalAddress
{
	/**
	 * Physical address type
	 */
	enum ZT1_Node_PhysicalAddressType type;

	/**
	 * Address in raw binary form -- length depends on type
	 */
	unsigned char bits[16];

	/**
	 * Port or netmask bits (for IPV4 and IPV6)
	 */
	unsigned int port;

	/**
	 * Address in canonical human-readable form
	 */
	char ascii[64];

	/**
	 * Zone index identifier (thing after % on IPv6 link-local addresses only)
	 */
	char zoneIndex[16];
};

/**
 * Physical path type
 */
enum ZT1_Node_PhysicalPathType { /* These must be numerically the same as type in Path.hpp */
	ZT1_Node_PhysicalPath_TYPE_NULL = 0,     /* none/invalid */
	ZT1_Node_PhysicalPath_TYPE_UDP = 1,      /* UDP association */
	ZT1_Node_PhysicalPath_TYPE_TCP_OUT = 2,  /* outgoing TCP tunnel using pseudo-SSL */
	ZT1_Node_PhysicalPath_TYPE_TCP_IN = 3,   /* incoming TCP tunnel using pseudo-SSL */
	ZT1_Node_PhysicalPath_TYPE_ETHERNET = 4  /* raw ethernet frames over trusted backplane */
};

/**
 * Network path result buffer
 */
struct ZT1_Node_PhysicalPath
{
	/**
	 * Physical path type
	 */
	enum ZT1_Node_PhysicalPathType type;

	/**
	 * Physical address of endpoint
	 */
	struct ZT1_Node_PhysicalAddress address;

	/**
	 * Time since last send in milliseconds or -1 for never
	 */
	long lastSend;

	/**
	 * Time since last receive in milliseconds or -1 for never
	 */
	long lastReceive;

	/**
	 * Time since last ping in milliseconds or -1 for never
	 */
	long lastPing;

	/**
	 * Is path active/connected? Non-fixed active paths may be garbage collected over time.
	 */
	bool active;

	/**
	 * Is path fixed? (i.e. not learned, static)
	 */
	bool fixed;
};

/**
 * What trust hierarchy role does this device have?
 */
enum ZT1_Node_PeerRole {
	ZT1_Node_Peer_SUPERNODE = 0, // planetary supernode
	ZT1_Node_Peer_HUB = 1,       // locally federated hub (coming soon)
	ZT1_Node_Peer_NODE = 2       // ordinary node
};

/**
 * Peer status result buffer
 */
struct ZT1_Node_Peer
{
	/**
	 * Remote peer version: major.minor.revision (or empty if unknown)
	 */
	char remoteVersion[16];

	/**
	 * ZeroTier address of peer as 10-digit hex string
	 */
	char address[16];

	/**
	 * ZeroTier address in least significant 40 bits of 64-bit integer
	 */
	uint64_t rawAddress;

	/**
	 * Last measured latency in milliseconds or zero if unknown
	 */
	unsigned int latency;

	/**
	 * What trust hierarchy role does this device have?
	 */
	enum ZT1_Node_PeerRole role;

	/**
	 * Array of network paths to peer
	 */
	struct ZT1_Node_PhysicalPath *paths;

	/**
	 * Number of paths (size of paths[])
	 */
	unsigned int numPaths;
};

/**
 * List of peers
 */
struct ZT1_Node_PeerList
{
	struct ZT1_Node_Peer *peers;
	unsigned int numPeers;
};

/**
 * Network status code
 */
enum ZT1_Node_NetworkStatus {
	ZT1_Node_Network_INITIALIZING = 0,
	ZT1_Node_Network_WAITING_FOR_FIRST_AUTOCONF = 1,
	ZT1_Node_Network_OK = 2,
	ZT1_Node_Network_ACCESS_DENIED = 3,
	ZT1_Node_Network_NOT_FOUND = 4,
	ZT1_Node_Network_INITIALIZATION_FAILED = 5,
	ZT1_Node_Network_NO_MORE_DEVICES = 6
};

/**
 * Network status result buffer
 */
struct ZT1_Node_Network
{
	/**
	 * 64-bit network ID
	 */
	uint64_t nwid;

	/**
	 * 64-bit network ID in hex form
	 */
	char nwidHex[32];

	/**
	 * Short network name
	 */
	char name[256];

	/**
	 * Longer network description
	 */
	char description[4096];

	/**
	 * Device name (system-dependent)
	 */
	char device[256];

	/**
	 * Status code in string format
	 */
	char statusStr[64];

	/**
	 * Ethernet MAC address of this endpoint in string form
	 */
	char macStr[32];

	/**
	 * Ethernet MAC address of this endpoint on the network in raw binary form
	 */
	unsigned char mac[6];

	/**
	 * Age of configuration in milliseconds or -1 if never refreshed
	 */
	long configAge;

	/**
	 * Assigned layer-3 IPv4 and IPv6 addresses
	 *
	 * Note that PhysicalAddress also supports other address types, but this
	 * list will only list IP address assignments. The port field will contain
	 * the number of bits in the netmask -- e.g. 192.168.1.1/24.
	 */
	struct ZT1_Node_PhysicalAddress *ips;

	/**
	 * Number of layer-3 IPs (size of ips[])
	 */
	unsigned int numIps;

	/**
	 * Network status code
	 */
	enum ZT1_Node_NetworkStatus status;

	/**
	 * True if traffic on network is enabled
	 */
	bool enabled;

	/**
	 * Is this a private network? If false, network lacks access control.
	 */
	bool isPrivate;
};

/**
 * Return buffer for list of networks
 */
struct ZT1_Node_NetworkList
{
	struct ZT1_Node_Network *networks;
	unsigned int numNetworks;
};

#ifdef __cplusplus
}
#endif

#endif
