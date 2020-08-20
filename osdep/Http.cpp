/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "Http.hpp"
#include "Phy.hpp"
#include "OSUtils.hpp"
#include "../node/Constants.hpp"
#include "../node/Utils.hpp"

#ifdef ZT_USE_SYSTEM_HTTP_PARSER
#include <http_parser.h>
#else
#include "../ext/http-parser/http_parser.h"
#endif

namespace ZeroTier {

namespace {

static int ShttpOnMessageBegin(http_parser *parser);
static int ShttpOnUrl(http_parser *parser,const char *ptr,size_t length);
#if (HTTP_PARSER_VERSION_MAJOR >= 2) && (HTTP_PARSER_VERSION_MINOR >= 2)
static int ShttpOnStatus(http_parser *parser,const char *ptr,size_t length);
#else
static int ShttpOnStatus(http_parser *parser);
#endif
static int ShttpOnHeaderField(http_parser *parser,const char *ptr,size_t length);
static int ShttpOnValue(http_parser *parser,const char *ptr,size_t length);
static int ShttpOnHeadersComplete(http_parser *parser);
static int ShttpOnBody(http_parser *parser,const char *ptr,size_t length);
static int ShttpOnMessageComplete(http_parser *parser);

#if (HTTP_PARSER_VERSION_MAJOR >= 2) && (HTTP_PARSER_VERSION_MINOR >= 1)
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
#else
static const struct http_parser_settings HTTP_PARSER_SETTINGS = {
	ShttpOnMessageBegin,
	ShttpOnUrl,
	ShttpOnHeaderField,
	ShttpOnValue,
	ShttpOnHeadersComplete,
	ShttpOnBody,
	ShttpOnMessageComplete
};
#endif

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
		if (writePtr < (unsigned long)writeBuf.length()) {
			long n = phy->streamSend(sock,writeBuf.data() + writePtr,(unsigned long)writeBuf.length() - writePtr,true);
			if (n > 0)
				writePtr += n;
		}
		if (writePtr >= (unsigned long)writeBuf.length())
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
	std::string writeBuf;

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
#if (HTTP_PARSER_VERSION_MAJOR >= 2) && (HTTP_PARSER_VERSION_MINOR >= 2)
static int ShttpOnStatus(http_parser *parser,const char *ptr,size_t length)
#else
static int ShttpOnStatus(http_parser *parser)
#endif
{
	/*
	HttpPhyHandler *hh = reinterpret_cast<HttpPhyHandler *>(parser->data);
	hh->messageSize += (unsigned long)length;
	if (hh->messageSize > hh->maxResponseSize)
		return -1;
	*/
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
			char tmp[1024];
			OSUtils::ztsnprintf(tmp,sizeof(tmp),"%s %s HTTP/1.1\r\n",method,path);
			handler.writeBuf.append(tmp);
			for(std::map<std::string,std::string>::const_iterator h(requestHeaders.begin());h!=requestHeaders.end();++h) {
				OSUtils::ztsnprintf(tmp,sizeof(tmp),"%s: %s\r\n",h->first.c_str(),h->second.c_str());
				handler.writeBuf.append(tmp);
			}
			handler.writeBuf.append("\r\n");
			if ((requestBody)&&(requestBodyLength))
				handler.writeBuf.append((const char *)requestBody,requestBodyLength);
		} catch ( ... ) {
			responseBody = "request too large";
			return 0;
		}

		if (maxResponseSize) {
			handler.maxResponseSize = maxResponseSize;
		} else {
			handler.maxResponseSize = 2147483647;
		}
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
