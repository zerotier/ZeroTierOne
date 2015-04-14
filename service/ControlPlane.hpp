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

#include "../include/ZeroTierOne.h"

#include <string>
#include <map>
#include <set>

namespace ZeroTier {

class Node;

/**
 * HTTP control plane and static web server
 */
class ControlPlane
{
public:
	ControlPlane(Node *n,const std::set<std::string> atoks);
	~ControlPlane();

	/**
	 * Handle HTTP request
	 *
	 * @param httpMethod HTTP method (as defined in ext/http-parser/http_parser.h)
	 * @param path Request path
	 * @param headers Request headers
	 * @param body Request body
	 * @param responseBody Result parameter: fill with response data
	 * @param responseContentType Result parameter: fill with content type
	 * @return HTTP response code
	 */
	unsigned int handleRequest(
		unsigned int httpMethod,
		const std::string &path,
		const std::map<std::string,std::string> &headers,
		const std::string &body,
		std::string &responseBody,
		std::string &responseContentType);

private:
	Node *const _node;
	std::set<std::string> _authTokens;
};

} // namespace ZeroTier

#endif
