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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <vector>
#include <set>
#include <list>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "Http.hpp"
#include "Utils.hpp"
#include "InetAddress.hpp"

static http_parser_settings _http_parser_settings;

namespace ZeroTier {

static bool _sendAll(int fd,const void *buf,unsigned int len)
{
	for(;;) {
		int n = (int)::send(fd,buf,len,0);
		if ((n < 0)&&(errno == EINTR))
			continue;
		return (n == (int)len);
	}
}

const std::map<std::string,std::string> Http::EMPTY_HEADERS;

Http::Request::Request(
	Http::Method m,
	const std::string &url,
	const std::map<std::string,std::string> &rh,
	const std::string &rb,
	bool (*handler)(Request *,void *,const std::string &,int,const std::map<std::string,std::string> &,const std::string &),
	void *arg) :
	_url(url),
	_requestHeaders(rh),
	_handler(handler),
	_arg(arg),
	_method(m),
	_fd(0)
{
	_http_parser_settings.on_message_begin = &Http::Request::_http_on_message_begin;
	_http_parser_settings.on_url = &Http::Request::_http_on_url;
	_http_parser_settings.on_status_complete = &Http::Request::_http_on_status_complete;
	_http_parser_settings.on_header_field = &Http::Request::_http_on_header_field;
	_http_parser_settings.on_header_value = &Http::Request::_http_on_header_value;
	_http_parser_settings.on_headers_complete = &Http::Request::_http_on_headers_complete;
	_http_parser_settings.on_body = &Http::Request::_http_on_body;
	_http_parser_settings.on_message_complete = &Http::Request::_http_on_message_complete;

	start();
}

Http::Request::~Request()
{
	if (_fd > 0)
		::close(_fd);
	join();
}

void Http::Request::main()
	throw()
{
	char buf[131072];

	try {
		http_parser_init(&_parser,HTTP_RESPONSE);
		_parser.data = this;

		http_parser_url urlParsed;
		if (http_parser_parse_url(_url.c_str(),_url.length(),0,&urlParsed)) {
			suicidalThread = !_handler(this,_arg,_url,0,_responseHeaders,"URL parse error");
			return;
		}
		if (!(urlParsed.field_set & (1 << UF_SCHEMA))) {
			suicidalThread = !_handler(this,_arg,_url,0,_responseHeaders,"URL specifies no schema");
			return;
		}

		std::string schema(_url.substr(urlParsed.field_data[UF_SCHEMA].off,urlParsed.field_data[UF_SCHEMA].len));

		if (schema == "file") {
			const std::string filePath(_url.substr(urlParsed.field_data[UF_PATH].off,urlParsed.field_data[UF_PATH].len));

			uint64_t lm = Utils::getLastModified(filePath.c_str());
			if (lm) {
				const std::map<std::string,std::string>::const_iterator ifModSince(_requestHeaders.find("If-Modified-Since"));
				if ((ifModSince != _requestHeaders.end())&&(ifModSince->second.length())) {
					uint64_t t64 = Utils::fromRfc1123(ifModSince->second);
					if ((t64)&&(lm > t64)) {
						suicidalThread = !_handler(this,_arg,_url,304,_responseHeaders,"");
						return;
					}
				}

				if (Utils::readFile(filePath.c_str(),_responseBody)) {
					_responseHeaders["Last-Modified"] = Utils::toRfc1123(lm);
					suicidalThread = !_handler(this,_arg,_url,200,_responseHeaders,_responseBody);
					return;
				}
			}

			suicidalThread = !_handler(this,_arg,_url,404,_responseHeaders,"file not found or not readable");
			return;
		} else if (schema == "http") {
			if (!(urlParsed.field_set & (1 << UF_HOST))) {
				suicidalThread = !_handler(this,_arg,_url,0,_responseHeaders,"URL contains no host");
				return;
			}
			std::string host(_url.substr(urlParsed.field_data[UF_HOST].off,urlParsed.field_data[UF_HOST].len));

			std::list<InetAddress> v4,v6;
			{
				struct addrinfo *res = (struct addrinfo *)0;
				if (!getaddrinfo(host.c_str(),(const char *)0,(const struct addrinfo *)0,&res)) {
					struct addrinfo *p = res;
					do {
						if (p->ai_family == AF_INET)
							v4.push_back(InetAddress(p->ai_addr));
						else if (p->ai_family == AF_INET6)
							v6.push_back(InetAddress(p->ai_addr));
					} while ((p = p->ai_next));
					freeaddrinfo(res);
				}
			}

			std::list<InetAddress> *addrList;
			if (v4.empty()&&v6.empty()) {
				suicidalThread = !_handler(this,_arg,_url,0,_responseHeaders,"could not find address for host in URL");
				return;
			} else if (v4.empty()) {
				addrList = &v6;
			} else {
				addrList = &v4;
			}
			InetAddress *addr;
			{
				addrList->sort();
				addrList->unique();
				unsigned int i = 0,k = 0;
				k = rand() % addrList->size();
				std::list<InetAddress>::iterator a(addrList->begin());
				while (i++ != k) ++a;
				addr = &(*a);
			}

			int remotePort = ((urlParsed.field_set & (1 << UF_PORT))&&(urlParsed.port)) ? (int)urlParsed.port : (int)80;
			if ((remotePort <= 0)||(remotePort > 0xffff)) {
				suicidalThread = !_handler(this,_arg,_url,0,_responseHeaders,"URL port out of range");
				return;
			}
			addr->setPort(remotePort);

			_fd = socket(addr->isV6() ? AF_INET6 : AF_INET,SOCK_STREAM,0);
			if (_fd <= 0) {
				suicidalThread = !_handler(this,_arg,_url,0,_responseHeaders,"could not open socket");
				return;
			}

			for(;;) {
				if (connect(_fd,addr->saddr(),addr->saddrLen())) {
					if (errno == EINTR)
						continue;
					::close(_fd); _fd = 0;
					suicidalThread = !_handler(this,_arg,_url,0,_responseHeaders,"connection failed to remote host");
					return;
				} else break;
			}

			const char *mstr = "GET";
			switch(_method) {
				case HTTP_METHOD_HEAD: mstr = "HEAD"; break;
				default: break;
			}
			int mlen = (int)snprintf(buf,sizeof(buf),"%s %s HTTP/1.1\r\nAccept-Encoding: \r\nHost: %s\r\n",mstr,_url.substr(urlParsed.field_data[UF_PATH].off,urlParsed.field_data[UF_PATH].len).c_str(),host.c_str());
			if (mlen >= (int)sizeof(buf)) {
				::close(_fd); _fd = 0;
				suicidalThread = !_handler(this,_arg,_url,0,_responseHeaders,"URL too long");
				return;
			}
			if (!_sendAll(_fd,buf,mlen)) {
				::close(_fd); _fd = 0;
				suicidalThread = !_handler(this,_arg,_url,0,_responseHeaders,"write error");
				return;
			}

			for(std::map<std::string,std::string>::const_iterator rh(_requestHeaders.begin());rh!=_requestHeaders.end();++rh) {
				mlen = (int)snprintf(buf,sizeof(buf),"%s: %s\r\n",rh->first.c_str(),rh->second.c_str());
				if (mlen >= (int)sizeof(buf)) {
					::close(_fd); _fd = 0;
					suicidalThread = !_handler(this,_arg,_url,0,_responseHeaders,"header too long");
					return;
				}
				if (!_sendAll(_fd,buf,mlen)) {
					::close(_fd); _fd = 0;
					suicidalThread = !_handler(this,_arg,_url,0,_responseHeaders,"write error");
					return;
				}
			}

			if (!_sendAll(_fd,"\r\n",2)) {
				::close(_fd); _fd = 0;
				suicidalThread = !_handler(this,_arg,_url,0,_responseHeaders,"write error");
				return;
			}

			_responseStatusCode = 0;
			_messageComplete = false;
			for(;;) {
				mlen = (int)::recv(_fd,buf,sizeof(buf),0);
				if (mlen < 0) {
					if (errno != EINTR)
						break;
					else continue;
				}
				if (((int)http_parser_execute(&_parser,&_http_parser_settings,buf,mlen) != mlen)||(_parser.upgrade)) {
					::close(_fd); _fd = 0;
					suicidalThread = !_handler(this,_arg,_url,0,_responseHeaders,"invalid HTTP response from server");
					return;
				}
				if (_messageComplete) {
					::close(_fd); _fd = 0;
					suicidalThread = !_handler(this,_arg,_url,_responseStatusCode,_responseHeaders,_responseBody);
					return;
				}
			}

			::close(_fd); _fd = 0;
			suicidalThread = !_handler(this,_arg,_url,0,_responseHeaders,"empty HTTP response from server");
			return;
		} else {
			suicidalThread = !_handler(this,_arg,_url,0,_responseHeaders,"only 'file' and 'http' methods are supported");
			return;
		}
	} catch ( ... ) {
		suicidalThread = !_handler(this,_arg,_url,0,_responseHeaders,"unexpected exception retrieving URL");
		return;
	}
}

int Http::Request::_http_on_message_begin(http_parser *parser)
{
	return 0;
}
int Http::Request::_http_on_url(http_parser *parser,const char *data,size_t length)
{
	return 0;
}
int Http::Request::_http_on_status_complete(http_parser *parser)
{
	Http::Request *r = (Http::Request *)parser->data;
	r->_responseStatusCode = parser->status_code;
	return 0;
}
int Http::Request::_http_on_header_field(http_parser *parser,const char *data,size_t length)
{
	Http::Request *r = (Http::Request *)parser->data;
	if ((r->_currentHeaderField.length())&&(r->_responseHeaders.find(r->_currentHeaderField) != r->_responseHeaders.end()))
		r->_currentHeaderField.assign("");
	r->_currentHeaderField.append(data,length);
	return 0;
}
int Http::Request::_http_on_header_value(http_parser *parser,const char *data,size_t length)
{
	Http::Request *r = (Http::Request *)parser->data;
	if (r->_currentHeaderField.length())
		r->_responseHeaders[r->_currentHeaderField].append(data,length);
	return 0;
}
int Http::Request::_http_on_headers_complete(http_parser *parser)
{
	Http::Request *r = (Http::Request *)parser->data;
	return ((r->_method == Http::HTTP_METHOD_HEAD) ? 1 : 0);
}
int Http::Request::_http_on_body(http_parser *parser,const char *data,size_t length)
{
	Http::Request *r = (Http::Request *)parser->data;
	r->_responseBody.append(data,length);
	return 0;
}
int Http::Request::_http_on_message_complete(http_parser *parser)
{
	Http::Request *r = (Http::Request *)parser->data;
	r->_messageComplete = true;
	return 0;
}

} // namespace ZeroTier
