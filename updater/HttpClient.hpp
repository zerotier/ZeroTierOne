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

#ifndef ZT_HTTPCLIENT_HPP
#define ZT_HTTPCLIENT_HPP

#include <string>
#include <map>
#include <set>

#include "../node/Mutex.hpp"

namespace ZeroTier {

class HttpClient_Private_Request;

/**
 * HTTP client that does queries in the background
 *
 * The handler method takes the following arguments: an arbitrary pointer, an
 * HTTP response code, the URL queried, whether or not the message body was
 * stored on disk, and the message body.
 *
 * If stored on disk, the body string contains the path and the file must be
 * moved or deleted by the receiver when it's done. If an error occurs, the
 * response code will be negative and the body will be the error message.
 *
 * All headers in the returned headers map will have their header names
 * converted to lower case, e.g. "content-type".
 *
 * Currently only the "http" transport is guaranteed to be supported on all
 * platforms.
 */
class HttpClient
{
public:
	friend class HttpClient_Private_Request;
	typedef void * Request;

	HttpClient();
	~HttpClient();

	/**
	 * Empty map for convenience use
	 */
	static const std::map<std::string,std::string> NO_HEADERS;

	/**
	 * Request a URL using the GET method
	 */
	inline Request GET(
		const std::string &url,
		const std::map<std::string,std::string> &headers,
		unsigned int timeout,
		void (*handler)(void *,int,const std::string &,const std::string &),
		void *arg)
	{
		return _do("GET",url,headers,timeout,handler,arg);
	}

	/**
	 * Cancel a request
	 *
	 * If the request is not active, this does nothing. This may take some time
	 * depending on HTTP implementation. It may also not kill instantly, but
	 * it will prevent the handler function from ever being called and cause the
	 * request to die silently when complete.
	 */
	void cancel(Request req);

private:
	Request _do(
		const char *method,
		const std::string &url,
		const std::map<std::string,std::string> &headers,
		unsigned int timeout,
		void (*handler)(void *,int,const std::string &,const std::string &),
		void *arg);

	std::set<Request> _requests;
	Mutex _requests_m;
};

} // namespace ZeroTier

#endif
