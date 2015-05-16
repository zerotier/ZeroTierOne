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

#ifndef ZT_ONE_CONTROLPLANE_HPP
#define ZT_ONE_CONTROLPLANE_HPP

#include <string>
#include <map>
#include <set>

#include "../include/ZeroTierOne.h"

#include "../node/Mutex.hpp"

namespace ZeroTier {

class OneService;
class Node;
class SqliteNetworkController;
struct InetAddress;

/**
 * HTTP control plane and static web server
 */
class ControlPlane
{
public:
	ControlPlane(OneService *svc,Node *n,const char *uiStaticPath);
	~ControlPlane();

	/**
	 * Add an authentication token for API access
	 */
	inline void addAuthToken(const char *tok)
	{
		Mutex::Lock _l(_lock);
		_authTokens.insert(std::string(tok));
	}

	/**
	 * Mount a subsystem under a prefix
	 *
	 * Note that the prefix must not contain a dot -- this is reserved for
	 * static pages -- and must not be a reserved prefix such as /peer
	 * or /network. Do not include path / characters in the prefix. Example
	 * would be 'controller' for SqliteNetworkController.
	 *
	 * @param prefix First element in URI path
	 * @param subsys Object to call for results of GET and POST/PUT operations
	 */
	inline void mount(const char *prefix,SqliteNetworkController *subsys)
	{
		Mutex::Lock _l(_lock);
		_subsystems[std::string(prefix)] = subsys;
	}

	/**
	 * Handle HTTP request
	 *
	 * @param fromAddress Originating IP address of request
	 * @param httpMethod HTTP method (as defined in ext/http-parser/http_parser.h)
	 * @param path Request path
	 * @param headers Request headers
	 * @param body Request body
	 * @param responseBody Result parameter: fill with response data
	 * @param responseContentType Result parameter: fill with content type
	 * @return HTTP response code
	 */
	unsigned int handleRequest(
		const InetAddress &fromAddress,
		unsigned int httpMethod,
		const std::string &path,
		const std::map<std::string,std::string> &headers,
		const std::string &body,
		std::string &responseBody,
		std::string &responseContentType);

private:
	OneService *const _svc;
	Node *const _node;
	std::string _uiStaticPath;
	std::set<std::string> _authTokens;
	std::map<std::string,SqliteNetworkController *> _subsystems;
	Mutex _lock;
};

} // namespace ZeroTier

#endif
