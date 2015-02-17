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

#ifndef ZT_NODECONTROLCLIENT_HPP
#define ZT_NODECONTROLCLIENT_HPP

#include <string>
#include <vector>

#include "../node/Constants.hpp"

#ifdef __WINDOWS__
#define ZT_IPC_ENDPOINT_BASE "\\\\.\\pipe\\ZeroTierOne-"
#else
#define ZT_IPC_ENDPOINT_BASE "/tmp/.ZeroTierOne-"
#endif

namespace ZeroTier {

/**
 * Client for controlling a local ZeroTier One node
 */
class NodeControlClient
{
public:
	/**
	 * Create a new node config client
	 *
	 * Initialization may fail. Call error() to check.
	 *
	 * @param ep Endpoint to connect to (OS-dependent)
	 * @param resultHandler Function to call when commands provide results
	 * @param arg First argument to result handler
	 */
	NodeControlClient(const char *ep,const char *authToken,void (*resultHandler)(void *,const char *),void *arg)
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
	 * @return Default path for current user's authtoken.secret or ~/.zeroTierOneAuthToken (location is platform-dependent)
	 */
	static const char *authTokenDefaultUserPath();

	/**
	 * Load (or generate) the authentication token
	 *
	 * @param path Full path to authtoken.secret
	 * @param generateIfNotFound If true, generate and save if not found or readable (requires appropriate privileges, returns empty on failure)
	 * @return Authentication token or empty string on failure
	 */
	static std::string getAuthToken(const char *path,bool generateIfNotFound);

private:
	// NodeControlClient is not copyable
	NodeControlClient(const NodeControlClient&);
	const NodeControlClient& operator=(const NodeControlClient&);

	void *_impl;
};

} // namespace ZeroTier

#endif
