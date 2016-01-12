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

#include "ControlPlane.hpp"
#include "OneService.hpp"

#include "../version.h"
#include "../include/ZeroTierOne.h"

#include "../ext/http-parser/http_parser.h"

#ifdef ZT_ENABLE_NETWORK_CONTROLLER
#include "../controller/SqliteNetworkController.hpp"
#endif

#include "../node/InetAddress.hpp"
#include "../node/Node.hpp"
#include "../node/Utils.hpp"
#include "../osdep/OSUtils.hpp"

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
			default:   buf.push_back(*p);  break;
		}
	}
	return buf;
}
static std::string _jsonEscape(const std::string &s) { return _jsonEscape(s.c_str()); }

static std::string _jsonEnumerate(const ZT_MulticastGroup *mg,unsigned int count)
{
	std::string buf;
	char tmp[128];
	buf.push_back('[');
	for(unsigned int i=0;i<count;++i) {
		if (i > 0)
			buf.push_back(',');
		Utils::snprintf(tmp,sizeof(tmp),"\"%.2x:%.2x:%.2x:%.2x:%.2x:%.2x\\/%.8lx\"",
			(unsigned int)((mg[i].mac >> 40) & 0xff),
			(unsigned int)((mg[i].mac >> 32) & 0xff),
			(unsigned int)((mg[i].mac >> 24) & 0xff),
			(unsigned int)((mg[i].mac >> 16) & 0xff),
			(unsigned int)((mg[i].mac >> 8) & 0xff),
			(unsigned int)(mg[i].mac & 0xff),
			(unsigned long)(mg[i].adi));
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
		buf.append(_jsonEscape(reinterpret_cast<const InetAddress *>(&(ss[i]))->toString()));
		buf.push_back('"');
	}
	buf.push_back(']');
	return buf;
}

static void _jsonAppend(unsigned int depth,std::string &buf,const ZT_VirtualNetworkConfig *nc,const std::string &portDeviceName)
{
	char json[4096];
	char prefix[32];

	if (depth >= sizeof(prefix)) // sanity check -- shouldn't be possible
		return;
	for(unsigned int i=0;i<depth;++i)
		prefix[i] = '\t';
	prefix[depth] = '\0';

	const char *nstatus = "",*ntype = "";
	switch(nc->status) {
		case ZT_NETWORK_STATUS_REQUESTING_CONFIGURATION: nstatus = "REQUESTING_CONFIGURATION"; break;
		case ZT_NETWORK_STATUS_OK:                       nstatus = "OK"; break;
		case ZT_NETWORK_STATUS_ACCESS_DENIED:            nstatus = "ACCESS_DENIED"; break;
		case ZT_NETWORK_STATUS_NOT_FOUND:                nstatus = "NOT_FOUND"; break;
		case ZT_NETWORK_STATUS_PORT_ERROR:               nstatus = "PORT_ERROR"; break;
		case ZT_NETWORK_STATUS_CLIENT_TOO_OLD:           nstatus = "CLIENT_TOO_OLD"; break;
	}
	switch(nc->type) {
		case ZT_NETWORK_TYPE_PRIVATE:                    ntype = "PRIVATE"; break;
		case ZT_NETWORK_TYPE_PUBLIC:                     ntype = "PUBLIC"; break;
	}

	Utils::snprintf(json,sizeof(json),
		"%s{\n"
		"%s\t\"nwid\": \"%.16llx\",\n"
		"%s\t\"mac\": \"%.2x:%.2x:%.2x:%.2x:%.2x:%.2x\",\n"
		"%s\t\"name\": \"%s\",\n"
		"%s\t\"status\": \"%s\",\n"
		"%s\t\"type\": \"%s\",\n"
		"%s\t\"mtu\": %u,\n"
		"%s\t\"dhcp\": %s,\n"
		"%s\t\"bridge\": %s,\n"
		"%s\t\"broadcastEnabled\": %s,\n"
		"%s\t\"portError\": %d,\n"
		"%s\t\"netconfRevision\": %lu,\n"
		"%s\t\"multicastSubscriptions\": %s,\n"
		"%s\t\"assignedAddresses\": %s,\n"
		"%s\t\"portDeviceName\": \"%s\"\n"
		"%s}",
		prefix,
		prefix,nc->nwid,
		prefix,(unsigned int)((nc->mac >> 40) & 0xff),(unsigned int)((nc->mac >> 32) & 0xff),(unsigned int)((nc->mac >> 24) & 0xff),(unsigned int)((nc->mac >> 16) & 0xff),(unsigned int)((nc->mac >> 8) & 0xff),(unsigned int)(nc->mac & 0xff),
		prefix,_jsonEscape(nc->name).c_str(),
		prefix,nstatus,
		prefix,ntype,
		prefix,nc->mtu,
		prefix,(nc->dhcp == 0) ? "false" : "true",
		prefix,(nc->bridge == 0) ? "false" : "true",
		prefix,(nc->broadcastEnabled == 0) ? "false" : "true",
		prefix,nc->portError,
		prefix,nc->netconfRevision,
		prefix,_jsonEnumerate(nc->multicastSubscriptions,nc->multicastSubscriptionCount).c_str(),
		prefix,_jsonEnumerate(nc->assignedAddresses,nc->assignedAddressCount).c_str(),
		prefix,_jsonEscape(portDeviceName).c_str(),
		prefix);
	buf.append(json);
}

static std::string _jsonEnumerate(unsigned int depth,const ZT_PeerPhysicalPath *pp,unsigned int count)
{
	char json[1024];
	char prefix[32];

	if (depth >= sizeof(prefix)) // sanity check -- shouldn't be possible
		return std::string();
	for(unsigned int i=0;i<depth;++i)
		prefix[i] = '\t';
	prefix[depth] = '\0';

	std::string buf;
	for(unsigned int i=0;i<count;++i) {
		if (i > 0)
			buf.push_back(',');
		Utils::snprintf(json,sizeof(json),
			"{\n"
			"%s\t\"address\": \"%s\",\n"
			"%s\t\"lastSend\": %llu,\n"
			"%s\t\"lastReceive\": %llu,\n"
			"%s\t\"active\": %s,\n"
			"%s\t\"preferred\": %s\n"
			"%s}",
			prefix,_jsonEscape(reinterpret_cast<const InetAddress *>(&(pp[i].address))->toString()).c_str(),
			prefix,pp[i].lastSend,
			prefix,pp[i].lastReceive,
			prefix,(pp[i].active == 0) ? "false" : "true",
			prefix,(pp[i].preferred == 0) ? "false" : "true",
			prefix);
		buf.append(json);
	}
	return buf;
}

static void _jsonAppend(unsigned int depth,std::string &buf,const ZT_Peer *peer)
{
	char json[1024];
	char prefix[32];

	if (depth >= sizeof(prefix)) // sanity check -- shouldn't be possible
		return;
	for(unsigned int i=0;i<depth;++i)
		prefix[i] = '\t';
	prefix[depth] = '\0';

	const char *prole = "";
	switch(peer->role) {
		case ZT_PEER_ROLE_LEAF:  prole = "LEAF"; break;
		case ZT_PEER_ROLE_RELAY: prole = "RELAY"; break;
		case ZT_PEER_ROLE_ROOT:  prole = "ROOT"; break;
	}

	Utils::snprintf(json,sizeof(json),
		"%s{\n"
		"%s\t\"address\": \"%.10llx\",\n"
		"%s\t\"lastUnicastFrame\": %llu,\n"
		"%s\t\"lastMulticastFrame\": %llu,\n"
		"%s\t\"versionMajor\": %d,\n"
		"%s\t\"versionMinor\": %d,\n"
		"%s\t\"versionRev\": %d,\n"
		"%s\t\"version\": \"%d.%d.%d\",\n"
		"%s\t\"latency\": %u,\n"
		"%s\t\"role\": \"%s\",\n"
		"%s\t\"paths\": [%s]\n"
		"%s}",
		prefix,
		prefix,peer->address,
		prefix,peer->lastUnicastFrame,
		prefix,peer->lastMulticastFrame,
		prefix,peer->versionMajor,
		prefix,peer->versionMinor,
		prefix,peer->versionRev,
		prefix,peer->versionMajor,peer->versionMinor,peer->versionRev,
		prefix,peer->latency,
		prefix,prole,
		prefix,_jsonEnumerate(depth+1,peer->paths,peer->pathCount).c_str(),
		prefix);
	buf.append(json);
}

ControlPlane::ControlPlane(OneService *svc,Node *n,const char *uiStaticPath) :
	_svc(svc),
	_node(n),
#ifdef ZT_ENABLE_NETWORK_CONTROLLER
	_controller((SqliteNetworkController *)0),
#endif
	_uiStaticPath((uiStaticPath) ? uiStaticPath : "")
{
}

ControlPlane::~ControlPlane()
{
}

unsigned int ControlPlane::handleRequest(
	const InetAddress &fromAddress,
	unsigned int httpMethod,
	const std::string &path,
	const std::map<std::string,std::string> &headers,
	const std::string &body,
	std::string &responseBody,
	std::string &responseContentType)
{
	char json[8194];
	unsigned int scode = 404;
	std::vector<std::string> ps(Utils::split(path.c_str(),"/","",""));
	std::map<std::string,std::string> urlArgs;
	Mutex::Lock _l(_lock);

	if (!((fromAddress.ipsEqual(InetAddress::LO4))||(fromAddress.ipsEqual(InetAddress::LO6))))
		return 403; // Forbidden: we only allow access from localhost right now

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
		ps.push_back(std::string("index.html"));
	}

	bool isAuth = false;
	{
		std::map<std::string,std::string>::const_iterator ah(headers.find("x-zt1-auth"));
		if ((ah != headers.end())&&(_authTokens.count(ah->second) > 0)) {
			isAuth = true;
		} else {
			ah = urlArgs.find("auth");
			if ((ah != urlArgs.end())&&(_authTokens.count(ah->second) > 0))
				isAuth = true;
		}
	}

	if (httpMethod == HTTP_GET) {

		std::string ext;
		std::size_t dotIdx = ps[0].find_last_of('.');
		if (dotIdx != std::string::npos)
			ext = ps[0].substr(dotIdx);

		if ((ps.size() == 1)&&(ext.length() >= 2)&&(ext[0] == '.')) {
			/* Static web pages can be served without authentication to enable a simple web
			 * UI. This is still only allowed from approved IP addresses. Anything with a
			 * dot in the first path element (e.g. foo.html) is considered a static page,
			 * as nothing in the API is so named. */

			if (_uiStaticPath.length() > 0) {
				if (ext == ".html")
					responseContentType = "text/html";
				else if (ext == ".js")
					responseContentType = "application/javascript";
				else if (ext == ".jsx")
					responseContentType = "text/jsx";
				else if (ext == ".json")
					responseContentType = "application/json";
				else if (ext == ".css")
					responseContentType = "text/css";
				else if (ext == ".png")
					responseContentType = "image/png";
				else if (ext == ".jpg")
					responseContentType = "image/jpeg";
				else if (ext == ".gif")
					responseContentType = "image/gif";
				else if (ext == ".txt")
					responseContentType = "text/plain";
				else if (ext == ".xml")
					responseContentType = "text/xml";
				else if (ext == ".svg")
					responseContentType = "image/svg+xml";
				else responseContentType = "application/octet-stream";
				scode = OSUtils::readFile((_uiStaticPath + ZT_PATH_SEPARATOR_S + ps[0]).c_str(),responseBody) ? 200 : 404;
			} else {
				scode = 404;
			}

		} else if (isAuth) {
			/* Things that require authentication -- a.k.a. everything but static web app pages. */

			if (ps[0] == "status") {
				responseContentType = "application/json";

				ZT_NodeStatus status;
				_node->status(&status);

				std::string clusterJson;
#ifdef ZT_ENABLE_CLUSTER
				{
					ZT_ClusterStatus cs;
					_node->clusterStatus(&cs);

					if (cs.clusterSize >= 1) {
						char t[1024];
						Utils::snprintf(t,sizeof(t),"{\n\t\t\"myId\": %u,\n\t\t\"clusterSize\": %u,\n\t\t\"members\": [",cs.myId,cs.clusterSize);
						clusterJson.append(t);
						for(unsigned int i=0;i<cs.clusterSize;++i) {
							Utils::snprintf(t,sizeof(t),"%s\t\t\t{\n\t\t\t\t\"id\": %u,\n\t\t\t\t\"msSinceLastHeartbeat\": %u,\n\t\t\t\t\"alive\": %s,\n\t\t\t\t\"x\": %d,\n\t\t\t\t\"y\": %d,\n\t\t\t\t\"z\": %d,\n\t\t\t\t\"load\": %llu,\n\t\t\t\t\"peers\": %llu\n\t\t\t}",
								((i == 0) ? "\n" : ",\n"),
								cs.members[i].id,
								cs.members[i].msSinceLastHeartbeat,
								(cs.members[i].alive != 0) ? "true" : "false",
								cs.members[i].x,
								cs.members[i].y,
								cs.members[i].z,
								cs.members[i].load,
								cs.members[i].peers);
							clusterJson.append(t);
						}
						clusterJson.append(" ]\n\t\t}");
					}
				}
#endif

				Utils::snprintf(json,sizeof(json),
					"{\n"
					"\t\"address\": \"%.10llx\",\n"
					"\t\"publicIdentity\": \"%s\",\n"
					"\t\"worldId\": %llu,\n"
					"\t\"worldTimestamp\": %llu,\n"
					"\t\"online\": %s,\n"
					"\t\"tcpFallbackActive\": %s,\n"
					"\t\"versionMajor\": %d,\n"
					"\t\"versionMinor\": %d,\n"
					"\t\"versionRev\": %d,\n"
					"\t\"version\": \"%d.%d.%d\",\n"
					"\t\"clock\": %llu,\n"
					"\t\"cluster\": %s\n"
					"}\n",
					status.address,
					status.publicIdentity,
					status.worldId,
					status.worldTimestamp,
					(status.online) ? "true" : "false",
					(_svc->tcpFallbackActive()) ? "true" : "false",
					ZEROTIER_ONE_VERSION_MAJOR,
					ZEROTIER_ONE_VERSION_MINOR,
					ZEROTIER_ONE_VERSION_REVISION,
					ZEROTIER_ONE_VERSION_MAJOR,ZEROTIER_ONE_VERSION_MINOR,ZEROTIER_ONE_VERSION_REVISION,
					(unsigned long long)OSUtils::now(),
					((clusterJson.length() > 0) ? clusterJson.c_str() : "null"));
				responseBody = json;
				scode = 200;
			} else if (ps[0] == "config") {
				responseContentType = "application/json";
				responseBody = "{}"; // TODO
				scode = 200;
			} else if (ps[0] == "network") {
				ZT_VirtualNetworkList *nws = _node->networks();
				if (nws) {
					if (ps.size() == 1) {
						// Return [array] of all networks
						responseContentType = "application/json";
						responseBody = "[\n";
						for(unsigned long i=0;i<nws->networkCount;++i) {
							if (i > 0)
								responseBody.append(",");
							_jsonAppend(1,responseBody,&(nws->networks[i]),_svc->portDeviceName(nws->networks[i].nwid));
						}
						responseBody.append("\n]\n");
						scode = 200;
					} else if (ps.size() == 2) {
						// Return a single network by ID or 404 if not found
						uint64_t wantnw = Utils::hexStrToU64(ps[1].c_str());
						for(unsigned long i=0;i<nws->networkCount;++i) {
							if (nws->networks[i].nwid == wantnw) {
								responseContentType = "application/json";
								_jsonAppend(0,responseBody,&(nws->networks[i]),_svc->portDeviceName(nws->networks[i].nwid));
								responseBody.push_back('\n');
								scode = 200;
								break;
							}
						}
					} // else 404
					_node->freeQueryResult((void *)nws);
				} else scode = 500;
			} else if (ps[0] == "peer") {
				ZT_PeerList *pl = _node->peers();
				if (pl) {
					if (ps.size() == 1) {
						// Return [array] of all peers
						responseContentType = "application/json";
						responseBody = "[\n";
						for(unsigned long i=0;i<pl->peerCount;++i) {
							if (i > 0)
								responseBody.append(",\n");
							_jsonAppend(1,responseBody,&(pl->peers[i]));
						}
						responseBody.append("\n]\n");
						scode = 200;
					} else if (ps.size() == 2) {
						// Return a single peer by ID or 404 if not found
						uint64_t wantp = Utils::hexStrToU64(ps[1].c_str());
						for(unsigned long i=0;i<pl->peerCount;++i) {
							if (pl->peers[i].address == wantp) {
								responseContentType = "application/json";
								_jsonAppend(0,responseBody,&(pl->peers[i]));
								responseBody.push_back('\n');
								scode = 200;
								break;
							}
						}
					} // else 404
					_node->freeQueryResult((void *)pl);
				} else scode = 500;
			} else if (ps[0] == "newIdentity") {
				// Return a newly generated ZeroTier identity -- this is primarily for debugging
				// and testing to make it easy for automated test scripts to generate test IDs.
				Identity newid;
				newid.generate();
				responseBody = newid.toString(true);
				responseContentType = "text/plain";
				scode = 200;
			} else {
#ifdef ZT_ENABLE_NETWORK_CONTROLLER
				if (_controller)
					scode = _controller->handleControlPlaneHttpGET(std::vector<std::string>(ps.begin()+1,ps.end()),urlArgs,headers,body,responseBody,responseContentType);
				else scode = 404;
#else
				scode = 404;
#endif
			}

		} else scode = 401; // isAuth == false

	} else if ((httpMethod == HTTP_POST)||(httpMethod == HTTP_PUT)) {

		if (isAuth) {

			if (ps[0] == "config") {
				// TODO
			} else if (ps[0] == "network") {
				if (ps.size() == 2) {
					uint64_t wantnw = Utils::hexStrToU64(ps[1].c_str());
					_node->join(wantnw,(void *)0); // does nothing if we are a member
					ZT_VirtualNetworkList *nws = _node->networks();
					if (nws) {
						for(unsigned long i=0;i<nws->networkCount;++i) {
							if (nws->networks[i].nwid == wantnw) {
								responseContentType = "application/json";
								_jsonAppend(0,responseBody,&(nws->networks[i]),_svc->portDeviceName(nws->networks[i].nwid));
								responseBody.push_back('\n');
								scode = 200;
								break;
							}
						}
						_node->freeQueryResult((void *)nws);
					} else scode = 500;
				}
			} else {
#ifdef ZT_ENABLE_NETWORK_CONTROLLER
				if (_controller)
					scode = _controller->handleControlPlaneHttpPOST(std::vector<std::string>(ps.begin()+1,ps.end()),urlArgs,headers,body,responseBody,responseContentType);
				else scode = 404;
#else
				scode = 404;
#endif
			}

		} else scode = 401; // isAuth == false

	} else if (httpMethod == HTTP_DELETE) {

		if (isAuth) {

			if (ps[0] == "config") {
				// TODO
			} else if (ps[0] == "network") {
				ZT_VirtualNetworkList *nws = _node->networks();
				if (nws) {
					if (ps.size() == 2) {
						uint64_t wantnw = Utils::hexStrToU64(ps[1].c_str());
						for(unsigned long i=0;i<nws->networkCount;++i) {
							if (nws->networks[i].nwid == wantnw) {
								_node->leave(wantnw,(void **)0);
								responseBody = "true";
								responseContentType = "application/json";
								scode = 200;
								break;
							}
						}
					} // else 404
					_node->freeQueryResult((void *)nws);
				} else scode = 500;
			} else {
#ifdef ZT_ENABLE_NETWORK_CONTROLLER
				if (_controller)
					scode = _controller->handleControlPlaneHttpDELETE(std::vector<std::string>(ps.begin()+1,ps.end()),urlArgs,headers,body,responseBody,responseContentType);
				else scode = 404;
#else
				scode = 404;
#endif
			}

		} else {
			scode = 401; // isAuth = false
		}

	} else {
		scode = 400;
		responseBody = "Method not supported.";
	}

	// Wrap result in jsonp function call if the user included a jsonp= url argument.
	// Also double-check isAuth since forbidding this without auth feels safer.
	std::map<std::string,std::string>::const_iterator jsonp(urlArgs.find("jsonp"));
	if ((isAuth)&&(jsonp != urlArgs.end())&&(responseContentType == "application/json")) {
		if (responseBody.length() > 0)
			responseBody = jsonp->second + "(" + responseBody + ");";
		else responseBody = jsonp->second + "(null);";
		responseContentType = "application/javascript";
	}

	return scode;
}

} // namespace ZeroTier
