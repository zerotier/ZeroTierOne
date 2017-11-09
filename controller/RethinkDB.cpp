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

#ifdef ZT_CONTROLLER_USE_RETHINKDB

#include "RethinkDB.hpp"
#include "EmbeddedNetworkController.hpp"

#include <chrono>
#include <algorithm>
#include <stdexcept>

#include "../ext/librethinkdbxx/build/include/rethinkdb.h"

namespace R = RethinkDB;
using json = nlohmann::json;

namespace ZeroTier {

RethinkDB::RethinkDB(EmbeddedNetworkController *const nc,const Address &myAddress,const char *path) :
	DB(nc,myAddress,path),
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
								if (--this->_ready == 0) {
									if (_waitNoticePrinted)
										fprintf(stderr,"NOTICE: controller RethinkDB data download complete." ZT_EOL_S);
									this->_readyLock.unlock();
								}
							} else {
								try {
									json &ov = tmp["old_val"];
									json &nv = tmp["new_val"];
									if (ov.is_object()||nv.is_object()) {
										//if (nv.is_object()) printf("MEMBER: %s" ZT_EOL_S,nv.dump().c_str());
										this->_memberChanged(ov,nv,(this->_ready <= 0));
									}
								} catch ( ... ) {} // ignore bad records
							}
						}
					}
				} catch (std::exception &e) {
					fprintf(stderr,"ERROR: controller RethinkDB (member change stream): %s" ZT_EOL_S,e.what());
				} catch (R::Error &e) {
					fprintf(stderr,"ERROR: controller RethinkDB (member change stream): %s" ZT_EOL_S,e.message.c_str());
				} catch ( ... ) {
					fprintf(stderr,"ERROR: controller RethinkDB (member change stream): unknown exception" ZT_EOL_S);
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
										fprintf(stderr,"NOTICE: controller RethinkDB data download complete." ZT_EOL_S);
									this->_readyLock.unlock();
								}
							} else {
								try {
									json &ov = tmp["old_val"];
									json &nv = tmp["new_val"];
									if (ov.is_object()||nv.is_object()) {
										//if (nv.is_object()) printf("NETWORK: %s" ZT_EOL_S,nv.dump().c_str());
										this->_networkChanged(ov,nv,(this->_ready <= 0));
									}
								} catch ( ... ) {} // ignore bad records
							}
						}
					}
				} catch (std::exception &e) {
					fprintf(stderr,"ERROR: controller RethinkDB (network change stream): %s" ZT_EOL_S,e.what());
				} catch (R::Error &e) {
					fprintf(stderr,"ERROR: controller RethinkDB (network change stream): %s" ZT_EOL_S,e.message.c_str());
				} catch ( ... ) {
					fprintf(stderr,"ERROR: controller RethinkDB (network change stream): unknown exception" ZT_EOL_S);
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
					json record;
					const std::string objtype = (*config)["objtype"];
					const char *table;
					std::string deleteId;
					try {
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
						} else {
							delete config;
							continue;
						}
						delete config;
					} catch ( ... ) {
						delete config;
						continue;
					}

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
									R::db(this->_db).table(table).insert(R::Datum::from_json(record.dump()),R::optargs("conflict","update","return_changes",false)).run(*rdb);
								}
								break;
							} else {
								fprintf(stderr,"ERROR: controller RethinkDB (insert/update): connect failed (will retry)" ZT_EOL_S);
							}
						} catch (std::exception &e) {
							fprintf(stderr,"ERROR: controller RethinkDB (insert/update): %s" ZT_EOL_S,e.what());
							rdb.reset();
						} catch (R::Error &e) {
							fprintf(stderr,"ERROR: controller RethinkDB (insert/update): %s" ZT_EOL_S,e.message.c_str());
							rdb.reset();
						} catch ( ... ) {
							fprintf(stderr,"ERROR: controller RethinkDB (insert/update): unknown exception" ZT_EOL_S);
							rdb.reset();
						}
						std::this_thread::sleep_for(std::chrono::milliseconds(250));
					}
				}
			} catch ( ... ) {}
		});
	}

	_onlineNotificationThread = std::thread([this]() {
		try {
			std::unique_ptr<R::Connection> rdb;
			while (_run == 1) {
				try {
					if (!rdb)
						rdb = R::connect(this->_host,this->_port,this->_auth);
					if (rdb) {
						std::lock_guard<std::mutex> l(_lastOnline_l);
						R::Array batch;
						R::Object tmpobj;
						for(auto i=_lastOnline.begin();i!=_lastOnline.end();++i) {
							char tmp[64];
							OSUtils::ztsnprintf(tmp,sizeof(tmp),"%.16llx-%.10llx",i->first.first,i->first.second);
							tmpobj["id"] = tmp;
							tmpobj["ts"] = i->second;
							batch.emplace_back(tmpobj);
							if (batch.size() >= 256) {
								R::db(this->_db).table("MemberLastRequest",R::optargs("read_mode","outdated")).insert(batch,R::optargs("conflict","update")).run(*rdb);
								batch.clear();
							}
						}
						if (batch.size() > 0)
							R::db(this->_db).table("MemberLastRequest",R::optargs("read_mode","outdated")).insert(batch,R::optargs("conflict","update")).run(*rdb);
						_lastOnline.clear();
					}
				} catch (std::exception &e) {
					fprintf(stderr,"ERROR: controller RethinkDB (node status update): %s" ZT_EOL_S,e.what());
					rdb.reset();
				} catch (R::Error &e) {
					fprintf(stderr,"ERROR: controller RethinkDB (node status update): %s" ZT_EOL_S,e.message.c_str());
					rdb.reset();
				} catch ( ... ) {
					fprintf(stderr,"ERROR: controller RethinkDB (node status update): unknown exception" ZT_EOL_S);
					rdb.reset();
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(250));
			}
		} catch ( ... ) {}
	});

	_heartbeatThread = std::thread([this]() {
		try {
			char tmp[1024];
			std::unique_ptr<R::Connection> rdb;
			while (_run == 1) {
				try {
					if (!rdb)
						rdb = R::connect(this->_host,this->_port,this->_auth);
					if (rdb) {
						OSUtils::ztsnprintf(tmp,sizeof(tmp),"{\"id\":\"%s\",\"lastAlive\":%lld}",this->_myAddressStr.c_str(),(long long)OSUtils::now());
						//printf("HEARTBEAT: %s" ZT_EOL_S,tmp);
						R::db(this->_db).table("Controller").update(R::Datum::from_json(tmp)).run(*rdb);
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
			fprintf(stderr,"NOTICE: controller RethinkDB waiting for initial data download..." ZT_EOL_S);
		}
		_readyLock.lock();
		_readyLock.unlock();
	}
	return true;
}

void RethinkDB::save(nlohmann::json *orig,nlohmann::json &record)
{
	if (!record.is_object()) // sanity check
		return;
	waitForReady();
	if (orig) {
		if (*orig != record) {
			nlohmann::json *q = new nlohmann::json();
			try {
				record["revision"] = OSUtils::jsonInt(record["revision"],0ULL) + 1;
				for(auto kv=record.begin();kv!=record.end();++kv) {
					if ((kv.key() == "id")||(kv.key() == "nwid")||(kv.key() == "objtype")||((*q)[kv.key()] != kv.value()))
						(*q)[kv.key()] = kv.value();
				}
			} catch ( ... ) {
				delete q;
				throw;
			}
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

void RethinkDB::nodeIsOnline(const uint64_t networkId,const uint64_t memberId)
{
	std::lock_guard<std::mutex> l(_lastOnline_l);
	_lastOnline[std::pair<uint64_t,uint64_t>(networkId,memberId)] = OSUtils::now();
}

} // namespace ZeroTier

#endif // ZT_CONTROLLER_USE_RETHINKDB
