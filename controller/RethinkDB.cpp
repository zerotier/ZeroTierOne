/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2018  ZeroTier, Inc.
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

//#define ZT_CONTROLLER_USE_RETHINKDB

#ifdef ZT_CONTROLLER_USE_RETHINKDB

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include "RethinkDB.hpp"
#include "EmbeddedNetworkController.hpp"

#include "../version.h"

#include <chrono>
#include <algorithm>
#include <stdexcept>

#include "../ext/librethinkdbxx/build/include/rethinkdb.h"

namespace R = RethinkDB;
using json = nlohmann::json;

namespace ZeroTier {

static const char *_timestr()
{
	time_t t = time(0);
	char *ts = ctime(&t);
	char *p = ts;
	if (!p)
		return "";
	while (*p) {
		if (*p == '\n') {
			*p = (char)0;
			break;
		}
		++p;
	}
	return ts;
}

RethinkDB::RethinkDB(EmbeddedNetworkController *const nc,const Identity &myId,const char *path) :
	DB(nc,myId,path),
	_ready(2), // two tables need to be synchronized before we're ready, so this is ready when it reaches 0
	_run(1),
	_waitNoticePrinted(false)
{
	// rethinkdb:host:port:db[:auth]
	std::vector<std::string> ps(OSUtils::split(path,":","",""));
	if ((ps.size() < 4)||(ps[0] != "rethinkdb"))
		throw std::runtime_error("invalid rethinkdb database url");
	_host = ps[1];
	_port = Utils::strToInt(ps[2].c_str());
	_db = ps[3];
	if (ps.size() > 4)
		_auth = ps[4];

	_readyLock.lock();

	_membersDbWatcher = std::thread([this]() {
		try {
			while (_run == 1) {
				try {
					std::unique_ptr<R::Connection> rdb(R::connect(this->_host,this->_port,this->_auth));
					if (rdb) {
						_membersDbWatcherConnection = (void *)rdb.get();
						auto cur = R::db(this->_db).table("Member",R::optargs("read_mode","outdated")).get_all(this->_myAddressStr,R::optargs("index","controllerId")).changes(R::optargs("squash",0.05,"include_initial",true,"include_types",true,"include_states",true)).run(*rdb);
						while (cur.has_next()) {
							if (_run != 1) break;
							json tmp(json::parse(cur.next().as_json()));
							if ((tmp["type"] == "state")&&(tmp["state"] == "ready")) {
								if (++this->_ready == 2) {
									if (_waitNoticePrinted)
										fprintf(stderr,"[%s] NOTICE: %.10llx controller RethinkDB data download complete." ZT_EOL_S,_timestr(),(unsigned long long)_myAddress.toInt());
									this->_readyLock.unlock();
								}
							} else {
								try {
									json &ov = tmp["old_val"];
									json &nv = tmp["new_val"];
									json oldConfig,newConfig;
									if (ov.is_object()) oldConfig = ov["config"];
									if (nv.is_object()) newConfig = nv["config"];
									if (oldConfig.is_object()||newConfig.is_object())
										this->_memberChanged(oldConfig,newConfig,(this->_ready <= 0));
								} catch ( ... ) {} // ignore bad records
							}
						}
					}
				} catch (std::exception &e) {
					fprintf(stderr,"[%s] ERROR: %.10llx controller RethinkDB (member change stream): %s" ZT_EOL_S,_timestr(),(unsigned long long)_myAddress.toInt(),e.what());
				} catch (R::Error &e) {
					fprintf(stderr,"[%s] ERROR: %.10llx controller RethinkDB (member change stream): %s" ZT_EOL_S,_timestr(),(unsigned long long)_myAddress.toInt(),e.message.c_str());
				} catch ( ... ) {
					fprintf(stderr,"[%s] ERROR: %.10llx controller RethinkDB (member change stream): unknown exception" ZT_EOL_S,_timestr(),(unsigned long long)_myAddress.toInt());
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(250));
			}
		} catch ( ... ) {}
	});

	_networksDbWatcher = std::thread([this]() {
		try {
			while (_run == 1) {
				try {
					std::unique_ptr<R::Connection> rdb(R::connect(this->_host,this->_port,this->_auth));
					if (rdb) {
						_networksDbWatcherConnection = (void *)rdb.get();
						auto cur = R::db(this->_db).table("Network",R::optargs("read_mode","outdated")).get_all(this->_myAddressStr,R::optargs("index","controllerId")).changes(R::optargs("squash",0.05,"include_initial",true,"include_types",true,"include_states",true)).run(*rdb);
						while (cur.has_next()) {
							if (_run != 1) break;
							json tmp(json::parse(cur.next().as_json()));
							if ((tmp["type"] == "state")&&(tmp["state"] == "ready")) {
								if (--this->_ready == 0) {
									if (_waitNoticePrinted)
										fprintf(stderr,"[%s] NOTICE: %.10llx controller RethinkDB data download complete." ZT_EOL_S,_timestr(),(unsigned long long)_myAddress.toInt());
									this->_readyLock.unlock();
								}
							} else {
								try {
									json &ov = tmp["old_val"];
									json &nv = tmp["new_val"];
									json oldConfig,newConfig;
									if (ov.is_object()) oldConfig = ov["config"];
									if (nv.is_object()) newConfig = nv["config"];
									if (oldConfig.is_object()||newConfig.is_object())
										this->_networkChanged(oldConfig,newConfig,(this->_ready <= 0));
								} catch ( ... ) {} // ignore bad records
							}
						}
					}
				} catch (std::exception &e) {
					fprintf(stderr,"[%s] ERROR: %.10llx controller RethinkDB (network change stream): %s" ZT_EOL_S,_timestr(),(unsigned long long)_myAddress.toInt(),e.what());
				} catch (R::Error &e) {
					fprintf(stderr,"[%s] ERROR: %.10llx controller RethinkDB (network change stream): %s" ZT_EOL_S,_timestr(),(unsigned long long)_myAddress.toInt(),e.message.c_str());
				} catch ( ... ) {
					fprintf(stderr,"[%s] ERROR: %.10llx controller RethinkDB (network change stream): unknown exception" ZT_EOL_S,_timestr(),(unsigned long long)_myAddress.toInt());
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(250));
			}
		} catch ( ... ) {}
	});

	for(int t=0;t<ZT_CONTROLLER_RETHINKDB_COMMIT_THREADS;++t) {
		_commitThread[t] = std::thread([this]() {
			try {
				std::unique_ptr<R::Connection> rdb;
				nlohmann::json *config = (nlohmann::json *)0;
				while ((this->_commitQueue.get(config))&&(_run == 1)) {
					if (!config)
						continue;
					nlohmann::json record;
					const char *table = (const char *)0;
					std::string deleteId;
					try {
						const std::string objtype = (*config)["objtype"];
						if (objtype == "member") {
							const std::string nwid = (*config)["nwid"];
							const std::string id = (*config)["id"];
							record["id"] = nwid + "-" + id;
							record["controllerId"] = this->_myAddressStr;
							record["networkId"] = nwid;
							record["nodeId"] = id;
							record["config"] = *config;
							table = "Member";
						} else if (objtype == "network") {
							const std::string id = (*config)["id"];
							record["id"] = id;
							record["controllerId"] = this->_myAddressStr;
 							record["config"] = *config;
							table = "Network";
						} else if (objtype == "trace") {
							record = *config;
							table = "RemoteTrace";
						} else if (objtype == "_delete_network") {
							deleteId = (*config)["id"];
							table = "Network";
						} else if (objtype == "_delete_member") {
							deleteId = (*config)["nwid"];
							deleteId.push_back('-');
							const std::string tmp = (*config)["id"];
							deleteId.append(tmp);
							table = "Member";
						}
					} catch (std::exception &e) {
						fprintf(stderr,"[%s] ERROR: %.10llx controller RethinkDB (insert/update record creation): %s" ZT_EOL_S,_timestr(),(unsigned long long)_myAddress.toInt(),e.what());
						table = (const char *)0;
					} catch (R::Error &e) {
						fprintf(stderr,"[%s] ERROR: %.10llx controller RethinkDB (insert/update record creation): %s" ZT_EOL_S,_timestr(),(unsigned long long)_myAddress.toInt(),e.message.c_str());
						table = (const char *)0;
					} catch ( ... ) {
						fprintf(stderr,"[%s] ERROR: %.10llx controller RethinkDB (insert/update record creation): unknown exception" ZT_EOL_S,_timestr(),(unsigned long long)_myAddress.toInt());
						table = (const char *)0;
					}
					delete config;
					if (!table)
						continue;
					const std::string jdump(OSUtils::jsonDump(record,-1));

					while (_run == 1) {
						try {
							if (!rdb)
								rdb = R::connect(this->_host,this->_port,this->_auth);
							if (rdb) {
								if (deleteId.length() > 0) {
									//printf("DELETE: %s" ZT_EOL_S,deleteId.c_str());
									R::db(this->_db).table(table).get(deleteId).delete_().run(*rdb);
								} else {
									//printf("UPSERT: %s" ZT_EOL_S,record.dump().c_str());
									R::db(this->_db).table(table).insert(R::Datum::from_json(jdump),R::optargs("conflict","update","return_changes",false)).run(*rdb);
								}
								break;
							} else {
								fprintf(stderr,"[%s] ERROR: %.10llx controller RethinkDB (insert/update): connect failed (will retry)" ZT_EOL_S,_timestr(),(unsigned long long)_myAddress.toInt());
								rdb.reset();
							}
						} catch (std::exception &e) {
							fprintf(stderr,"[%s] ERROR: %.10llx controller RethinkDB (insert/update): %s [%s]" ZT_EOL_S,_timestr(),(unsigned long long)_myAddress.toInt(),e.what(),jdump.c_str());
							rdb.reset();
						} catch (R::Error &e) {
							fprintf(stderr,"[%s] ERROR: %.10llx controller RethinkDB (insert/update): %s [%s]" ZT_EOL_S,_timestr(),(unsigned long long)_myAddress.toInt(),e.message.c_str(),jdump.c_str());
							rdb.reset();
						} catch ( ... ) {
							fprintf(stderr,"[%s] ERROR: %.10llx controller RethinkDB (insert/update): unknown exception [%s]" ZT_EOL_S,_timestr(),(unsigned long long)_myAddress.toInt(),jdump.c_str());
							rdb.reset();
						}
						std::this_thread::sleep_for(std::chrono::milliseconds(250));
					}
				}
			} catch (std::exception &e) {
				fprintf(stderr,"[%s] ERROR: %.10llx controller RethinkDB (insert/update outer loop): %s" ZT_EOL_S,_timestr(),(unsigned long long)_myAddress.toInt(),e.what());
			} catch (R::Error &e) {
				fprintf(stderr,"[%s] ERROR: %.10llx controller RethinkDB (insert/update outer loop): %s" ZT_EOL_S,_timestr(),(unsigned long long)_myAddress.toInt(),e.message.c_str());
			} catch ( ... ) {
				fprintf(stderr,"[%s] ERROR: %.10llx controller RethinkDB (insert/update outer loop): unknown exception" ZT_EOL_S,_timestr(),(unsigned long long)_myAddress.toInt());
			}
		});
	}

	_onlineNotificationThread = std::thread([this]() {
		int64_t lastUpdatedNetworkStatus = 0;
		std::unordered_map< std::pair<uint64_t,uint64_t>,int64_t,_PairHasher > lastOnlineCumulative;
		try {
			std::unique_ptr<R::Connection> rdb;
			while (_run == 1) {
				try {
					if (!rdb) {
						_connected = 0;
						rdb = R::connect(this->_host,this->_port,this->_auth);
					}

					if (rdb) {
						_connected = 1;
						R::Array batch;
						R::Object tmpobj;

						std::unordered_map< std::pair<uint64_t,uint64_t>,std::pair<int64_t,InetAddress>,_PairHasher > lastOnline;
						{
							std::lock_guard<std::mutex> l(_lastOnline_l);
							lastOnline.swap(_lastOnline);
						}

						for(auto i=lastOnline.begin();i!=lastOnline.end();++i) {
							lastOnlineCumulative[i->first] = i->second.first;
							char tmp[64],tmp2[64];
							OSUtils::ztsnprintf(tmp,sizeof(tmp),"%.16llx-%.10llx",i->first.first,i->first.second);
							tmpobj["id"] = tmp;
							tmpobj["ts"] = i->second.first;
							tmpobj["phy"] = i->second.second.toIpString(tmp2);
							batch.emplace_back(tmpobj);
							if (batch.size() >= 1024) {
								R::db(this->_db).table("MemberStatus",R::optargs("read_mode","outdated")).insert(batch,R::optargs("conflict","update")).run(*rdb);
								batch.clear();
							}
						}
						if (batch.size() > 0) {
							R::db(this->_db).table("MemberStatus",R::optargs("read_mode","outdated")).insert(batch,R::optargs("conflict","update")).run(*rdb);
							batch.clear();
						}
						tmpobj.clear();

						const int64_t now = OSUtils::now();
						if ((now - lastUpdatedNetworkStatus) > 10000) {
							lastUpdatedNetworkStatus = now;

							std::vector< std::pair< uint64_t,std::shared_ptr<_Network> > > networks;
							{
								std::lock_guard<std::mutex> l(_networks_l);
								networks.reserve(_networks.size() + 1);
								for(auto i=_networks.begin();i!=_networks.end();++i)
									networks.push_back(*i);
							}

							for(auto i=networks.begin();i!=networks.end();++i) {
								char tmp[64];
								Utils::hex(i->first,tmp);
								tmpobj["id"] = tmp;
								{
									std::lock_guard<std::mutex> l2(i->second->lock);
									tmpobj["authorizedMemberCount"] = i->second->authorizedMembers.size();
									tmpobj["totalMemberCount"] = i->second->members.size();
									unsigned long onlineMemberCount = 0;
									for(auto m=i->second->members.begin();m!=i->second->members.end();++m) {
										auto lo = lastOnlineCumulative.find(std::pair<uint64_t,uint64_t>(i->first,m->first));
										if (lo != lastOnlineCumulative.end()) {
											if ((now - lo->second) <= (ZT_NETWORK_AUTOCONF_DELAY * 2))
												++onlineMemberCount;
											else lastOnlineCumulative.erase(lo);
										}
									}
									tmpobj["onlineMemberCount"] = onlineMemberCount;
									tmpobj["bridgeCount"] = i->second->activeBridgeMembers.size();
									tmpobj["ts"] = now;
								}
								batch.emplace_back(tmpobj);
								if (batch.size() >= 1024) {
									R::db(this->_db).table("NetworkStatus",R::optargs("read_mode","outdated")).insert(batch,R::optargs("conflict","update")).run(*rdb);
									batch.clear();
								}
							}
							if (batch.size() > 0) {
								R::db(this->_db).table("NetworkStatus",R::optargs("read_mode","outdated")).insert(batch,R::optargs("conflict","update")).run(*rdb);
								batch.clear();
							}
						}
					}
				} catch (std::exception &e) {
					fprintf(stderr,"[%s] ERROR: %.10llx controller RethinkDB (node status update): %s" ZT_EOL_S,_timestr(),(unsigned long long)_myAddress.toInt(),e.what());
					rdb.reset();
				} catch (R::Error &e) {
					fprintf(stderr,"[%s] ERROR: %.10llx controller RethinkDB (node status update): %s" ZT_EOL_S,_timestr(),(unsigned long long)_myAddress.toInt(),e.message.c_str());
					rdb.reset();
				} catch ( ... ) {
					fprintf(stderr,"[%s] ERROR: %.10llx controller RethinkDB (node status update): unknown exception" ZT_EOL_S,_timestr(),(unsigned long long)_myAddress.toInt());
					rdb.reset();
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(250));
			}
		} catch ( ... ) {}
	});

	_heartbeatThread = std::thread([this]() {
		try {
			R::Object controllerRecord;
			std::unique_ptr<R::Connection> rdb;

			{
				char publicId[1024];
				//char secretId[1024];
				char hostname[1024];
				this->_myId.toString(false,publicId);
				//this->_myId.toString(true,secretId);
				if (gethostname(hostname,sizeof(hostname)) != 0) {
					hostname[0] = (char)0;
				} else {
					for(int i=0;i<sizeof(hostname);++i) {
						if ((hostname[i] == '.')||(hostname[i] == 0)) {
							hostname[i] = (char)0;
							break;
						}
					}
				}
				controllerRecord["id"] = this->_myAddressStr.c_str();
				controllerRecord["publicIdentity"] = publicId;
				//controllerRecord["secretIdentity"] = secretId;
				if (hostname[0])
					controllerRecord["clusterHost"] = hostname;
				controllerRecord["vMajor"] = ZEROTIER_ONE_VERSION_MAJOR;
				controllerRecord["vMinor"] = ZEROTIER_ONE_VERSION_MINOR;
				controllerRecord["vRev"] = ZEROTIER_ONE_VERSION_REVISION;
				controllerRecord["vBuild"] = ZEROTIER_ONE_VERSION_BUILD;
			}

			while (_run == 1) {
				try {
					if (!rdb)
						rdb = R::connect(this->_host,this->_port,this->_auth);
					if (rdb) {
						controllerRecord["lastAlive"] = OSUtils::now();
						//printf("HEARTBEAT: %s" ZT_EOL_S,tmp);
						R::db(this->_db).table("Controller",R::optargs("read_mode","outdated")).insert(controllerRecord,R::optargs("conflict","update")).run(*rdb);
					}
				} catch ( ... ) {
					rdb.reset();
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			}
		} catch ( ... ) {}
	});
}

RethinkDB::~RethinkDB()
{
	_run = 0;
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	_commitQueue.stop();
	for(int t=0;t<ZT_CONTROLLER_RETHINKDB_COMMIT_THREADS;++t)
		_commitThread[t].join();
	if (_membersDbWatcherConnection)
		((R::Connection *)_membersDbWatcherConnection)->close();
	if (_networksDbWatcherConnection)
		((R::Connection *)_networksDbWatcherConnection)->close();
	_membersDbWatcher.join();
	_networksDbWatcher.join();
	_heartbeatThread.join();
	_onlineNotificationThread.join();
}

bool RethinkDB::waitForReady()
{
	while (_ready > 0) {
		if (!_waitNoticePrinted) {
			_waitNoticePrinted = true;
			fprintf(stderr,"[%s] NOTICE: %.10llx controller RethinkDB waiting for initial data download..." ZT_EOL_S,_timestr(),(unsigned long long)_myAddress.toInt());
		}
		_readyLock.lock();
		_readyLock.unlock();
	}
	return true;
}

bool RethinkDB::isReady()
{
	return ((_ready)&&(_connected));
}

void RethinkDB::save(nlohmann::json *orig,nlohmann::json &record)
{
	if (!record.is_object()) // sanity check
		return;
	waitForReady();
	if (orig) {
		if (*orig != record) {
			record["revision"] = OSUtils::jsonInt(record["revision"],0ULL) + 1;
			_commitQueue.post(new nlohmann::json(record));
		}
	} else {
		record["revision"] = 1;
		_commitQueue.post(new nlohmann::json(record));
	}
}

void RethinkDB::eraseNetwork(const uint64_t networkId)
{
	char tmp2[24];
	waitForReady();
	Utils::hex(networkId,tmp2);
	json *tmp = new json();
	(*tmp)["id"] = tmp2;
	(*tmp)["objtype"] = "_delete_network"; // pseudo-type, tells thread to delete network
	_commitQueue.post(tmp);
}

void RethinkDB::eraseMember(const uint64_t networkId,const uint64_t memberId)
{
	char tmp2[24];
	json *tmp = new json();
	waitForReady();
	Utils::hex(networkId,tmp2);
	(*tmp)["nwid"] = tmp2;
	Utils::hex10(memberId,tmp2);
	(*tmp)["id"] = tmp2;
	(*tmp)["objtype"] = "_delete_member"; // pseudo-type, tells thread to delete network
	_commitQueue.post(tmp);
}

void RethinkDB::nodeIsOnline(const uint64_t networkId,const uint64_t memberId,const InetAddress &physicalAddress)
{
	std::lock_guard<std::mutex> l(_lastOnline_l);
	std::pair<int64_t,InetAddress> &i = _lastOnline[std::pair<uint64_t,uint64_t>(networkId,memberId)];
	i.first = OSUtils::now();
	if (physicalAddress)
		i.second = physicalAddress;
}

} // namespace ZeroTier

#endif // ZT_CONTROLLER_USE_RETHINKDB
