/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2019  ZeroTier, Inc.  https://www.zerotier.com/
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

#ifndef ZT_HTTP_HPP
#define ZT_HTTP_HPP

#include <string>
#include <map>
#include <stdexcept>

#if defined(_WIN32) || defined(_WIN64)
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#else
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

namespace ZeroTier {

/**
 * Simple synchronous HTTP client used for updater and cli
 */
class Http
{
public:
	/**
	 * Make HTTP GET request
	 *
	 * The caller must set all headers, including Host.
	 *
	 * @return HTTP status code or 0 on error (responseBody will contain error message)
	 */
	static inline unsigned int GET(
		unsigned long maxResponseSize,
		unsigned long timeout,
		const struct sockaddr *remoteAddress,
		const char *path,
		const std::map<std::string,std::string> &requestHeaders,
		std::map<std::string,std::string> &responseHeaders,
		std::string &responseBody)
	{
		return _do(
			"GET",
			maxResponseSize,
			timeout,
			remoteAddress,
			path,
			requestHeaders,
			(const void *)0,
			0,
			responseHeaders,
			responseBody);
	}

	/**
	 * Make HTTP DELETE request
	 *
	 * The caller must set all headers, including Host.
	 *
	 * @return HTTP status code or 0 on error (responseBody will contain error message)
	 */
	static inline unsigned int DEL(
		unsigned long maxResponseSize,
		unsigned long timeout,
		const struct sockaddr *remoteAddress,
		const char *path,
		const std::map<std::string,std::string> &requestHeaders,
		std::map<std::string,std::string> &responseHeaders,
		std::string &responseBody)
	{
		return _do(
			"DELETE",
			maxResponseSize,
			timeout,
			remoteAddress,
			path,
			requestHeaders,
			(const void *)0,
			0,
			responseHeaders,
			responseBody);
	}

	/**
	 * Make HTTP POST request
	 *
	 * It is the responsibility of the caller to set all headers. With POST, the
	 * Content-Length and Content-Type headers must be set or the POST will not
	 * work.
	 *
	 * @return HTTP status code or 0 on error (responseBody will contain error message)
	 */
	static inline unsigned int POST(
		unsigned long maxResponseSize,
		unsigned long timeout,
		const struct sockaddr *remoteAddress,
		const char *path,
		const std::map<std::string,std::string> &requestHeaders,
		const void *postData,
		unsigned long postDataLength,
		std::map<std::string,std::string> &responseHeaders,
		std::string &responseBody)
	{
		return _do(
			"POST",
			maxResponseSize,
			timeout,
			remoteAddress,
			path,
			requestHeaders,
			postData,
			postDataLength,
			responseHeaders,
			responseBody);
	}

	/**
	 * Make HTTP PUT request
	 *
	 * It is the responsibility of the caller to set all headers. With PUT, the
	 * Content-Length and Content-Type headers must be set or the PUT will not
	 * work.
	 *
	 * @return HTTP status code or 0 on error (responseBody will contain error message)
	 */
	static inline unsigned int PUT(
		unsigned long maxResponseSize,
		unsigned long timeout,
		const struct sockaddr *remoteAddress,
		const char *path,
		const std::map<std::string,std::string> &requestHeaders,
		const void *postData,
		unsigned long postDataLength,
		std::map<std::string,std::string> &responseHeaders,
		std::string &responseBody)
	{
		return _do(
			"PUT",
			maxResponseSize,
			timeout,
			remoteAddress,
			path,
			requestHeaders,
			postData,
			postDataLength,
			responseHeaders,
			responseBody);
	}

private:
	static unsigned int _do(
		const char *method,
		unsigned long maxResponseSize,
		unsigned long timeout,
		const struct sockaddr *remoteAddress,
		const char *path,
		const std::map<std::string,std::string> &requestHeaders,
		const void *requestBody,
		unsigned long requestBodyLength,
		std::map<std::string,std::string> &responseHeaders,
		std::string &responseBody);
};

} // namespace ZeroTier

#endif
