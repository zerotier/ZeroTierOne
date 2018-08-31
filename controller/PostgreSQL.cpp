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
	// TODO: do stuff here

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
	_connected = 1;

	while (_run == 1) {
		std::this_thread::sleep_for(std::chrono::milliseconds(250));
	}
}
#endif //ZT_CONTROLLER_USE_LIBPQ