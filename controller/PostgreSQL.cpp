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

#ifdef ZT_CONTROLLER_USE_LIBPQ

#include "PostgreSQL.hpp"
#include "EmbeddedNetworkController.hpp"
#include "../version.h"

#include <libpq-fe.h>

using json = nlohmann::json;
namespace {

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

}

using namespace ZeroTier;

PostgreSQL::PostgreSQL(EmbeddedNetworkController *const nc, const Identity &myId, const char *path)
    : DB(nc, myId, path)
    , _ready(0)
	, _connected(1)
    , _run(1)
    , _waitNoticePrinted(false)
{
	_connString = std::string(path);

	_readyLock.lock();
	_heartbeatThread = std::thread(&PostgreSQL::heartbeat, this);
	_membersDbWatcher = std::thread(&PostgreSQL::membersDbWatcher, this);
	_networksDbWatcher = std::thread(&PostgreSQL::networksDbWatcher, this);
	for (int i = 0; i < ZT_CONTROLLER_RETHINKDB_COMMIT_THREADS; ++i) {
		_commitThread[i] = std::thread(&PostgreSQL::commitThread, this);
	}
	_onlineNotificationThread = std::thread(&PostgreSQL::onlineNotificationThread, this);
}

PostgreSQL::~PostgreSQL()
{
	_run = 0;
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	
	_heartbeatThread.join();
	_membersDbWatcher.join();
	_networksDbWatcher.join();
	for (int i = 0; i < ZT_CONTROLLER_RETHINKDB_COMMIT_THREADS; ++i) {
		_commitThread[i].join();
	}
	_onlineNotificationThread.join();

}


bool PostgreSQL::waitForReady()
{
	while (_ready < 2) {
		if (!_waitNoticePrinted) {
			_waitNoticePrinted = true;
			fprintf(stderr, "[%s] NOTICE: %.10llx controller PostgreSQL waiting for initial data download..." ZT_EOL_S, ::_timestr(), (unsigned long long)_myAddress.toInt());
		}
		_readyLock.lock();
		_readyLock.unlock();
	}
	return true;
}

bool PostgreSQL::isReady()
{
	return ((_ready == 2)&&(_connected));
}

void PostgreSQL::save(nlohmann::json *orig, nlohmann::json &record)
{
	if (!record.is_object()) {
		return;
	}
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

void PostgreSQL::eraseNetwork(const uint64_t networkId)
{
	char tmp2[24];
	waitForReady();
	Utils::hex(networkId, tmp2);
	json *tmp = new json();
	(*tmp)["id"] = tmp2;
	(*tmp)["objtype"] = "_delete_network";
	_commitQueue.post(tmp);
}

void PostgreSQL::eraseMember(const uint64_t networkId, const uint64_t memberId) 
{
	char tmp2[24];
	json *tmp = new json();
	Utils::hex(networkId, tmp2);
	(*tmp)["nwid"] = tmp2;
	Utils::hex(memberId, tmp2);
	(*tmp)["id"] = tmp2;
	(*tmp)["objtype"] = "_delete_member";
	_commitQueue.post(tmp);
}

void PostgreSQL::nodeIsOnline(const uint64_t networkId, const uint64_t memberId, const InetAddress &physicalAddress)
{
	std::lock_guard<std::mutex> l(_lastOnline_l);
	std::pair<int64_t, InetAddress> &i = _lastOnline[std::pair<uint64_t,uint64_t>(networkId, memberId)];
	i.first = OSUtils::now();
	if (physicalAddress) {
		i.second = physicalAddress;
	}
}

void PostgreSQL::initializeNetworks(PGconn *conn)
{
	if (PQstatus(conn) != CONNECTION_OK) {
		fprintf(stderr, "Bad Database Connection: %s", PQerrorMessage(conn));
		exit(1);
	}

	const char *params[1] = {
		_myAddressStr.c_str()
	};

	PGresult *res = PQexecParams(conn, "SELECT id, EXTRACT(EPOCH FROM creation_time AT TIME ZONE 'UTC')*1000, capabilities, "
		"enable_broadcast, EXTRACT(EPOCH FROM last_modified AT TIME ZONE 'UTC')*1000, mtu, multicast_limit, name, private, remote_trace_level, "
		"remote_trace_target, revision, rules, tags, v4_assign_mode, v6_assign_mode FROM ztc_network "
		"WHERE deleted = false AND controller_id = $1",
		1,
		NULL,
		params,
		NULL,
		NULL,
		0);
	
	if (PQresultStatus(res) != PGRES_TUPLES_OK) {
		fprintf(stderr, "Networks Initialization Failed: %s", PQerrorMessage(conn));
		PQclear(res);
		exit(1);
	}

	int numRows = PQntuples(res);
	for (int i = 0; i < numRows; ++i) {
		json empty;
		json config;
		config["nwid"] = PQgetvalue(res, i, 0);
		config["creationTime"] = std::stoull(PQgetvalue(res, i, 1));
		config["capabilities"] = json::parse(PQgetvalue(res, i, 2));
		config["enableBroadcast"] = (strcmp(PQgetvalue(res, i, 3),"true")==0);
		config["lastModified"] = std::stoull(PQgetvalue(res, i, 4));
		config["mtu"] = std::stoi(PQgetvalue(res, i, 5));
		config["multicastLimit"] = std::stoi(PQgetvalue(res, i, 6));
		config["name"] = PQgetvalue(res, i, 7);
		config["private"] = (strcmp(PQgetvalue(res, i, 8),"true")==0);
		config["remoteTraceLevel"] = std::stoi(PQgetvalue(res, i, 9));
		config["remoteTraceTarget"] = PQgetvalue(res, i, 10);
		config["revision"] = std::stoull(PQgetvalue(res, i, 11));
		config["rules"] = json::parse(PQgetvalue(res, i, 12));
		config["tags"] = json::parse(PQgetvalue(res, i, 13));
		config["v4AssignMode"] = json::parse(PQgetvalue(res, i, 14));
		config["v6AssignMode"] = json::parse(PQgetvalue(res, i, 15));
		
		_networkChanged(empty, config, false);
	}

	PQclear(res);

	if (++this->_ready == 2) {
		if (_waitNoticePrinted) {
			fprintf(stderr,"[%s] NOTICE: %.10llx controller RethinkDB data download complete." ZT_EOL_S,_timestr(),(unsigned long long)_myAddress.toInt());
		}
		_readyLock.unlock();
	}
}

void PostgreSQL::initializeMembers(PGconn *conn)
{
	// TODO: do stuff here

	if (++this->_ready == 2) {
		if (_waitNoticePrinted) {
			fprintf(stderr,"[%s] NOTICE: %.10llx controller RethinkDB data download complete." ZT_EOL_S,_timestr(),(unsigned long long)_myAddress.toInt());
		}
		_readyLock.unlock();
	}
}

void PostgreSQL::heartbeat()
{
	char publicId[1024];
	char hostnameTmp[1024];
	_myId.toString(false,publicId);
	if (gethostname(hostnameTmp, sizeof(hostnameTmp))!= 0) {
		hostnameTmp[0] = (char)0;
	} else {
		for (int i = 0; i < sizeof(hostnameTmp); ++i) {
			if ((hostnameTmp[i] == '.')||(hostnameTmp[i] == 0)) {
				hostnameTmp[i] = (char)0;
				break;
			}
		}
	}
	const char *controllerId = _myAddressStr.c_str();
	const char *publicIdentity = publicId;
	const char *hostname = hostnameTmp;

	PGconn *conn = PQconnectdb(_path.c_str());
	if (PQstatus(conn) == CONNECTION_BAD) {
		fprintf(stderr, "Connection to database failed: %s\n", PQerrorMessage(conn));
		PQfinish(conn);
		exit(1);
	}
	while (_run == 1) {
		if(PQstatus(conn) != CONNECTION_OK) {
			PQfinish(conn);
			conn = PQconnectdb(_path.c_str());
		}
		if (conn) {
			const char *values[8] = {
				controllerId,
				hostname,
				std::to_string(OSUtils::now()).c_str(),
				publicIdentity,
				std::to_string(ZEROTIER_ONE_VERSION_MAJOR).c_str(),
				std::to_string(ZEROTIER_ONE_VERSION_MINOR).c_str(),
				std::to_string(ZEROTIER_ONE_VERSION_REVISION).c_str(),
				std::to_string(ZEROTIER_ONE_VERSION_BUILD).c_str()
			};
			int lengths[8] = {
				(int)strlen(values[0]),
				(int)strlen(values[1]),
				(int)strlen(values[2]),
				(int)strlen(values[3]),
				(int)strlen(values[4]),
				(int)strlen(values[5]),
				(int)strlen(values[6]),
				(int)strlen(values[7])
			};
			int binary[8] = {0,0,0,0,0,0,0,0};

			PGresult *res = PQexecParams(conn,
				"INSERT INTO ztc_controller (id, cluster_host, last_alive, public_identity, v_major, v_minor, v_rev, v_build) " 
				"VALUES ($1, $2, TO_TIMESTAMP($3::double precision/1000), $4, $5, $6, $7, $8) "
				"ON CONFLICT (id) DO UPDATE SET cluster_host = EXCLUDED.cluster_host, last_alive = EXCLUDED.last_alive, "
				"public_identity = EXCLUDED.public_identity, v_major = EXCLUDED.v_major, v_minor = EXCLUDED.v_minor, "
				"v_rev = EXCLUDED.v_rev, v_build = EXCLUDED.v_rev",
				8,       // number of parameters
				NULL,    // oid field.   ignore
				values,  // values for substitution
				lengths, // lengths in bytes of each value
				binary,  // binary?
				0);

			if (PQresultStatus(res) != PGRES_COMMAND_OK) {
				fprintf(stderr, "Heartbeat Update Failed: %s\n", PQresultErrorMessage(res));
			}
			PQclear(res);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}

	PQfinish(conn);
	conn = NULL;
}

void PostgreSQL::membersDbWatcher()
{
	PGconn *conn = PQconnectdb(_path.c_str());
	if (PQstatus(conn) == CONNECTION_BAD) {
		fprintf(stderr, "Connection to database failed: %s\n", PQerrorMessage(conn));
		PQfinish(conn);
		exit(1);
	}

	initializeMembers(conn);

	char buf[11] = {0};
	std::string cmd = "LISTEN member_" + std::string(_myAddress.toString(buf));
	PGresult *res = PQexec(conn, cmd.c_str());
	if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
		fprintf(stderr, "LISTEN command failed: %s\n", PQresultErrorMessage(res));
		PQclear(res);
		PQfinish(conn);
		exit(1);
	}

	PQclear(res); res = NULL;

	while(_run == 1) {
		if (PQstatus(conn) != CONNECTION_OK) {
			fprintf(stderr, "ERROR: Member Watcher lost connection to Postgres.");
			exit(-1);
		}
		PGnotify *notify = NULL;
		PQconsumeInput(conn);
		while ((notify = PQnotifies(conn)) != NULL) {
			fprintf(stderr, "ASYNC NOTIFY of '%s' id:%s received\n", notify->relname, notify->extra);

			try {
				json tmp(json::parse(notify->extra));
				json &ov = tmp["old_val"];
				json &nv = tmp["new_val"];
				json oldConfig, newConfig;
				if (ov.is_object()) oldConfig = ov;
				if (nv.is_object()) newConfig = nv;
				if (oldConfig.is_object() || newConfig.is_object()) {
					_memberChanged(oldConfig,newConfig,(this->_ready>=2));
				}
			} catch (...) {} // ignore bad records

			free(notify);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	PQfinish(conn);
	conn = NULL;
}

void PostgreSQL::networksDbWatcher()
{
	PGconn *conn = PQconnectdb(_path.c_str());
	if (PQstatus(conn) == CONNECTION_BAD) {
		fprintf(stderr, "Connection to database failed: %s\n", PQerrorMessage(conn));
		PQfinish(conn);
		exit(1);
	}

	initializeNetworks(conn);

	char buf[11] = {0};
	std::string cmd = "LISTEN network_" + std::string(_myAddress.toString(buf));
	PGresult *res = PQexec(conn, cmd.c_str());
	if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
		fprintf(stderr, "LISTEN command failed: %s\n", PQresultErrorMessage(res));
		PQclear(res);
		PQfinish(conn);
		exit(1);
	}

	PQclear(res); res = NULL;

	while(_run == 1) {
		if (PQstatus(conn) != CONNECTION_OK) {
			fprintf(stderr, "ERROR: Network Watcher lost connection to Postgres.");
			exit(-1);
		}
		PGnotify *notify = NULL;
		PQconsumeInput(conn);
		while ((notify = PQnotifies(conn)) != NULL) {
			fprintf(stderr, "ASYNC NOTIFY of '%s' id:%s received\n", notify->relname, notify->extra);
			try {
				json tmp(json::parse(notify->extra));
				json &ov = tmp["old_val"];
				json &nv = tmp["new_val"];
				json oldConfig, newConfig;
				if (ov.is_object()) oldConfig = ov;
				if (nv.is_object()) newConfig = nv;
				if (oldConfig.is_object()||newConfig.is_object()) {
					_networkChanged(oldConfig,newConfig,(this->_ready >= 2));
				}
			} catch (...) {} // ignore bad records
			free(notify);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	PQfinish(conn);
	conn = NULL;
}

void PostgreSQL::commitThread()
{
	json *config = nullptr;
	while(_commitQueue.get(config)&(_run == 1)) {
		if (!config) {
			continue;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

void PostgreSQL::onlineNotificationThread()
{
	PGconn *conn = PQconnectdb(_path.c_str());
	if (PQstatus(conn) == CONNECTION_BAD) {
		fprintf(stderr, "Connection to database failed: %s\n", PQerrorMessage(conn));
		PQfinish(conn);
		exit(1);
	}
	_connected = 1;

	int64_t	lastUpdatedNetworkStatus = 0;
	std::unordered_map< std::pair<uint64_t,uint64_t>,int64_t,_PairHasher > lastOnlineCumulative;
	while (_run == 1) {
		if (PQstatus(conn) != CONNECTION_OK) {
			fprintf(stderr, "ERROR: Online Notification thread lost connection to Postgres.");
			exit(-1);
		}

		std::unordered_map< std::pair<uint64_t,uint64_t>,std::pair<int64_t,InetAddress>,_PairHasher > lastOnline;
		{
			std::lock_guard<std::mutex> l(_lastOnline_l);
			lastOnline.swap(_lastOnline);
		}

		PGresult *res = NULL;
		int qCount = 0;

		if (!lastOnline.empty()) {
			fprintf(stderr, "Last Online Update\n");
			res = PQexec(conn, "BEGIN");
			if (PQresultStatus(res) != PGRES_COMMAND_OK) {
				fprintf(stderr, "ERROR: Error on BEGIN command (onlineNotificationThread): %s\n", PQresultErrorMessage(res));
				PQclear(res);
				exit(1);
			}
			PQclear(res);
		}

		for (auto i=lastOnline.begin(); i != lastOnline.end(); ++i) {
			lastOnlineCumulative[i->first] = i->second.first;
			char nwidTmp[64];
			char memTmp[64];
			char ipTmp[64];
			OSUtils::ztsnprintf(nwidTmp,sizeof(nwidTmp), "%.16llx", i->first.first);
			OSUtils::ztsnprintf(memTmp,sizeof(memTmp), "%.10llx", i->first.second);

			std::string networkId(nwidTmp);
			std::string memberId(memTmp);
			int64_t ts = i->second.first;
			std::string ipAddr = i->second.second.toIpString(ipTmp);

			const char *values[4] = {
				networkId.c_str(),
				memberId.c_str(),
				std::to_string(ts).c_str(),
				ipAddr.c_str()
			};

			res = PQexecParams(conn,
				"INSERT INTO ztc_member_status (network_id, member_id, address, last_updated) VALUES ($1, $2, $3, $4)"
				"ON CONFLICT (network_id, member_id) DO UPDATE SET address = EXCLUDED.address, last_updated = EXCLUDED.last_updated",
				8,       // number of parameters
				NULL,    // oid field.   ignore
				values,  // values for substitution
				NULL, // lengths in bytes of each value
				NULL,
				0);

			if (PQresultStatus(res) != PGRES_COMMAND_OK) {
				fprintf(stderr, "Error on Member Status upsert: %s\n", PQresultErrorMessage(res));
				PQclear(res);
				PQexec(conn, "ROLLBACK");
				exit(1);
			}

			PQclear(res);

			if ((++qCount) == 1024) {
				res = PQexec(conn, "COMMIT");
				if (PQresultStatus(res) != PGRES_COMMAND_OK) {
					fprintf(stderr, "ERROR: Error on commit (onlineNotificationThread): %s\n", PQresultErrorMessage(res));
					PQclear(res);
					PQexec(conn, "ROLLBACK");
					exit(1);
				}
				PQclear(res);

				res = PQexec(conn, "BEGIN");
				if (PQresultStatus(res) != PGRES_COMMAND_OK) {
					fprintf(stderr, "ERROR: Error on BEGIN (onlineNotificationThread): %s\n", PQresultErrorMessage(res));
					PQclear(res);
					exit(1);
				}
				PQclear(res);
				qCount = 0;
			}
		}
		if (qCount > 0) {
			fprintf(stderr, "qCount is %d\n", qCount);
			res = PQexec(conn, "COMMIT");
			if (PQresultStatus(res) != PGRES_COMMAND_OK) {
				fprintf(stderr, "ERROR: Error on commit (onlineNotificationThread): %s\n", PQresultErrorMessage(res));
				PQclear(res);
				PQexec(conn, "ROLLBACK");
				exit(1);
			}
			PQclear(res);
		}

		const int64_t now = OSUtils::now();
		if ((now - lastUpdatedNetworkStatus) > 10000) {
			lastUpdatedNetworkStatus = now;

			std::vector<std::pair<uint64_t, std::shared_ptr<_Network>>> networks;
			{
				std::lock_guard<std::mutex> l(_networks_l);
				for (auto i = _networks.begin(); i != _networks.end(); ++i) {
					networks.push_back(*i);
				}
			}

			int nCount = 0;
			if (!networks.empty()) {
				fprintf(stderr, "Network update");
				res = PQexec(conn, "BEGIN");
				if (PQresultStatus(res) != PGRES_COMMAND_OK) {
					fprintf(stderr, "ERROR: Error on BEGIN command (onlineNotificationThread): %s\n", PQresultErrorMessage(res));
					PQclear(res);
					exit(1);
				}
				PQclear(res);
			}
			for (auto i = networks.begin(); i != networks.end(); ++i) {
				char tmp[64];
				Utils::hex(i->first, tmp);

				std::string networkId(tmp);
				uint64_t authMemberCount = 0;
				uint64_t totalMemberCount = 0;
				uint64_t onlineMemberCount = 0;
				uint64_t bridgeCount = 0;
				uint64_t ts = now;
				{
					std::lock_guard<std::mutex> l2(i->second->lock);
					authMemberCount = i->second->authorizedMembers.size();
					totalMemberCount = i->second->members.size();
					bridgeCount = i->second->activeBridgeMembers.size();
					for (auto m=i->second->members.begin(); m != i->second->members.end(); ++m) {
						auto lo = lastOnlineCumulative.find(std::pair<uint64_t,uint64_t>(i->first, m->first));
						if (lo != lastOnlineCumulative.end()) {
							if ((now - lo->second) <= (ZT_NETWORK_AUTOCONF_DELAY * 2)) {
								++onlineMemberCount;
							} else {
								lastOnlineCumulative.erase(lo);
							}
						}
					}
				}

				const char *values[6] = {
					networkId.c_str(),
					std::to_string(bridgeCount).c_str(),
					std::to_string(authMemberCount).c_str(),
					std::to_string(onlineMemberCount).c_str(),
					std::to_string(totalMemberCount).c_str(),
					std::to_string(ts).c_str()
				};

				res = PQexecParams(conn, "INSERT INTO ztc_network_status (network_id, bridge_count, authorized_member_count, "
					"online_member_count, total_member_count, last_modified) VALUES ($1, $2, $3, $4, $5, $6) "
					"ON CONFLICT (network_id) DO UPDATE SET bridge_count = EXCLUDED.bridge_count, "
					"authorized_member_count = EXCLUDED.authorized_member_count, online_member_count = EXCDLUDED.online_member_count, "
					"total_member_count = EXCLUDED.total_member_count, last_modified = EXCLUDED.last_modified",
					6,
					NULL,
					values,
					NULL,
					NULL,
					0);
				
				if (PQresultStatus(res) != PGRES_COMMAND_OK) {
					fprintf(stderr, "ERROR: Error on Network Satus upsert (onlineNotificationThread): %s\n", PQresultErrorMessage(res));
					PQclear(res);
					PQexec(conn, "ROLLBACK");
					exit(1);
				}

				if ((++nCount) == 1024) {
					res = PQexec(conn, "COMMIT");
					if (PQresultStatus(res) != PGRES_COMMAND_OK) {
						fprintf(stderr, "ERROR: Error on COMMIT (onlineNotificationThread): %s\n" , PQresultErrorMessage(res));
						PQclear(res);
						PQexec(conn, "ROLLBACK");
						exit(1);
					}

					res = PQexec(conn, "BEGIN");
					if (PQresultStatus(res) != PGRES_COMMAND_OK) {
						fprintf(stderr, "ERROR: Error on BEGIN command (onlineNotificationThread): %s\n", PQresultErrorMessage(res));
						PQclear(res);
						exit(1);
					}

					nCount = 0;
				}
			}

			if (nCount > 0) {
				fprintf(stderr, "nCount is %d\n", nCount);
				res = PQexec(conn, "COMMIT");
				if (PQresultStatus(res) != PGRES_COMMAND_OK) {
					fprintf(stderr, "ERROR: Error on COMMIT (onlineNotificationThread): %s\n", PQresultErrorMessage(res));
					PQclear(res);
					PQexec(conn, "ROLLBACK");
					exit(1);
				}
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(250));
	}
	PQfinish(conn);
}
#endif //ZT_CONTROLLER_USE_LIBPQ