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

#ifdef ZT_USE_SYSTEM_HTTP_PARSER
#include <http_parser.h>
#else
#include "../ext/http-parser/http_parser.h"
#endif

#include "../ext/json/json.hpp"

#include "../controller/EmbeddedNetworkController.hpp"

#include "../node/InetAddress.hpp"
#include "../node/Node.hpp"
#include "../node/Utils.hpp"
#include "../node/World.hpp"

#include "../osdep/OSUtils.hpp"

namespace ZeroTier {

namespace {

static void _networkToJson(nlohmann::json &nj,const ZT_VirtualNetworkConfig *nc,const std::string &portDeviceName,const OneService::NetworkSettings &localSettings)
{
	char tmp[256];

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

	Utils::snprintf(tmp,sizeof(tmp),"%.16llx",nc->nwid);
	nj["id"] = tmp;
	nj["nwid"] = tmp;
	Utils::snprintf(tmp,sizeof(tmp),"%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",(unsigned int)((nc->mac >> 40) & 0xff),(unsigned int)((nc->mac >> 32) & 0xff),(unsigned int)((nc->mac >> 24) & 0xff),(unsigned int)((nc->mac >> 16) & 0xff),(unsigned int)((nc->mac >> 8) & 0xff),(unsigned int)(nc->mac & 0xff));
	nj["mac"] = tmp;
	nj["name"] = nc->name;
	nj["status"] = nstatus;
	nj["type"] = ntype;
	nj["mtu"] = nc->mtu;
	nj["dhcp"] = (bool)(nc->dhcp != 0);
	nj["bridge"] = (bool)(nc->bridge != 0);
	nj["broadcastEnabled"] = (bool)(nc->broadcastEnabled != 0);
	nj["portError"] = nc->portError;
	nj["netconfRevision"] = nc->netconfRevision;
	nj["portDeviceName"] = portDeviceName;
	nj["allowManaged"] = localSettings.allowManaged;
	nj["allowGlobal"] = localSettings.allowGlobal;
	nj["allowDefault"] = localSettings.allowDefault;

	nlohmann::json aa = nlohmann::json::array();
	for(unsigned int i=0;i<nc->assignedAddressCount;++i) {
		aa.push_back(reinterpret_cast<const InetAddress *>(&(nc->assignedAddresses[i]))->toString());
	}
	nj["assignedAddresses"] = aa;

	nlohmann::json ra = nlohmann::json::array();
	for(unsigned int i=0;i<nc->routeCount;++i) {
		nlohmann::json rj;
		rj["target"] = reinterpret_cast<const InetAddress *>(&(nc->routes[i].target))->toString();
		if (nc->routes[i].via.ss_family == nc->routes[i].target.ss_family)
			rj["via"] = reinterpret_cast<const InetAddress *>(&(nc->routes[i].via))->toIpString();
		else rj["via"] = nlohmann::json();
		rj["flags"] = (int)nc->routes[i].flags;
		rj["metric"] = (int)nc->routes[i].metric;
		ra.push_back(rj);
	}
	nj["routes"] = ra;
}

static void _peerToJson(nlohmann::json &pj,const ZT_Peer *peer)
{
	char tmp[256];

	const char *prole = "";
	switch(peer->role) {
		case ZT_PEER_ROLE_LEAF: prole = "LEAF"; break;
		case ZT_PEER_ROLE_MOON: prole = "MOON"; break;
		case ZT_PEER_ROLE_PLANET: prole = "PLANET"; break;
	}

	Utils::snprintf(tmp,sizeof(tmp),"%.10llx",peer->address);
	pj["address"] = tmp;
	pj["versionMajor"] = peer->versionMajor;
	pj["versionMinor"] = peer->versionMinor;
	pj["versionRev"] = peer->versionRev;
	Utils::snprintf(tmp,sizeof(tmp),"%d.%d.%d",peer->versionMajor,peer->versionMinor,peer->versionRev);
	pj["version"] = tmp;
	pj["latency"] = peer->latency;
	pj["role"] = prole;

	nlohmann::json pa = nlohmann::json::array();
	for(unsigned int i=0;i<peer->pathCount;++i) {
		nlohmann::json j;
		j["address"] = reinterpret_cast<const InetAddress *>(&(peer->paths[i].address))->toString();
		j["lastSend"] = peer->paths[i].lastSend;
		j["lastReceive"] = peer->paths[i].lastReceive;
		j["trustedPathId"] = peer->paths[i].trustedPathId;
		j["linkQuality"] = (double)peer->paths[i].linkQuality / (double)ZT_PATH_LINK_QUALITY_MAX;
		j["active"] = (bool)(peer->paths[i].expired == 0);
		j["expired"] = (bool)(peer->paths[i].expired != 0);
		j["preferred"] = (bool)(peer->paths[i].preferred != 0);
		pa.push_back(j);
	}
	pj["paths"] = pa;
}

static void _moonToJson(nlohmann::json &mj,const World &world)
{
	char tmp[64];
	Utils::snprintf(tmp,sizeof(tmp),"%.16llx",world.id());
	mj["id"] = tmp;
	mj["timestamp"] = world.timestamp();
	mj["signature"] = Utils::hex(world.signature().data,(unsigned int)world.signature().size());
	mj["updatesMustBeSignedBy"] = Utils::hex(world.updatesMustBeSignedBy().data,(unsigned int)world.updatesMustBeSignedBy().size());
	nlohmann::json ra = nlohmann::json::array();
	for(std::vector<World::Root>::const_iterator r(world.roots().begin());r!=world.roots().end();++r) {
		nlohmann::json rj;
		rj["identity"] = r->identity.toString(false);
		nlohmann::json eps = nlohmann::json::array();
		for(std::vector<InetAddress>::const_iterator a(r->stableEndpoints.begin());a!=r->stableEndpoints.end();++a)
			eps.push_back(a->toString());
		rj["stableEndpoints"] = eps;
		ra.push_back(rj);
	}
	mj["roots"] = ra;
	mj["waiting"] = false;
}

} // anonymous namespace

ControlPlane::ControlPlane(OneService *svc,Node *n) :
	_svc(svc),
	_node(n),
	_controller((EmbeddedNetworkController *)0)
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
	char tmp[256];
	unsigned int scode = 404;
	nlohmann::json res;
	std::vector<std::string> ps(OSUtils::split(path.c_str(),"/","",""));
	std::map<std::string,std::string> urlArgs;

	Mutex::Lock _l(_lock);

	/* Note: this is kind of restricted in what it'll take. It does not support
	 * URL encoding, and /'s in URL args will screw it up. But the only URL args
	 * it really uses in ?jsonp=funcionName, and otherwise it just takes simple
	 * paths to simply-named resources. */
	if (ps.size() > 0) {
		std::size_t qpos = ps[ps.size() - 1].find('?');
		if (qpos != std::string::npos) {
			std::string args(ps[ps.size() - 1].substr(qpos + 1));
			ps[ps.size() - 1] = ps[ps.size() - 1].substr(0,qpos);
			std::vector<std::string> asplit(OSUtils::split(args.c_str(),"&","",""));
			for(std::vector<std::string>::iterator a(asplit.begin());a!=asplit.end();++a) {
				std::size_t eqpos = a->find('=');
				if (eqpos == std::string::npos)
					urlArgs[*a] = "";
				else urlArgs[a->substr(0,eqpos)] = a->substr(eqpos + 1);
			}
		}
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

#ifdef __SYNOLOGY__
	#include <stdlib.h>
	// Authenticate via Synology's built-in cgi script
	if (!isAuth) {
		/*
		fprintf(stderr, "path = %s\n", path.c_str());
		fprintf(stderr, "headers.size=%d\n", headers.size());
		std::map<std::string, std::string>::const_iterator it(headers.begin());		
		while(it != headers.end()) {	
			fprintf(stderr,"header[%s] = %s\n", (it->first).c_str(), (it->second).c_str());
			it++;
		}
		*/
		// parse out url args
		int synotoken_pos = path.find("SynoToken");
		int argpos = path.find("?");
		if(synotoken_pos != std::string::npos && argpos != std::string::npos) {	
			std::string cookie = path.substr(argpos+1, synotoken_pos-(argpos+1));
			std::string synotoken = path.substr(synotoken_pos);
			std::string cookie_val = cookie.substr(cookie.find("=")+1);
			std::string synotoken_val = synotoken.substr(synotoken.find("=")+1);
			// Set necessary env for auth script
			std::map<std::string,std::string>::const_iterator ah2(headers.find("x-forwarded-for"));
			setenv("HTTP_COOKIE", cookie_val.c_str(), true);
			setenv("HTTP_X_SYNO_TOKEN", synotoken_val.c_str(), true);
			setenv("REMOTE_ADDR", ah2->second.c_str(),true);
	    	//fprintf(stderr, "HTTP_COOKIE: %s\n",std::getenv ("HTTP_COOKIE"));
	    	//fprintf(stderr, "HTTP_X_SYNO_TOKEN: %s\n",std::getenv ("HTTP_X_SYNO_TOKEN"));
	    	//fprintf(stderr, "REMOTE_ADDR: %s\n",std::getenv ("REMOTE_ADDR"));
			// check synology web auth
			char user[256], buf[1024];
			FILE *fp = NULL;
			bzero(user, 256);
			fp = popen("/usr/syno/synoman/webman/modules/authenticate.cgi", "r");
			if(!fp)
				isAuth = false;
			else {
				bzero(buf, sizeof(buf));
				fread(buf, 1024, 1, fp);
				if(strlen(buf) > 0) {
					snprintf(user, 256, "%s", buf);
					isAuth = true;
				}
			}
			pclose(fp);
		}
	}
#endif

	if (httpMethod == HTTP_GET) {
		if (isAuth) {
			if (ps[0] == "status") {
				ZT_NodeStatus status;
				_node->status(&status);

				Utils::snprintf(tmp,sizeof(tmp),"%.10llx",status.address);
				res["address"] = tmp;
				res["publicIdentity"] = status.publicIdentity;
				res["online"] = (bool)(status.online != 0);
				res["tcpFallbackActive"] = _svc->tcpFallbackActive();
				res["versionMajor"] = ZEROTIER_ONE_VERSION_MAJOR;
				res["versionMinor"] = ZEROTIER_ONE_VERSION_MINOR;
				res["versionRev"] = ZEROTIER_ONE_VERSION_REVISION;
				res["versionBuild"] = ZEROTIER_ONE_VERSION_BUILD;
				Utils::snprintf(tmp,sizeof(tmp),"%d.%d.%d",ZEROTIER_ONE_VERSION_MAJOR,ZEROTIER_ONE_VERSION_MINOR,ZEROTIER_ONE_VERSION_REVISION);
				res["version"] = tmp;
				res["clock"] = OSUtils::now();

				World planet(_node->planet());
				res["planetWorldId"] = planet.id();
				res["planetWorldTimestamp"] = planet.timestamp();

#ifdef ZT_ENABLE_CLUSTER
				nlohmann::json cj;
				ZT_ClusterStatus cs;
				_node->clusterStatus(&cs);
				if (cs.clusterSize >= 1) {
					nlohmann::json cja = nlohmann::json::array();
					for(unsigned int i=0;i<cs.clusterSize;++i) {
						nlohmann::json cjm;
						cjm["id"] = (int)cs.members[i].id;
						cjm["msSinceLastHeartbeat"] = cs.members[i].msSinceLastHeartbeat;
						cjm["alive"] = (bool)(cs.members[i].alive != 0);
						cjm["x"] = cs.members[i].x;
						cjm["y"] = cs.members[i].y;
						cjm["z"] = cs.members[i].z;
						cjm["load"] = cs.members[i].load;
						cjm["peers"] = cs.members[i].peers;
						cja.push_back(cjm);
					}
					cj["members"] = cja;
					cj["myId"] = (int)cs.myId;
					cj["clusterSize"] = cs.clusterSize;
				}
				res["cluster"] = cj;
#else
				res["cluster"] = nlohmann::json();
#endif

				scode = 200;
			} else if (ps[0] == "moon") {
				std::vector<World> moons(_node->moons());
				if (ps.size() == 1) {
					// Return [array] of all moons

					res = nlohmann::json::array();
					for(std::vector<World>::const_iterator m(moons.begin());m!=moons.end();++m) {
						nlohmann::json mj;
						_moonToJson(mj,*m);
						res.push_back(mj);
					}

					scode = 200;
				} else {
					// Return a single moon by ID

					const uint64_t id = Utils::hexStrToU64(ps[1].c_str());
					for(std::vector<World>::const_iterator m(moons.begin());m!=moons.end();++m) {
						if (m->id() == id) {
							_moonToJson(res,*m);
							scode = 200;
							break;
						}
					}

				}
			} else if (ps[0] == "network") {
				ZT_VirtualNetworkList *nws = _node->networks();
				if (nws) {
					if (ps.size() == 1) {
						// Return [array] of all networks

						res = nlohmann::json::array();
						for(unsigned long i=0;i<nws->networkCount;++i) {
							OneService::NetworkSettings localSettings;
							_svc->getNetworkSettings(nws->networks[i].nwid,localSettings);
							nlohmann::json nj;
							_networkToJson(nj,&(nws->networks[i]),_svc->portDeviceName(nws->networks[i].nwid),localSettings);
							res.push_back(nj);
						}

						scode = 200;
					} else if (ps.size() == 2) {
						// Return a single network by ID or 404 if not found

						const uint64_t wantnw = Utils::hexStrToU64(ps[1].c_str());
						for(unsigned long i=0;i<nws->networkCount;++i) {
							if (nws->networks[i].nwid == wantnw) {
								OneService::NetworkSettings localSettings;
								_svc->getNetworkSettings(nws->networks[i].nwid,localSettings);
								_networkToJson(res,&(nws->networks[i]),_svc->portDeviceName(nws->networks[i].nwid),localSettings);
								scode = 200;
								break;
							}
						}

					} else scode = 404;
					_node->freeQueryResult((void *)nws);
				} else scode = 500;
			} else if (ps[0] == "peer") {
				ZT_PeerList *pl = _node->peers();
				if (pl) {
					if (ps.size() == 1) {
						// Return [array] of all peers

						res = nlohmann::json::array();
						for(unsigned long i=0;i<pl->peerCount;++i) {
							nlohmann::json pj;
							_peerToJson(pj,&(pl->peers[i]));
							res.push_back(pj);
						}

						scode = 200;
					} else if (ps.size() == 2) {
						// Return a single peer by ID or 404 if not found

						uint64_t wantp = Utils::hexStrToU64(ps[1].c_str());
						for(unsigned long i=0;i<pl->peerCount;++i) {
							if (pl->peers[i].address == wantp) {
								_peerToJson(res,&(pl->peers[i]));
								scode = 200;
								break;
							}
						}

					} else scode = 404;
					_node->freeQueryResult((void *)pl);
				} else scode = 500;
			} else {
				if (_controller) {
					scode = _controller->handleControlPlaneHttpGET(std::vector<std::string>(ps.begin()+1,ps.end()),urlArgs,headers,body,responseBody,responseContentType);
				} else scode = 404;
			}

		} else scode = 401; // isAuth == false
	} else if ((httpMethod == HTTP_POST)||(httpMethod == HTTP_PUT)) {
		if (isAuth) {

			if (ps[0] == "moon") {
				if (ps.size() == 2) {

					uint64_t seed = 0;
					try {
						nlohmann::json j(OSUtils::jsonParse(body));
						if (j.is_object()) {
							seed = Utils::hexStrToU64(OSUtils::jsonString(j["seed"],"0").c_str());
						}
					} catch ( ... ) {
						// discard invalid JSON
					}

					std::vector<World> moons(_node->moons());
					const uint64_t id = Utils::hexStrToU64(ps[1].c_str());
					for(std::vector<World>::const_iterator m(moons.begin());m!=moons.end();++m) {
						if (m->id() == id) {
							_moonToJson(res,*m);
							scode = 200;
							break;
						}
					}

					if ((scode != 200)&&(seed != 0)) {
						char tmp[64];
						Utils::snprintf(tmp,sizeof(tmp),"%.16llx",id);
						res["id"] = tmp;
						res["roots"] = nlohmann::json::array();
						res["timestamp"] = 0;
						res["signature"] = nlohmann::json();
						res["updatesMustBeSignedBy"] = nlohmann::json();
						res["waiting"] = true;
						_node->orbit(id,seed);
					}

				} else scode = 404;
			} else if (ps[0] == "network") {
				if (ps.size() == 2) {

					uint64_t wantnw = Utils::hexStrToU64(ps[1].c_str());
					_node->join(wantnw,(void *)0); // does nothing if we are a member
					ZT_VirtualNetworkList *nws = _node->networks();
					if (nws) {
						for(unsigned long i=0;i<nws->networkCount;++i) {
							if (nws->networks[i].nwid == wantnw) {
								OneService::NetworkSettings localSettings;
								_svc->getNetworkSettings(nws->networks[i].nwid,localSettings);

								try {
									nlohmann::json j(OSUtils::jsonParse(body));
									if (j.is_object()) {
										nlohmann::json &allowManaged = j["allowManaged"];
										if (allowManaged.is_boolean()) localSettings.allowManaged = (bool)allowManaged;
										nlohmann::json &allowGlobal = j["allowGlobal"];
										if (allowGlobal.is_boolean()) localSettings.allowGlobal = (bool)allowGlobal;
										nlohmann::json &allowDefault = j["allowDefault"];
										if (allowDefault.is_boolean()) localSettings.allowDefault = (bool)allowDefault;
									}
								} catch ( ... ) {
									// discard invalid JSON
								}

								_svc->setNetworkSettings(nws->networks[i].nwid,localSettings);
								_networkToJson(res,&(nws->networks[i]),_svc->portDeviceName(nws->networks[i].nwid),localSettings);

								scode = 200;
								break;
							}
						}
						_node->freeQueryResult((void *)nws);
					} else scode = 500;

				} else scode = 404;
			} else {
				if (_controller)
					scode = _controller->handleControlPlaneHttpPOST(std::vector<std::string>(ps.begin()+1,ps.end()),urlArgs,headers,body,responseBody,responseContentType);
				else scode = 404;
			}

		} else scode = 401; // isAuth == false
	} else if (httpMethod == HTTP_DELETE) {
		if (isAuth) {

			if (ps[0] == "moon") {
				if (ps.size() == 2) {
					_node->deorbit(Utils::hexStrToU64(ps[1].c_str()));
					res["result"] = true;
					scode = 200;
				} // else 404
			} else if (ps[0] == "network") {
				ZT_VirtualNetworkList *nws = _node->networks();
				if (nws) {
					if (ps.size() == 2) {
						uint64_t wantnw = Utils::hexStrToU64(ps[1].c_str());
						for(unsigned long i=0;i<nws->networkCount;++i) {
							if (nws->networks[i].nwid == wantnw) {
								_node->leave(wantnw,(void **)0);
								res["result"] = true;
								scode = 200;
								break;
							}
						}
					} // else 404
					_node->freeQueryResult((void *)nws);
				} else scode = 500;
			} else {
				if (_controller)
					scode = _controller->handleControlPlaneHttpDELETE(std::vector<std::string>(ps.begin()+1,ps.end()),urlArgs,headers,body,responseBody,responseContentType);
				else scode = 404;
			}

		} else scode = 401; // isAuth = false
	} else {
		scode = 400;
	}

	if (responseBody.length() == 0) {
		if ((res.is_object())||(res.is_array()))
			responseBody = OSUtils::jsonDump(res);
		else responseBody = "{}";
		responseContentType = "application/json";
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
