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

#include "ControlPlane.hpp"

#include "../version.h"
#include "../include/ZeroTierOne.h"

#include "../ext/http-parser/http_parser.h"

#include "../node/InetAddress.hpp"
#include "../node/Node.hpp"
#include "../node/Utils.hpp"

namespace ZeroTier {

static std::string _jsonEscape(const char *s)
{
	std::string buf;
	for(const char *p=s;(*p);++p) {
		switch(*p) {
			case '\t': buf.append("\\t");  break;
			case '\b': buf.append("\\b");  break;
			case '\r': buf.append("\\r");  break;
			case '\n': buf.append("\\n");  break;
			case '\f': buf.append("\\f");  break;
			case '"':  buf.append("\\\""); break;
			case '\\': buf.append("\\\\"); break;
			case '/':  buf.append("\\/");  break;
			default:   buf.push_back(*s);  break;
		}
	}
	return buf;
}
static std::string _jsonEscape(const std::string &s) { return _jsonEscape(s.c_str()); }
static std::string _jsonEnumerate(const ZT1_MulticastGroup *mg,unsigned int count)
{
	std::string buf;
	char tmp[128];
	buf.push_back('[');
	for(unsigned int i=0;i<count;++i) {
		if (i > 0)
			buf.push_back(',');
		Utils::snprintf(tmp,sizeof(tmp),"\"%.2x:%.2x:%.2x:%.2x:%.2x:%.2x\\/%.8lx\"",
			(unsigned int)((mg->mac >> 40) & 0xff),
			(unsigned int)((mg->mac >> 32) & 0xff),
			(unsigned int)((mg->mac >> 24) & 0xff),
			(unsigned int)((mg->mac >> 16) & 0xff),
			(unsigned int)((mg->mac >> 8) & 0xff),
			(unsigned int)(mg->mac & 0xff),
			mg->adi);
		buf.append(tmp);
	}
	buf.push_back(']');
	return buf;
}
static std::string _jsonEnumerate(const struct sockaddr_storage *ss,unsigned int count)
{
	std::string buf;
	buf.push_back('[');
	for(unsigned int i=0;i<count;++i) {
		if (i > 0)
			buf.push_back(',');
		buf.push_back('"');
		buf.append(_jsonEscape(reinterpret_cast<const InetAddress *>(ss)->toString()));
		buf.push_back('"');
	}
	buf.push_back(']');
	return buf;
}
static void _jsonAppend(std::string &buf,const ZT1_VirtualNetworkConfig *nc)
{
	char json[65536];
	const char *nstatus = "",*ntype = "";
	switch(nc->status) {
		case ZT1_NETWORK_STATUS_REQUESTING_CONFIGURATION: nstatus = "REQUESTING_CONFIGURATION"; break;
		case ZT1_NETWORK_STATUS_OK:                       nstatus = "OK"; break;
		case ZT1_NETWORK_STATUS_ACCESS_DENIED:            nstatus = "ACCESS_DENIED"; break;
		case ZT1_NETWORK_STATUS_NOT_FOUND:                nstatus = "NOT_FOUND"; break;
		case ZT1_NETWORK_STATUS_PORT_ERROR:               nstatus = "PORT_ERROR"; break;
		case ZT1_NETWORK_STATUS_CLIENT_TOO_OLD:           nstatus = "CLIENT_TOO_OLD"; break;
	}
	switch(nc->type) {
		case ZT1_NETWORK_TYPE_PRIVATE:                    ntype = "PRIVATE"; break;
		case ZT1_NETWORK_TYPE_PUBLIC:                     ntype = "PUBLIC"; break;
	}
	Utils::snprintf(json,sizeof(json),
		"{"
		"\"nwid\": \"%.16llx\","
		"\"mac\": \"%.2x:%.2x:%.2x:%.2x:%.2x:%.2x\","
		"\"name\": %s,"
		"\"status\": \"%s\","
		"\"type\": \"%s\","
		"\"mtu\": %u,"
		"\"dhcp\": %s,"
		"\"bridge\": %s,"
		"\"broadcastEnabled\": %s,"
		"\"portError\": %d,"
		"\"netconfRevision\": %lu,"
		"\"multicastSubscriptions\": %s,"
		"\"assignedAddresses\": %s"
		"}",
		nc->nwid,
		(unsigned int)((nc->mac >> 40) & 0xff),(unsigned int)((nc->mac >> 32) & 0xff),(unsigned int)((nc->mac >> 24) & 0xff),(unsigned int)((nc->mac >> 16) & 0xff),(unsigned int)((nc->mac >> 8) & 0xff),(unsigned int)(nc->mac & 0xff),
		_jsonEscape(nc->name).c_str(),
		nstatus,
		ntype,
		nc->mtu,
		(nc->dhcp == 0) ? "false" : "true",
		(nc->bridge == 0) ? "false" : "true",
		(nc->broadcastEnabled == 0) ? "false" : "true",
		nc->portError,
		nc->netconfRevision,
		_jsonEnumerate(nc->multicastSubscriptions,nc->multicastSubscriptionCount).c_str(),
		_jsonEnumerate(nc->assignedAddresses,nc->assignedAddressCount).c_str());
	buf.append(json);
}
static void _jsonAppend(std::string &buf,const ZT1_Peer *peer)
{
}

ControlPlane::ControlPlane(Node *n,const std::set<std::string> atoks) :
	_node(n),
	_authTokens(atoks)
{
}

ControlPlane::~ControlPlane()
{
}

unsigned int ControlPlane::handleRequest(
	unsigned int httpMethod,
	const std::string &path,
	const std::map<std::string,std::string> &headers,
	const std::string &body,
	std::string &responseBody,
	std::string &responseContentType)
{
	char json[65536];
	unsigned int scode = 404;
	std::vector<std::string> ps(Utils::split(path.c_str(),"/","",""));
	std::map<std::string,std::string> urlArgs;

	/* Note: this is kind of restricted in what it'll take. It does not support
	 * URL encoding, and /'s in URL args will screw it up. But the only URL args
	 * it really uses in ?jsonp=funcionName, and otherwise it just takes simple
	 * paths to simply-named resources. */

	if (ps.size() > 0) {
		std::size_t qpos = ps[ps.size() - 1].find('?');
		if (qpos != std::string::npos) {
			std::string args(ps[ps.size() - 1].substr(qpos + 1));
			ps[ps.size() - 1] = ps[ps.size() - 1].substr(0,qpos);
			std::vector<std::string> asplit(Utils::split(args.c_str(),"&","",""));
			for(std::vector<std::string>::iterator a(asplit.begin());a!=asplit.end();++a) {
				std::size_t eqpos = a->find('=');
				if (eqpos == std::string::npos)
					urlArgs[*a] = "";
				else urlArgs[a->substr(0,eqpos)] = a->substr(eqpos + 1);
			}
		}
	} else {
		ps.push_back(std::string("index"));
	}

	if (httpMethod == HTTP_GET) {
		if (ps[0] == "index") {
			responseContentType = "text/html";
			responseBody = "<html><body>Hello World!</body></html>";
		} else if (ps[0] == "status") {
			responseContentType = "application/json";
			ZT1_NodeStatus status;
			_node->status(&status);
			Utils::snprintf(json,sizeof(json),
				"{"
				"\"address\":\"%.10llx\","
				"\"publicIdentity\":\"%s\","
				"\"online\":%s,"
				"\"versionMajor\":%d,"
				"\"versionMinor\":%d,"
				"\"versionRevision\":%d,"
				"\"version\":\"%d.%d.%d\""
				"}",
				status.address,
				status.publicIdentity,
				(status.online) ? "true" : "false",
				ZEROTIER_ONE_VERSION_MAJOR,
				ZEROTIER_ONE_VERSION_MINOR,
				ZEROTIER_ONE_VERSION_REVISION,
				ZEROTIER_ONE_VERSION_MAJOR,ZEROTIER_ONE_VERSION_MINOR,ZEROTIER_ONE_VERSION_REVISION);
			responseBody = json;
		} else if (ps[0] == "config") {
			responseContentType = "application/json";
			responseBody = "{}"; // TODO
		} else if (ps[0] == "network") {
			ZT1_VirtualNetworkList *nws = _node->networks();
			if (nws) {
				if (ps.size() == 1) {
					// Return [array] of all networks
					responseContentType = "application/json";
					responseBody = "[";
					for(unsigned long i=0;i<nws->networkCount;++i) {
						if (i > 0)
							responseBody.push_back(',');
						_jsonAppend(responseBody,&(nws->networks[i]));
					}
					responseBody.push_back(']');
				} else if (ps.size() == 2) {
					// Return a single network by ID or 404 if not found
					uint64_t wantnw = Utils::hexStrToU64(ps[1].c_str());
					bool got = false;
					for(unsigned long i=0;i<nws->networkCount;++i) {
						if (nws->networks[i].nwid == wantnw) {
							got = true;
							responseContentType = "application/json";
							_jsonAppend(responseBody,&(nws->networks[i]));
							break;
						}
					}
					if (!got)
						scode = 404;
				} else {
					// Not sure what they want here, 404
					scode = 404;
				}
				_node->freeQueryResult((void *)nws);
			} else scode = 500;
		} else if (ps[0] == "peer") {
		}
	} else if ((httpMethod == HTTP_POST)||(httpMethod == HTTP_PUT)) { // PUT is weird in REST but we'll take it anyway
	} else if (httpMethod == HTTP_DELETE) {
	} else {
		scode = 400;
		responseBody = "Method not supported for resource ";
		responseBody.append(path);
	}

	std::map<std::string,std::string>::const_iterator jsonp(urlArgs.find("jsonp"));
	if (jsonp != urlArgs.end()) {
		if (responseBody.length() > 0)
			responseBody = jsonp->second + "(" + responseBody + ");";
		else responseBody = jsonp->second + "(null);";
		responseContentType = "application/javascript";
	}

	return scode;
}

} // namespace ZeroTier
