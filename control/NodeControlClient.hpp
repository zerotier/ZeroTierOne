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

#include <string>
#include <vector>

#ifndef ZT_NODECONTROLCLIENT_HPP
#define ZT_NODECONTROLCLIENT_HPP

namespace ZeroTier {

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
	 * @param hp Home path of ZeroTier One instance or NULL for default system home path
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

} // namespace ZeroTier

#endif
