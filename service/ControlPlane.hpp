/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2016  ZeroTier, Inc.  https://www.zerotier.com/
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
class EmbeddedNetworkController;
struct InetAddress;

/**
 * HTTP control plane and static web server
 */
class ControlPlane
{
public:
	ControlPlane(OneService *svc,Node *n);

	/**
	 * Set controller, which will be available under /controller
	 *
	 * @param c Network controller instance
	 */
	inline void setController(EmbeddedNetworkController *c)
	{
		Mutex::Lock _l(_lock);
		_controller = c;
	}

	/**
	 * Add an authentication token for API access
	 */
	inline void addAuthToken(const char *tok)
	{
		Mutex::Lock _l(_lock);
		_authTokens.insert(std::string(tok));
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
	EmbeddedNetworkController *_controller;
	std::set<std::string> _authTokens;
	Mutex _lock;
};

} // namespace ZeroTier

#endif
