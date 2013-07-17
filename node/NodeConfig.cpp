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
#include <string.h>
#include <memory>
#include <string>

#include <json/json.h>

#include "NodeConfig.hpp"
#include "RuntimeEnvironment.hpp"
#include "Defaults.hpp"
#include "Utils.hpp"
#include "Logger.hpp"

namespace ZeroTier {

NodeConfig::NodeConfig(const RuntimeEnvironment *renv,const std::string &url) :
	_r(renv),
	_lastAutoconfigure(0),
	_lastAutoconfigureLastModified(),
	_url(url),
	_autoconfigureLock(),
	_networks(),
	_networks_m()
{
}

NodeConfig::~NodeConfig()
{
	_autoconfigureLock.lock(); // wait for any autoconfs to finish
	_autoconfigureLock.unlock();
}

void NodeConfig::whackAllTaps()
{
	std::vector< SharedPtr<Network> > nwlist;
	Mutex::Lock _l(_networks_m);
	for(std::map< uint64_t,SharedPtr<Network> >::const_iterator n(_networks.begin());n!=_networks.end();++n)
		n->second->tap().whack();
}

void NodeConfig::refreshConfiguration()
{
	_autoconfigureLock.lock(); // unlocked when handler gets called

	TRACE("refreshing autoconfigure URL %s (if modified since: '%s')",_url.c_str(),_lastAutoconfigureLastModified.c_str());

	std::map<std::string,std::string> reqHeaders;
	reqHeaders["X-ZT-ID"] = _r->identity.toString(false);
	reqHeaders["X-ZT-OVSH"] = _r->ownershipVerificationSecretHash;
	if (_lastAutoconfigureLastModified.length())
		reqHeaders["If-Modified-Since"] = _lastAutoconfigureLastModified;

	new Http::Request(Http::HTTP_METHOD_GET,_url,reqHeaders,std::string(),&NodeConfig::_CBautoconfHandler,this);
}

void NodeConfig::__CBautoconfHandler(const std::string &lastModified,const std::string &body)
{
	try {
		Json::Value root;
		Json::Reader reader;

		std::string dec(_r->identity.decrypt(_r->configAuthority,body.data(),body.length()));
		if (!dec.length()) {
			LOG("autoconfigure from %s failed: data did not decrypt as from config authority %s",_url.c_str(),_r->configAuthority.address().toString().c_str());
			return;
		}
		TRACE("decrypted autoconf: %s",dec.c_str());

		if (!reader.parse(dec,root,false)) {
			LOG("autoconfigure from %s failed: JSON parse error: %s",_url.c_str(),reader.getFormattedErrorMessages().c_str());
			return;
		}

		if (!root.isObject()) {
			LOG("autoconfigure from %s failed: not a JSON object",_url.c_str());
			return;
		}

		// Configure networks
		const Json::Value &networks = root["_networks"];
		if (networks.isArray()) {
			Mutex::Lock _l(_networks_m);
			for(unsigned int ni=0;ni<networks.size();++ni) {
				if (networks[ni].isObject()) {
					const Json::Value &nwid_ = networks[ni]["id"];
					uint64_t nwid = nwid_.isNumeric() ? (uint64_t)nwid_.asUInt64() : (uint64_t)strtoull(networks[ni]["id"].asString().c_str(),(char **)0,10);

					if (nwid) {
						SharedPtr<Network> nw;
						std::map< uint64_t,SharedPtr<Network> >::iterator nwent(_networks.find(nwid));
						if (nwent != _networks.end())
							nw = nwent->second;
						else {
							try {
								nw = SharedPtr<Network>(new Network(_r,nwid));
								_networks[nwid] = nw;
							} catch (std::exception &exc) {
								LOG("unable to create network %llu: %s",nwid,exc.what());
							} catch ( ... ) {
								LOG("unable to create network %llu: unknown exception",nwid);
							}
						}

						if (nw) {
							Mutex::Lock _l2(nw->_lock);
							nw->_open = networks[ni]["isOpen"].asBool();

							// Ensure that TAP device has all the right IP addresses
							// TODO: IPv6 might work a tad differently
							std::set<InetAddress> allIps;
							const Json::Value &addresses = networks[ni]["_addresses"];
							if (addresses.isArray()) {
								for(unsigned int ai=0;ai<addresses.size();++ai) {
									if (addresses[ai].isString()) {
										InetAddress addr(addresses[ai].asString());
										if (addr) {
											TRACE("network %llu IP/netmask: %s",nwid,addr.toString().c_str());
											allIps.insert(addr);
										}
									}
								}
							}
							nw->_tap.setIps(allIps);

							// NOTE: the _members field is optional for open networks,
							// since members of open nets do not need to check membership
							// of packet senders and mutlicasters.
							const Json::Value &members = networks[ni]["_members"];
							nw->_members.clear();
							if (members.isArray()) {
								for(unsigned int mi=0;mi<members.size();++mi) {
									std::string rawAddr(Utils::unhex(members[mi].asString()));
									if (rawAddr.length() == ZT_ADDRESS_LENGTH) {
										Address addr(rawAddr.data());
										if ((addr)&&(!addr.isReserved())) {
											//TRACE("network %llu member: %s",nwid,addr.toString().c_str());
											nw->_members.insert(addr);
										}
									}
								}
							}
						}
					} else {
						TRACE("ignored networks[%u], 'id' field missing");
					}
				} else {
					TRACE("ignored networks[%u], not a JSON object",ni);
				}
			}
		}

		_lastAutoconfigure = Utils::now();
		_lastAutoconfigureLastModified = lastModified;
	} catch (std::exception &exc) {
		TRACE("exception parsing autoconf URL response: %s",exc.what());
	} catch ( ... ) {
		TRACE("unexpected exception parsing autoconf URL response");
	}
}

bool NodeConfig::_CBautoconfHandler(Http::Request *req,void *arg,const std::string &url,int code,const std::map<std::string,std::string> &headers,const std::string &body)
{
#ifdef ZT_TRACE
	const RuntimeEnvironment *_r = ((NodeConfig *)arg)->_r;
#endif

	if (code == 200) {
		TRACE("200 got autoconfigure response from %s: %u bytes",url.c_str(),(unsigned int)body.length());

		std::map<std::string,std::string>::const_iterator lm(headers.find("Last-Modified"));
		if (lm != headers.end())
			((NodeConfig *)arg)->__CBautoconfHandler(lm->second,body);
		else ((NodeConfig *)arg)->__CBautoconfHandler(std::string(),body);
	} else if (code == 304) {
		TRACE("304 autoconfigure deferred, remote URL %s not modified",url.c_str());
		((NodeConfig *)arg)->_lastAutoconfigure = Utils::now(); // still considered a success
	} else if (code == 409) { // conflict, ID address in use by another ID
		TRACE("%d autoconfigure failed from %s",code,url.c_str());
	} else {
		TRACE("%d autoconfigure failed from %s",code,url.c_str());
	}

	((NodeConfig *)arg)->_autoconfigureLock.unlock();
	return false; // causes Request to delete itself
}

} // namespace ZeroTier
