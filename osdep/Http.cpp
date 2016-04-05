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

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "Http.hpp"
#include "Phy.hpp"
#include "OSUtils.hpp"
#include "../node/Constants.hpp"
#include "../node/Utils.hpp"
#include "../ext/http-parser/http_parser.h"

namespace ZeroTier {

namespace {

static int ShttpOnMessageBegin(http_parser *parser);
static int ShttpOnUrl(http_parser *parser,const char *ptr,size_t length);
static int ShttpOnStatus(http_parser *parser,const char *ptr,size_t length);
static int ShttpOnHeaderField(http_parser *parser,const char *ptr,size_t length);
static int ShttpOnValue(http_parser *parser,const char *ptr,size_t length);
static int ShttpOnHeadersComplete(http_parser *parser);
static int ShttpOnBody(http_parser *parser,const char *ptr,size_t length);
static int ShttpOnMessageComplete(http_parser *parser);
static const struct http_parser_settings HTTP_PARSER_SETTINGS = {
	ShttpOnMessageBegin,
	ShttpOnUrl,
	ShttpOnStatus,
	ShttpOnHeaderField,
	ShttpOnValue,
	ShttpOnHeadersComplete,
	ShttpOnBody,
	ShttpOnMessageComplete
};

struct HttpPhyHandler
{
	// not used
	inline void phyOnDatagram(PhySocket *sock,void **uptr,const struct sockaddr *localAddr,const struct sockaddr *from,void *data,unsigned long len) {}
	inline void phyOnTcpAccept(PhySocket *sockL,PhySocket *sockN,void **uptrL,void **uptrN,const struct sockaddr *from) {}

	inline void phyOnTcpConnect(PhySocket *sock,void **uptr,bool success)
	{
		if (success) {
			phy->setNotifyWritable(sock,true);
		} else {
			*responseBody = "connection failed";
			error = true;
			done = true;
		}
	}

	inline void phyOnTcpClose(PhySocket *sock,void **uptr)
	{
		done = true;
	}

	inline void phyOnTcpData(PhySocket *sock,void **uptr,void *data,unsigned long len)
	{
		lastActivity = OSUtils::now();
		http_parser_execute(&parser,&HTTP_PARSER_SETTINGS,(const char *)data,len);
		if ((parser.upgrade)||(parser.http_errno != HPE_OK))
			phy->close(sock);
	}

	inline void phyOnTcpWritable(PhySocket *sock,void **uptr)
	{
		if (writePtr < writeSize) {
			long n = phy->streamSend(sock,writeBuf + writePtr,writeSize - writePtr,true);
			if (n > 0)
				writePtr += n;
		}
		if (writePtr >= writeSize)
			phy->setNotifyWritable(sock,false);
	}

	inline void phyOnFileDescriptorActivity(PhySocket *sock,void **uptr,bool readable,bool writable) {}
#ifdef __UNIX_LIKE__
	inline void phyOnUnixAccept(PhySocket *sockL,PhySocket *sockN,void **uptrL,void **uptrN) {}
	inline void phyOnUnixClose(PhySocket *sock,void **uptr) {}
	inline void phyOnUnixData(PhySocket *sock,void **uptr,void *data,unsigned long len) {}
	inline void phyOnUnixWritable(PhySocket *sock,void **uptr) {}
#endif // __UNIX_LIKE__

	http_parser parser;
	std::string currentHeaderField;
	std::string currentHeaderValue;
	unsigned long messageSize;
	unsigned long writePtr;
	uint64_t lastActivity;
	unsigned long writeSize;
	char writeBuf[32768];

	unsigned long maxResponseSize;
	std::map<std::string,std::string> *responseHeaders;
	std::string *responseBody;
	bool error;
	bool done;

	Phy<HttpPhyHandler *> *phy;
	PhySocket *sock;
};

static int ShttpOnMessageBegin(http_parser *parser)
{
	return 0;
}
static int ShttpOnUrl(http_parser *parser,const char *ptr,size_t length)
{
	return 0;
}
static int ShttpOnStatus(http_parser *parser,const char *ptr,size_t length)
{
	HttpPhyHandler *hh = reinterpret_cast<HttpPhyHandler *>(parser->data);
	hh->messageSize += (unsigned long)length;
	if (hh->messageSize > hh->maxResponseSize)
		return -1;
	return 0;
}
static int ShttpOnHeaderField(http_parser *parser,const char *ptr,size_t length)
{
	HttpPhyHandler *hh = reinterpret_cast<HttpPhyHandler *>(parser->data);
	hh->messageSize += (unsigned long)length;
	if (hh->messageSize > hh->maxResponseSize)
		return -1;
	if ((hh->currentHeaderField.length())&&(hh->currentHeaderValue.length())) {
		(*hh->responseHeaders)[hh->currentHeaderField] = hh->currentHeaderValue;
		hh->currentHeaderField = "";
		hh->currentHeaderValue = "";
	}
	for(size_t i=0;i<length;++i)
		hh->currentHeaderField.push_back(OSUtils::toLower(ptr[i]));
	return 0;
}
static int ShttpOnValue(http_parser *parser,const char *ptr,size_t length)
{
	HttpPhyHandler *hh = reinterpret_cast<HttpPhyHandler *>(parser->data);
	hh->messageSize += (unsigned long)length;
	if (hh->messageSize > hh->maxResponseSize)
		return -1;
	hh->currentHeaderValue.append(ptr,length);
	return 0;
}
static int ShttpOnHeadersComplete(http_parser *parser)
{
	HttpPhyHandler *hh = reinterpret_cast<HttpPhyHandler *>(parser->data);
	if ((hh->currentHeaderField.length())&&(hh->currentHeaderValue.length()))
		(*hh->responseHeaders)[hh->currentHeaderField] = hh->currentHeaderValue;
	return 0;
}
static int ShttpOnBody(http_parser *parser,const char *ptr,size_t length)
{
	HttpPhyHandler *hh = reinterpret_cast<HttpPhyHandler *>(parser->data);
	hh->messageSize += (unsigned long)length;
	if (hh->messageSize > hh->maxResponseSize)
		return -1;
	hh->responseBody->append(ptr,length);
	return 0;
}
static int ShttpOnMessageComplete(http_parser *parser)
{
	HttpPhyHandler *hh = reinterpret_cast<HttpPhyHandler *>(parser->data);
	hh->phy->close(hh->sock);
	return 0;
}

} // anonymous namespace

unsigned int Http::_do(
	const char *method,
	unsigned long maxResponseSize,
	unsigned long timeout,
	const struct sockaddr *remoteAddress,
	const char *path,
	const std::map<std::string,std::string> &requestHeaders,
	const void *requestBody,
	unsigned long requestBodyLength,
	std::map<std::string,std::string> &responseHeaders,
	std::string &responseBody)
{
	try {
		responseHeaders.clear();
		responseBody = "";

		HttpPhyHandler handler;

		http_parser_init(&(handler.parser),HTTP_RESPONSE);
		handler.parser.data = (void *)&handler;
		handler.messageSize = 0;
		handler.writePtr = 0;
		handler.lastActivity = OSUtils::now();

		try {
			handler.writeSize = Utils::snprintf(handler.writeBuf,sizeof(handler.writeBuf),"%s %s HTTP/1.1\r\n",method,path);
			for(std::map<std::string,std::string>::const_iterator h(requestHeaders.begin());h!=requestHeaders.end();++h)
				handler.writeSize += Utils::snprintf(handler.writeBuf + handler.writeSize,sizeof(handler.writeBuf) - handler.writeSize,"%s: %s\r\n",h->first.c_str(),h->second.c_str());
			handler.writeSize += Utils::snprintf(handler.writeBuf + handler.writeSize,sizeof(handler.writeBuf) - handler.writeSize,"\r\n");
			if ((requestBody)&&(requestBodyLength)) {
				if ((handler.writeSize + requestBodyLength) > sizeof(handler.writeBuf)) {
					responseBody = "request too large";
					return 0;
				}
				memcpy(handler.writeBuf + handler.writeSize,requestBody,requestBodyLength);
				handler.writeSize += requestBodyLength;
			}
		} catch ( ... ) {
			responseBody = "request too large";
			return 0;
		}

		handler.maxResponseSize = maxResponseSize;
		handler.responseHeaders = &responseHeaders;
		handler.responseBody = &responseBody;
		handler.error = false;
		handler.done = false;

		Phy<HttpPhyHandler *> phy(&handler,true,true);

		bool instantConnect = false;
		handler.phy = &phy;
		handler.sock = phy.tcpConnect((const struct sockaddr *)remoteAddress,instantConnect,(void *)0,true);
		if (!handler.sock) {
			responseBody = "connection failed (2)";
			return 0;
		}

		while (!handler.done) {
			phy.poll(timeout / 2);
			if ((timeout)&&((unsigned long)(OSUtils::now() - handler.lastActivity) > timeout)) {
				phy.close(handler.sock);
				responseBody = "timed out";
				return 0;
			}
		}

		return ((handler.error) ? 0 : ((handler.parser.http_errno != HPE_OK) ? 0 : handler.parser.status_code));
	} catch (std::exception &exc) {
		responseBody = exc.what();
		return 0;
	} catch ( ... ) {
		responseBody = "unknown exception";
		return 0;
	}
}

} // namespace ZeroTier
