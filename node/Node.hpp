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

#include <string>
#include <vector>

namespace ZeroTier {

/**
 * A ZeroTier One node
 *
 * This class hides all its implementation details and all other classes in
 * preparation for ZeroTier One being made available in library form for
 * embedding in mobile apps.
 */
class Node
{
public:
	/**
	 * Client for controlling a local ZeroTier One node
	 *
	 * Windows note: be sure you call WSAStartup() before using this,
	 * otherwise it will be unable to open a local UDP socket to
	 * communicate with the service.
	 */
	class NodeControlClient
	{
	public:
		/**
		 * Create a new node config client
		 *
		 * Initialization may fail. Call error() to check.
		 *
		 * @param hp Home path of ZeroTier One instance
		 * @param resultHandler Function to call when commands provide results
		 * @param arg First argument to result handler
		 * @param authToken Authentication token or NULL (default) to read from authtoken.secret in home path
		 */
		NodeControlClient(const char *hp,void (*resultHandler)(void *,const char *),void *arg,const char *authToken = (const char *)0)
			throw();

		~NodeControlClient();

		/**
		 * @return Initialization error or NULL if none
		 */
		const char *error() const
			throw();

		/**
		 * Send a command to the local node
		 *
		 * Note that the returned conversation ID will never be 0. A return value
		 * of 0 indicates a fatal error such as failure to bind to any local UDP
		 * port.
		 *
		 * @param command
		 * @return Conversation ID that will be provided to result handler when/if results are sent back
		 */
		void send(const char *command)
			throw();
		inline void send(const std::string &command)
			throw() { return send(command.c_str()); }

		/**
		 * Split a line of results
		 *
		 * @param line Line to split
		 * @return Vector of fields
		 */
		static std::vector<std::string> splitLine(const char *line);
		static inline std::vector<std::string> splitLine(const std::string &line) { return splitLine(line.c_str()); }

		/**
		 * @return Default path for current user's authtoken.secret
		 */
		static const char *authTokenDefaultUserPath();

		/**
		 * @return Default path to system authtoken.secret
		 */
		static const char *authTokenDefaultSystemPath();

	private:
		// NodeControlClient is not copyable
		NodeControlClient(const NodeControlClient&);
		const NodeControlClient& operator=(const NodeControlClient&);

		void *_impl;
	};

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
	 * The node is not executed until run() is called.
	 *
	 * @param hp Home directory path or NULL for system-wide default for this platform (default: NULL)
	 * @param udpPort UDP port or 0 for default (9993) (default: 0)
	 * @param tcpPort TCP port or 0 for default (9993) (default: 0)
	 * @param resetIdentity If true, delete identity before starting and regenerate (default: false)
	 */
	Node(const char *hp = (const char *)0,unsigned int udpPort = 0,unsigned int tcpPort = 0,bool resetIdentity = false)
		throw();

	~Node();

	/**
	 * Execute node in current thread
	 *
	 * This does not return until the node shuts down. Shutdown may be caused
	 * by an internally detected condition such as a new upgrade being
	 * available or a fatal error, or it may be signaled externally using
	 * the terminate() method.
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
	const char *reasonForTermination() const
		throw();

	/**
	 * Cause run() to return
	 *
	 * This can be called from a signal handler or another thread to signal a
	 * running node to shut down. Shutdown may take a few seconds, so run()
	 * may not return instantly. Multiple calls are ignored.
	 *
	 * @param reason Reason for termination
	 * @param reasonText Text to be returned by reasonForTermination()
	 */
	void terminate(ReasonForTermination reason,const char *reasonText)
		throw();

	/**
	 * Forget p2p links now and resynchronize with peers
	 */
	void resync()
		throw();

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
