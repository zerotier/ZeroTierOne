/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2011-2014  ZeroTier Networks LLC
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

#ifndef ZT_NODE_HPP
#define ZT_NODE_HPP

#include <stdint.h>

#include "../include/ZeroTierOne.h"

namespace ZeroTier {

class EthernetTapFactory;
class RoutingTable;
class SocketManager;

/**
 * A ZeroTier One node
 */
class Node
{
public:
	/**
	 * Returned by node main if/when it terminates
	 */
	enum ReasonForTermination
	{
		/**
		 * Node is currently in run()
		 */
		NODE_RUNNING = 0,

		/**
		 * Node is shutting down for normal reasons, including a signal
		 */
		NODE_NORMAL_TERMINATION = 1,

		/**
		 * An upgrade is available. Its path is in reasonForTermination().
		 */
		NODE_RESTART_FOR_UPGRADE = 2,

		/**
		 * A serious unrecoverable error has occurred.
		 */
		NODE_UNRECOVERABLE_ERROR = 3,

		/**
		 * An address collision occurred (typically this should cause re-invocation with resetIdentity set to true)
		 */
		NODE_ADDRESS_COLLISION = 4
	};

	/**
	 * Create a new node
	 *
	 * The node is not executed until run() is called. The supplied tap factory
	 * and routing table must not be freed until the node is no longer
	 * executing. Node does not delete these objects; the caller still owns
	 * them.
	 *
	 * @param hp Home directory path or NULL for system-wide default for this platform
	 * @param tf Ethernet tap factory for platform network stack
	 * @param rt Routing table interface for platform network stack
	 * @param sm Socket manager for physical network I/O
	 * @param resetIdentity If true, delete identity before starting and regenerate
	 * @param overrideRootTopology Override root topology with this dictionary (in string serialized format) and do not update (default: NULL for none)
	 */
	Node(
		const char *hp,
		EthernetTapFactory *tf,
		RoutingTable *rt,
		SocketManager *sm,
		bool resetIdentity,
		const char *overrideRootTopology = (const char *)0) throw();

	~Node();

	/**
	 * Execute node in current thread, return on shutdown
	 *
	 * @return Reason for termination
	 */
	ReasonForTermination run()
		throw();

	/**
	 * Obtain a human-readable reason for node termination
	 *
	 * @return Reason for node termination or NULL if run() has not returned
	 */
	const char *terminationMessage() const
		throw();

	/**
	 * Terminate this node, causing run() to return
	 *
	 * @param reason Reason for termination
	 * @param reasonText Text to be returned by terminationMessage()
	 */
	void terminate(ReasonForTermination reason,const char *reasonText)
		throw();

	/**
	 * Forget p2p links now and resynchronize with peers
	 *
	 * This can be used if the containing application knows its network environment has
	 * changed. ZeroTier itself tries to detect such changes, but is not always successful.
	 */
	void resync()
		throw();

	/**
	 * @return True if we appear to be online in some viable capacity
	 */
	bool online()
		throw();

	/**
	 * @return True if run() has been called
	 */
	bool started()
		throw();

	/**
	 * @return True if run() has not yet returned
	 */
	bool running()
		throw();

	/**
	 * @return True if initialization phase of startup is complete
	 */
	bool initialized()
		throw();

	/**
	 * @return This node's address (in least significant 40 bits of 64-bit int) or 0 if not yet initialized
	 */
	uint64_t address()
		throw();

	/**
	 * Join a network
	 *
	 * Use getNetworkStatus() to check the network's status after joining. If you
	 * are already a member of the network, this does nothing.
	 *
	 * @param nwid 64-bit network ID
	 */
	void join(uint64_t nwid)
		throw();

	/**
	 * Leave a network
	 *
	 * @param nwid 64-bit network ID
	 */
	void leave(uint64_t nwid)
		throw();

	/**
	 * Get the status of this node
	 *
	 * @param status Buffer to fill with status information
	 */
	void status(ZT1_Node_Status *status)
		throw();

	/**
	 * @return List of known peers or NULL on failure
	 */
	ZT1_Node_PeerList *listPeers()
		throw();

	/**
	 * @param nwid 64-bit network ID
	 * @return Network status or NULL if we are not a member of this network
	 */
	ZT1_Node_Network *getNetworkStatus(uint64_t nwid)
		throw();

	/**
	 * @return List of networks we've joined or NULL on failure
	 */
	ZT1_Node_NetworkList *listNetworks()
		throw();

	/**
	 * Free a query result buffer
	 *
	 * Use this to free the return values of listNetworks(), listPeers(), etc.
	 *
	 * @param qr Query result buffer
	 */
	void freeQueryResult(void *qr)
		throw();

	/**
	 * Check for software updates (if enabled) (updates will eventually get factored out of node/)
	 */
	bool updateCheck()
		throw();

	/**
	 * Inject a packet into a network's tap as if it came from the host
	 *
	 * This is primarily for debugging, and at the moment is only supported on
	 * the test/dummy Ethernet tap implementation. Attempting to use it for real
	 * devices will fail and return 'false.'
	 *
	 * @param nwid Network ID
	 * @param from Source MAC address (must be 6 bytes in length)
	 * @param to Destination MAC address (must be 6 bytes in length)
	 * @param etherType Ethernet frame type
	 * @param data Frame data
	 * @param len Length of frame in bytes
	 * @return True on success; false if not a member of network, injection not supported, or data too large
	 */
	bool injectPacketFromHost(uint64_t nwid,const unsigned char *from,const unsigned char *to,unsigned int etherType,const void *data,unsigned int len);

	static const char *versionString() throw();
	static unsigned int versionMajor() throw();
	static unsigned int versionMinor() throw();
	static unsigned int versionRevision() throw();

private:
	// Nodes are not copyable
	Node(const Node&);
	const Node& operator=(const Node&);

	void *const _impl; // private implementation
};

} // namespace ZeroTier

#endif
