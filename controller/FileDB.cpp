/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#include "FileDB.hpp"

#include "../node/Metrics.hpp"

namespace ZeroTier
{

FileDB::FileDB(const char *path) :
	DB(),
	_path(path),
	_networksPath(_path + ZT_PATH_SEPARATOR_S + "network"),
	_tracePath(_path + ZT_PATH_SEPARATOR_S + "trace"),
	_running(true)
{
	OSUtils::mkdir(_path.c_str());
	OSUtils::lockDownFile(_path.c_str(),true);
	OSUtils::mkdir(_networksPath.c_str());
	OSUtils::mkdir(_tracePath.c_str());

	std::vector<std::string> networks(OSUtils::listDirectory(_networksPath.c_str(),false));
	std::string buf;
	for(auto n=networks.begin();n!=networks.end();++n) {
		buf.clear();
		if ((n->length() == 21)&&(OSUtils::readFile((_networksPath + ZT_PATH_SEPARATOR_S + *n).c_str(),buf))) {
			try {
				nlohmann::json network(OSUtils::jsonParse(buf));
				const std::string nwids = network["id"];
				if (nwids.length() == 16) {
					nlohmann::json nullJson;
					_networkChanged(nullJson,network,false);
					Metrics::network_count++;
					std::string membersPath(_networksPath + ZT_PATH_SEPARATOR_S + nwids + ZT_PATH_SEPARATOR_S "member");
					std::vector<std::string> members(OSUtils::listDirectory(membersPath.c_str(),false));
					for(auto m=members.begin();m!=members.end();++m) {
						buf.clear();
						if ((m->length() == 15)&&(OSUtils::readFile((membersPath + ZT_PATH_SEPARATOR_S + *m).c_str(),buf))) {
							try {
								nlohmann::json member(OSUtils::jsonParse(buf));
								const std::string addrs = member["id"];
								if (addrs.length() == 10) {
									nlohmann::json nullJson2;
									_memberChanged(nullJson2,member,false);
									Metrics::member_count++;
								}
							} catch ( ... ) {}
						}
					}
				}
			} catch ( ... ) {}
		}
	}
}

FileDB::~FileDB()
{
	try {
		_online_l.lock();
		_running = false;
		_online_l.unlock();
		_onlineUpdateThread.join();
	} catch ( ... ) {}
}

bool FileDB::waitForReady() { return true; }
bool FileDB::isReady() { return true; }

bool FileDB::save(nlohmann::json &record,bool notifyListeners)
{
	char p1[4096],p2[4096],pb[4096];
	bool modified = false;
	try {
		const std::string objtype = record["objtype"];
		if (objtype == "network") {

			const uint64_t nwid = OSUtils::jsonIntHex(record["id"],0ULL);
			if (nwid) {
				nlohmann::json old;
				get(nwid,old);
				if ((!old.is_object())||(!_compareRecords(old,record))) {
					record["revision"] = OSUtils::jsonInt(record["revision"],0ULL) + 1ULL;
					OSUtils::ztsnprintf(p1,sizeof(p1),"%s" ZT_PATH_SEPARATOR_S "%.16llx.json",_networksPath.c_str(),nwid);
					if (!OSUtils::writeFile(p1,OSUtils::jsonDump(record,-1))) {
						fprintf(stderr,"WARNING: controller unable to write to path: %s" ZT_EOL_S,p1);
					}
					_networkChanged(old,record,notifyListeners);
					modified = true;
				}
			}

		} else if (objtype == "member") {

			const uint64_t id = OSUtils::jsonIntHex(record["id"],0ULL);
			const uint64_t nwid = OSUtils::jsonIntHex(record["nwid"],0ULL);
			if ((id)&&(nwid)) {
				nlohmann::json network,old;
				get(nwid,network,id,old);
				if ((!old.is_object())||(!_compareRecords(old,record))) {
					record["revision"] = OSUtils::jsonInt(record["revision"],0ULL) + 1ULL;
					OSUtils::ztsnprintf(pb,sizeof(pb),"%s" ZT_PATH_SEPARATOR_S "%.16llx" ZT_PATH_SEPARATOR_S "member",_networksPath.c_str(),(unsigned long long)nwid);
					OSUtils::ztsnprintf(p1,sizeof(p1),"%s" ZT_PATH_SEPARATOR_S "%.10llx.json",pb,(unsigned long long)id);
					if (!OSUtils::writeFile(p1,OSUtils::jsonDump(record,-1))) {
						OSUtils::ztsnprintf(p2,sizeof(p2),"%s" ZT_PATH_SEPARATOR_S "%.16llx",_networksPath.c_str(),(unsigned long long)nwid);
						OSUtils::mkdir(p2);
						OSUtils::mkdir(pb);
						if (!OSUtils::writeFile(p1,OSUtils::jsonDump(record,-1))) {
							fprintf(stderr,"WARNING: controller unable to write to path: %s" ZT_EOL_S,p1);
						}
					}
					_memberChanged(old,record,notifyListeners);
					modified = true;
				}
			}

		}
	} catch ( ... ) {} // drop invalid records missing fields
	return modified;
}

void FileDB::eraseNetwork(const uint64_t networkId)
{
	nlohmann::json network,nullJson;
	get(networkId,network);
	char p[16384];
	OSUtils::ztsnprintf(p,sizeof(p),"%s" ZT_PATH_SEPARATOR_S "%.16llx.json",_networksPath.c_str(),networkId);
	OSUtils::rm(p);
	OSUtils::ztsnprintf(p,sizeof(p),"%s" ZT_PATH_SEPARATOR_S "%.16llx",_networksPath.c_str(),(unsigned long long)networkId);
	OSUtils::rmDashRf(p);
	_networkChanged(network,nullJson,true);
	std::lock_guard<std::mutex> l(this->_online_l);
	this->_online.erase(networkId);
}

void FileDB::eraseMember(const uint64_t networkId,const uint64_t memberId)
{
	nlohmann::json network,member,nullJson;
	get(networkId,network,memberId,member);
	char p[4096];
	OSUtils::ztsnprintf(p,sizeof(p),"%s" ZT_PATH_SEPARATOR_S "%.16llx" ZT_PATH_SEPARATOR_S "member" ZT_PATH_SEPARATOR_S "%.10llx.json",_networksPath.c_str(),networkId,memberId);
	OSUtils::rm(p);
	_memberChanged(member,nullJson,true);
	std::lock_guard<std::mutex> l(this->_online_l);
	this->_online[networkId].erase(memberId);
}

void FileDB::nodeIsOnline(const uint64_t networkId,const uint64_t memberId,const InetAddress &physicalAddress)
{
	char mid[32],atmp[64];
	OSUtils::ztsnprintf(mid,sizeof(mid),"%.10llx",(unsigned long long)memberId);
	physicalAddress.toString(atmp);
	std::lock_guard<std::mutex> l(this->_online_l);
	this->_online[networkId][memberId][OSUtils::now()] = physicalAddress;
}

} // namespace ZeroTier
