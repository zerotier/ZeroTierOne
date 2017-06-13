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
 */

#include "JSONDB.hpp"

#define ZT_JSONDB_HTTP_TIMEOUT 60000

namespace ZeroTier {

static const nlohmann::json _EMPTY_JSON(nlohmann::json::object());
static const std::map<std::string,std::string> _ZT_JSONDB_GET_HEADERS;

JSONDB::JSONDB(const std::string &basePath) :
	_basePath(basePath),
	_summaryThreadRun(true)
{
	if ((_basePath.length() > 7)&&(_basePath.substr(0,7) == "http://")) {
		// TODO: this doesn't yet support IPv6 since bracketed address notiation isn't supported.
		// Typically it's just used with 127.0.0.1 anyway.
		std::string hn = _basePath.substr(7);
		std::size_t hnend = hn.find_first_of('/');
		if (hnend != std::string::npos)
			hn = hn.substr(0,hnend);
		std::size_t hnsep = hn.find_last_of(':');
		if (hnsep != std::string::npos)
			hn[hnsep] = '/';
		_httpAddr.fromString(hn);
		if (hnend != std::string::npos)
			_basePath = _basePath.substr(7 + hnend);
		if (_basePath.length() == 0)
			_basePath = "/";
		if (_basePath[0] != '/')
			_basePath = std::string("/") + _basePath;
	} else {
		OSUtils::mkdir(_basePath.c_str());
		OSUtils::lockDownFile(_basePath.c_str(),true); // networks might contain auth tokens, etc., so restrict directory permissions
	}

	unsigned int cnt = 0;
	while (!_load(_basePath)) {
		if ((++cnt & 7) == 0)
			fprintf(stderr,"WARNING: controller still waiting to read '%s'..." ZT_EOL_S,_basePath.c_str());
		Thread::sleep(250);
	}

	for(std::unordered_map<uint64_t,_NW>::iterator n(_networks.begin());n!=_networks.end();++n)
		_recomputeSummaryInfo(n->first);
	for(;;) {
		_summaryThread_m.lock();
		if (_summaryThreadToDo.empty()) {
			_summaryThread_m.unlock();
			break;
		}
		_summaryThread_m.unlock();
		Thread::sleep(50);
	}
}

JSONDB::~JSONDB()
{
	{
		Mutex::Lock _l(_networks_m);
		_networks.clear();
	}
	Thread t;
	{
		Mutex::Lock _l(_summaryThread_m);
		_summaryThreadRun = false;
		t = _summaryThread;
	}
	if (t)
		Thread::join(t);
}

bool JSONDB::writeRaw(const std::string &n,const std::string &obj)
{
	if (_httpAddr) {
		std::map<std::string,std::string> headers;
		std::string body;
		std::map<std::string,std::string> reqHeaders;
		char tmp[64];
		Utils::snprintf(tmp,sizeof(tmp),"%lu",(unsigned long)obj.length());
		reqHeaders["Content-Length"] = tmp;
		reqHeaders["Content-Type"] = "application/json";
		const unsigned int sc = Http::PUT(0,ZT_JSONDB_HTTP_TIMEOUT,reinterpret_cast<const struct sockaddr *>(&_httpAddr),(_basePath+"/"+n).c_str(),reqHeaders,obj.data(),(unsigned long)obj.length(),headers,body);
		return (sc == 200);
	} else {
		const std::string path(_genPath(n,true));
		if (!path.length())
			return false;
		return OSUtils::writeFile(path.c_str(),obj);
	}
}

bool JSONDB::hasNetwork(const uint64_t networkId) const
{
	Mutex::Lock _l(_networks_m);
	return (_networks.find(networkId) != _networks.end());
}

bool JSONDB::getNetwork(const uint64_t networkId,nlohmann::json &config) const
{
	Mutex::Lock _l(_networks_m);
	const std::unordered_map<uint64_t,_NW>::const_iterator i(_networks.find(networkId));
	if (i == _networks.end())
		return false;
	config = nlohmann::json::from_msgpack(i->second.config);
	return true;
}

bool JSONDB::getNetworkSummaryInfo(const uint64_t networkId,NetworkSummaryInfo &ns) const
{
	Mutex::Lock _l(_networks_m);
	const std::unordered_map<uint64_t,_NW>::const_iterator i(_networks.find(networkId));
	if (i == _networks.end())
		return false;
	ns = i->second.summaryInfo;
	return true;
}

int JSONDB::getNetworkAndMember(const uint64_t networkId,const uint64_t nodeId,nlohmann::json &networkConfig,nlohmann::json &memberConfig,NetworkSummaryInfo &ns) const
{
	Mutex::Lock _l(_networks_m);
	const std::unordered_map<uint64_t,_NW>::const_iterator i(_networks.find(networkId));
	if (i == _networks.end())
		return 0;
	const std::unordered_map< uint64_t,std::vector<uint8_t> >::const_iterator j(i->second.members.find(nodeId));
	if (j == i->second.members.end())
		return 1;
	networkConfig = nlohmann::json::from_msgpack(i->second.config);
	memberConfig = nlohmann::json::from_msgpack(j->second);
	ns = i->second.summaryInfo;
	return 3;
}

bool JSONDB::getNetworkMember(const uint64_t networkId,const uint64_t nodeId,nlohmann::json &memberConfig) const
{
	Mutex::Lock _l(_networks_m);
	const std::unordered_map<uint64_t,_NW>::const_iterator i(_networks.find(networkId));
	if (i == _networks.end())
		return false;
	const std::unordered_map< uint64_t,std::vector<uint8_t> >::const_iterator j(i->second.members.find(nodeId));
	if (j == i->second.members.end())
		return false;
	memberConfig = nlohmann::json::from_msgpack(j->second);
	return true;
}

void JSONDB::saveNetwork(const uint64_t networkId,const nlohmann::json &networkConfig)
{
	char n[64];
	Utils::snprintf(n,sizeof(n),"network/%.16llx",(unsigned long long)networkId);
	writeRaw(n,OSUtils::jsonDump(networkConfig));
	{
		Mutex::Lock _l(_networks_m);
		_networks[networkId].config = nlohmann::json::to_msgpack(networkConfig);
	}
	_recomputeSummaryInfo(networkId);
}

void JSONDB::saveNetworkMember(const uint64_t networkId,const uint64_t nodeId,const nlohmann::json &memberConfig)
{
	char n[256];
	Utils::snprintf(n,sizeof(n),"network/%.16llx/member/%.10llx",(unsigned long long)networkId,(unsigned long long)nodeId);
	writeRaw(n,OSUtils::jsonDump(memberConfig));
	{
		Mutex::Lock _l(_networks_m);
		_networks[networkId].members[nodeId] = nlohmann::json::to_msgpack(memberConfig);
	}
	_recomputeSummaryInfo(networkId);
}

nlohmann::json JSONDB::eraseNetwork(const uint64_t networkId)
{
	if (!_httpAddr) { // Member deletion is done by Central in harnessed mode, and deleting the cache network entry also deletes all members
		std::vector<uint64_t> memberIds;
		{
			Mutex::Lock _l(_networks_m);
			const std::unordered_map<uint64_t,_NW>::iterator i(_networks.find(networkId));
			if (i == _networks.end())
				return _EMPTY_JSON;
			for(std::unordered_map< uint64_t,std::vector<uint8_t> >::iterator m(i->second.members.begin());m!=i->second.members.end();++m)
				memberIds.push_back(m->first);
		}
		for(std::vector<uint64_t>::iterator m(memberIds.begin());m!=memberIds.end();++m)
			eraseNetworkMember(networkId,*m,false);
	}

	char n[256];
	Utils::snprintf(n,sizeof(n),"network/%.16llx",(unsigned long long)networkId);

	if (_httpAddr) {
		// Deletion is currently done by Central in harnessed mode
		//std::map<std::string,std::string> headers;
		//std::string body;
		//Http::DEL(0,ZT_JSONDB_HTTP_TIMEOUT,reinterpret_cast<const struct sockaddr *>(&_httpAddr),(_basePath+"/"+n).c_str(),_ZT_JSONDB_GET_HEADERS,headers,body);
	} else {
		const std::string path(_genPath(n,false));
		if (path.length())
			OSUtils::rm(path.c_str());
	}

	{
		Mutex::Lock _l(_networks_m);
		std::unordered_map<uint64_t,_NW>::iterator i(_networks.find(networkId));
		if (i == _networks.end())
			return _EMPTY_JSON; // sanity check, shouldn't happen
		nlohmann::json tmp(nlohmann::json::from_msgpack(i->second.config));
		_networks.erase(i);
		return tmp;
	}
}

nlohmann::json JSONDB::eraseNetworkMember(const uint64_t networkId,const uint64_t nodeId,bool recomputeSummaryInfo)
{
	char n[256];
	Utils::snprintf(n,sizeof(n),"network/%.16llx/member/%.10llx",(unsigned long long)networkId,(unsigned long long)nodeId);

	if (_httpAddr) {
		// Deletion is currently done by the caller in Central harnessed mode
		//std::map<std::string,std::string> headers;
		//std::string body;
		//Http::DEL(0,ZT_JSONDB_HTTP_TIMEOUT,reinterpret_cast<const struct sockaddr *>(&_httpAddr),(_basePath+"/"+n).c_str(),_ZT_JSONDB_GET_HEADERS,headers,body);
	} else {
		const std::string path(_genPath(n,false));
		if (path.length())
			OSUtils::rm(path.c_str());
	}

	{
		Mutex::Lock _l(_networks_m);
		std::unordered_map<uint64_t,_NW>::iterator i(_networks.find(networkId));
		if (i == _networks.end())
			return _EMPTY_JSON;
		std::unordered_map< uint64_t,std::vector<uint8_t> >::iterator j(i->second.members.find(nodeId));
		if (j == i->second.members.end())
			return _EMPTY_JSON;
		nlohmann::json tmp(j->second);
		i->second.members.erase(j);
		if (recomputeSummaryInfo)
			_recomputeSummaryInfo(networkId);
		return tmp;
	}
}

void JSONDB::threadMain()
	throw()
{
	std::vector<uint64_t> todo;
	while (_summaryThreadRun) {
		Thread::sleep(10);

		{
			Mutex::Lock _l(_summaryThread_m);
			if (_summaryThreadToDo.empty())
				continue;
			else _summaryThreadToDo.swap(todo);
		}

		const uint64_t now = OSUtils::now();
		for(std::vector<uint64_t>::iterator ii(todo.begin());ii!=todo.end();++ii) {
			const uint64_t networkId = *ii;

			Mutex::Lock _l(_networks_m);
			std::unordered_map<uint64_t,_NW>::iterator n(_networks.find(networkId));
			if (n != _networks.end()) {
				NetworkSummaryInfo &ns = n->second.summaryInfo;
				ns.activeBridges.clear();
				ns.allocatedIps.clear();
				ns.authorizedMemberCount = 0;
				ns.activeMemberCount = 0;
				ns.totalMemberCount = 0;
				ns.mostRecentDeauthTime = 0;

				for(std::unordered_map< uint64_t,std::vector<uint8_t> >::const_iterator m(n->second.members.begin());m!=n->second.members.end();++m) {
					try {
						nlohmann::json member(nlohmann::json::from_msgpack(m->second));

						if (OSUtils::jsonBool(member["authorized"],false)) {
							++ns.authorizedMemberCount;

							try {
								const nlohmann::json &mlog = member["recentLog"];
								if ((mlog.is_array())&&(mlog.size() > 0)) {
									const nlohmann::json &mlog1 = mlog[0];
									if (mlog1.is_object()) {
										if ((now - OSUtils::jsonInt(mlog1["ts"],0ULL)) < (ZT_NETWORK_AUTOCONF_DELAY * 2))
											++ns.activeMemberCount;
									}
								}
							} catch ( ... ) {}

							try {
								if (OSUtils::jsonBool(member["activeBridge"],false))
									ns.activeBridges.push_back(Address(m->first));
							} catch ( ... ) {}

							try {
								const nlohmann::json &mips = member["ipAssignments"];
								if (mips.is_array()) {
									for(unsigned long i=0;i<mips.size();++i) {
										InetAddress mip(OSUtils::jsonString(mips[i],""));
										if ((mip.ss_family == AF_INET)||(mip.ss_family == AF_INET6))
											ns.allocatedIps.push_back(mip);
									}
								}
							} catch ( ... ) {}
						} else {
							try {
								ns.mostRecentDeauthTime = std::max(ns.mostRecentDeauthTime,OSUtils::jsonInt(member["lastDeauthorizedTime"],0ULL));
							} catch ( ... ) {}
						}
						++ns.totalMemberCount;
					} catch ( ... ) {}
				}

				std::sort(ns.activeBridges.begin(),ns.activeBridges.end());
				std::sort(ns.allocatedIps.begin(),ns.allocatedIps.end());

				n->second.summaryInfoLastComputed = now;
			}
		}

		todo.clear();
	}
}

bool JSONDB::_load(const std::string &p)
{
	if (_httpAddr) {
		// In HTTP harnessed mode we download our entire working data set on startup.

		std::string body;
		std::map<std::string,std::string> headers;
		const unsigned int sc = Http::GET(0,ZT_JSONDB_HTTP_TIMEOUT,reinterpret_cast<const struct sockaddr *>(&_httpAddr),_basePath.c_str(),_ZT_JSONDB_GET_HEADERS,headers,body);
		if (sc == 200) {
			try {
				nlohmann::json dbImg(OSUtils::jsonParse(body));
				std::string tmp;
				if (dbImg.is_object()) {
					Mutex::Lock _l(_networks_m);
					for(nlohmann::json::iterator i(dbImg.begin());i!=dbImg.end();++i) {
						nlohmann::json &j = i.value();
						if (j.is_object()) {
							std::string id(OSUtils::jsonString(j["id"],"0"));
							std::string objtype(OSUtils::jsonString(j["objtype"],""));

							if ((id.length() == 16)&&(objtype == "network")) {
								const uint64_t nwid = Utils::hexStrToU64(id.c_str());
								if (nwid)
									_networks[nwid].config = nlohmann::json::to_msgpack(j);
							} else if ((id.length() == 10)&&(objtype == "member")) {
								const uint64_t mid = Utils::hexStrToU64(id.c_str());
								const uint64_t nwid = Utils::hexStrToU64(OSUtils::jsonString(j["nwid"],"0").c_str());
								if ((mid)&&(nwid))
									_networks[nwid].members[mid] = nlohmann::json::to_msgpack(j);
							}
						}
					}
					return true;
				}
			} catch ( ... ) {} // invalid JSON, so maybe incomplete request
		}
		return false;

	} else {
		// In regular mode we recursively read it from controller.d/ on disk

		std::vector<std::string> dl(OSUtils::listDirectory(p.c_str(),true));
		for(std::vector<std::string>::const_iterator di(dl.begin());di!=dl.end();++di) {
			if ((di->length() > 5)&&(di->substr(di->length() - 5) == ".json")) {
				std::string buf;
				if (OSUtils::readFile((p + ZT_PATH_SEPARATOR_S + *di).c_str(),buf)) {
					try {
						nlohmann::json j(OSUtils::jsonParse(buf));
						std::string id(OSUtils::jsonString(j["id"],"0"));
						std::string objtype(OSUtils::jsonString(j["objtype"],""));

						if ((id.length() == 16)&&(objtype == "network")) {
							const uint64_t nwid = Utils::hexStrToU64(id.c_str());
							if (nwid) {
								Mutex::Lock _l(_networks_m);
								_networks[nwid].config = nlohmann::json::to_msgpack(j);
							}
						} else if ((id.length() == 10)&&(objtype == "member")) {
							const uint64_t mid = Utils::hexStrToU64(id.c_str());
							const uint64_t nwid = Utils::hexStrToU64(OSUtils::jsonString(j["nwid"],"0").c_str());
							if ((mid)&&(nwid)) {
								Mutex::Lock _l(_networks_m);
								_networks[nwid].members[mid] = nlohmann::json::to_msgpack(j);
							}
						}
					} catch ( ... ) {}
				}
			} else {
				this->_load((p + ZT_PATH_SEPARATOR_S + *di));
			}
		}
		return true;

	}
}

void JSONDB::_recomputeSummaryInfo(const uint64_t networkId)
{
	Mutex::Lock _l(_summaryThread_m);
	if (std::find(_summaryThreadToDo.begin(),_summaryThreadToDo.end(),networkId) == _summaryThreadToDo.end())
		_summaryThreadToDo.push_back(networkId);
	if (!_summaryThread)
		_summaryThread = Thread::start(this);
}

std::string JSONDB::_genPath(const std::string &n,bool create)
{
	std::vector<std::string> pt(OSUtils::split(n.c_str(),"/","",""));
	if (pt.size() == 0)
		return std::string();

	char sep;
	if (_httpAddr) {
		sep = '/';
		create = false;
	} else {
		sep = ZT_PATH_SEPARATOR;
	}

	std::string p(_basePath);
	if (create) OSUtils::mkdir(p.c_str());
	for(unsigned long i=0,j=(unsigned long)(pt.size()-1);i<j;++i) {
		p.push_back(sep);
		p.append(pt[i]);
		if (create) OSUtils::mkdir(p.c_str());
	}

	p.push_back(sep);
	p.append(pt[pt.size()-1]);
	p.append(".json");

	return p;
}

} // namespace ZeroTier
