/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2012-2013  ZeroTier Networks LLC
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

#ifndef _ZT_HTTP_HPP
#define _ZT_HTTP_HPP

#include <map>
#include <string>
#include <stdexcept>
#include "Thread.hpp"

#include "../ext/http-parser/http_parser.h"

namespace ZeroTier {

class Http
{
public:
	/**
	 * HTTP request methods
	 */
	enum Method
	{
		HTTP_METHOD_GET,
		HTTP_METHOD_HEAD
	};

	/**
	 * An empty headers map for convenience
	 */
	static const std::map<std::string,std::string> EMPTY_HEADERS;

	/**
	 * HTTP request
	 */
	class Request : protected Thread
	{
	public:
		/**
		 * Create and issue an HTTP request
		 *
		 * The supplied handler is called when the request is
		 * complete or if an error occurs. A code of zero indicates
		 * that the server could not be reached, and a description
		 * of the error will be in 'body'. If the handler returns
		 * false the Request object deletes itself. Otherwise the
		 * object must be deleted by other code.
		 *
		 * @param m Request method
		 * @param url Destination URL
		 * @param rh Request headers
		 * @param rb Request body or empty string for none (currently unused)
		 * @param handler Request handler function
		 * @param arg First argument to request handler
		 */
		Request(
			Http::Method m,
			const std::string &url,
			const std::map<std::string,std::string> &rh,
			const std::string &rb,
			bool (*handler)(Request *,void *,const std::string &,int,const std::map<std::string,std::string> &,const std::string &),
			void *arg);

		/**
		 * Destruction cancels any in-progress request
		 */
		virtual ~Request();

	protected:
		virtual void main()
			throw();

	private:
		// HTTP parser handlers
		static int _http_on_message_begin(http_parser *parser);
		static int _http_on_url(http_parser *parser,const char *data,size_t length);
		static int _http_on_status_complete(http_parser *parser);
		static int _http_on_header_field(http_parser *parser,const char *data,size_t length);
		static int _http_on_header_value(http_parser *parser,const char *data,size_t length);
		static int _http_on_headers_complete(http_parser *parser);
		static int _http_on_body(http_parser *parser,const char *data,size_t length);
		static int _http_on_message_complete(http_parser *parser);

		http_parser _parser;
		std::string _url;

		std::map<std::string,std::string> _requestHeaders;
		std::map<std::string,std::string> _responseHeaders;

		std::string _currentHeaderField;
		std::string _responseBody;

		bool (*_handler)(Request *,void *,const std::string &,int,const std::map<std::string,std::string> &,const std::string &);
		void *_arg;

		Http::Method _method;
		int _responseStatusCode;
		bool _messageComplete;
		volatile int _fd;
	};
};

} // namespace ZeroTier

#endif
