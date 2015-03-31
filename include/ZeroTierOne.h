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

/* ------------------------------------------------------------------------ */
/* ZeroTier One C API                                                       */
/* ------------------------------------------------------------------------ */

/* coming soon... */

#endif
