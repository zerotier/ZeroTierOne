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

#include <pqxx/pqxx>
#include <sstream>

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

std::string join(const std::vector<std::string> &elements, const char * const separator)
{
	switch(elements.size()) {
	case 0:
		return "";
	case 1:
		return elements[0];
	default:
		std::ostringstream os;
		std::copy(elements.begin(), elements.end()-1, std::ostream_iterator<std::string>(os, separator));
		os << *elements.rbegin();
		return os.str();
	}
}

}

namespace ZeroTier {

class _MemberNotificationReceiver : public pqxx::notification_receiver
{
private:
	ZeroTier::PostgreSQL *_pgsql;

public:
	_MemberNotificationReceiver(pqxx::connection_base &c, const std::string &channel, ZeroTier::PostgreSQL *pgsql)
		: pqxx::notification_receiver(c, channel)
		, _pgsql(pgsql)
	{}

	virtual void operator()(const std::string &payload, int backend_pid)
	{
		try {
			json tmp(json::parse(payload));
			json &ov = tmp["old_val"];
			json &nv = tmp["new_val"];
			json oldConfig, newConfig;
			if (ov.is_object()) oldConfig = ov;
			if (nv.is_object()) newConfig = nv;
			if (oldConfig.is_object() || newConfig.is_object()) {
				_pgsql->_memberChanged(oldConfig,newConfig,_pgsql->isReady());
			}
		} catch (std::exception &e) {
			fprintf(stderr, "Exception parsing member notification: %s\n", e.what());
		}
	}
};

class _NetworkNotificationReceiver : public pqxx::notification_receiver
{
private:
	ZeroTier::PostgreSQL *_pgsql;
public:
	_NetworkNotificationReceiver(pqxx::connection_base &c, const std::string &channel, ZeroTier::PostgreSQL *pgsql)
		: pqxx::notification_receiver(c, channel)
		, _pgsql(pgsql)
	{}

	virtual void operator()(const std::string &payload, int backend_pid)
	{
		try {
			json tmp(json::parse(payload));
			json &ov = tmp["old_val"];
			json &nv = tmp["new_val"];
			json oldConfig, newConfig;
			if (ov.is_object()) oldConfig = ov;
			if (nv.is_object()) newConfig = nv;
			if (oldConfig.is_object()||newConfig.is_object()) {
				_pgsql->_networkChanged(oldConfig,newConfig,_pgsql->isReady());
			}
		} catch (std::exception &e) {
			fprintf(stderr, "Exception parsing member notification: %s\n", e.what());
		} // ignore bad records
	}
};

} // namespace ZeroTier

using namespace ZeroTier;

PostgreSQL::PostgreSQL(EmbeddedNetworkController *const nc, const Identity &myId, const char *path)
    : DB(nc, myId, path)
    , _ready(0)
	, _connected(1)
    , _run(1)
    , _waitNoticePrinted(false)
{
	fprintf(stderr, "PostgreSQL Constructed");
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
	try {
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
	} catch (std::exception &e) {
		fprintf(stderr, "Error on PostgreSQL::save: %s\n", e.what());
	} catch (...) {
		fprintf(stderr, "Unknown error on PostgreSQL::save\n");
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

void PostgreSQL::initializeNetworks(pqxx::connection &conn)
{
	try {
		if (!conn.is_open()) {
			fprintf(stderr, "Bad Database Connection in initializeNetworks\n");
			exit(1);
		}

		pqxx::work w(conn);

		pqxx::result r = w.exec("SELECT id, EXTRACT(EPOCH FROM creation_time AT TIME ZONE 'UTC')*1000 AS creation_time, capabilities, "
			"enable_broadcast, EXTRACT(EPOCH FROM last_modified AT TIME ZONE 'UTC')*1000 AS last_modified, mtu, multicast_limit, name, private, remote_trace_level, "
			"remote_trace_target, revision, rules, tags, v4_assign_mode, v6_assign_mode FROM ztc_network "
			"WHERE deleted = false AND controller_id = '" + w.esc(_myAddressStr) + "'");


		for (pqxx::result::const_iterator row = r.begin(); row != r.end(); ++row) {
			json empty;
			json config;

			std::string nwid = row["id"].as<std::string>();
			config["id"] = nwid;
			config["nwid"] = nwid;
			try {
				config["creationTime"] = row["creation_time"].as<uint64_t>();
			} catch(std::exception &e) {
				config["creationTime"] = 0ULL;
			}
			config["capabilities"] = json::parse(row["capabilities"].as<std::string>());
			config["enableBroadcast"] = row["enable_broadcast"].as<bool>();
			try {
				config["lastModified"] = row["last_modified"].as<uint64_t>();
			} catch (std::exception &e) {
				config["lastModified"] = 0ULL;
			}
			try {
				config["mtu"] = row["mtu"].as<int>();
			} catch (std::exception &e) {
				config["mtu"] = 2800;
			}
			try {
				config["multicastLimit"] = row["multicast_limit"].as<int>();
			} catch (std::exception &e) {
				config["multicastLimit"] = 64;
			}
			config["name"] = row["name"].as<std::string>();
			config["private"] = row["private"].as<bool>();
			try {
				config["remoteTraceLevel"] = row["remote_trace_level"].as<int>();
			} catch (std::exception &e) {
				config["remoteTraceLevel"] = 0;
			}
			config["remoteTraceTarget"] = (row["remote_trace_target"].is_null() ? nullptr : row["remote_trace_target"].as<std::string>());
			try {
				config["revision"] = row["revision"].as<uint64_t>();
			} catch (std::exception &e) {
				config["revision"] = 0ULL;
				//fprintf(stderr, "Error converting revision: %s\n", PQgetvalue(res, i, 11));
			}
			config["rules"] = json::parse(row["rules"].as<std::string>());
			config["tags"] = json::parse(row["tags"].as<std::string>());
			config["v4AssignMode"] = json::parse(row["v4_assign_mode"].as<std::string>());
			config["v6AssignMode"] = json::parse(row["v6_assign_mode"].as<std::string>());
			config["objtype"] = "network";
			config["ipAssignmentPools"] = json::array();
			config["routes"] = json::array();

			pqxx::work w2(conn);
			pqxx::result res2 = w2.exec("SELECT host(ip_range_start) AS ip_range_start, host(ip_range_end) AS ip_range_end FROM ztc_network_assignment_pool WHERE network_id = '" + w2.esc(nwid) + "'");
			for(pqxx::result::const_iterator it = res2.begin(); it != res2.end(); ++it) {
				json ip;
				ip["ipRangeStart"] = it["ip_range_start"].as<std::string>();
				ip["ipRangeEnd"] = it["ip_range_end"].as<std::string>();
				config["ipAssignmentPools"].push_back(ip);
			}
			w2.commit();

			pqxx::work w3(conn);
			pqxx::result res3 = w3.exec("SELECT host(address) AS address, bits, host(via) AS via FROM ztc_network_route WHERE network_id = '" + w3.esc(nwid) + "'");
			for(pqxx::result::const_iterator it = res3.begin(); it != res3.end(); ++it) {
				json route;
				route["target"] = it["address"].as<std::string>() + "/" + it["bits"].as<std::string>();
				if (route["via"].is_null()) {
					route["via"] = nullptr;
				} else {
					route["via"] = it["via"].as<std::string>();
				}
				config["routes"].push_back(route);
			}
			w3.commit();

			_networkChanged(empty, config, false);
		}

		w.commit();

		if (++this->_ready == 2) {
			if (_waitNoticePrinted) {
				fprintf(stderr,"[%s] NOTICE: %.10llx controller PostgreSQL data download complete." ZT_EOL_S,_timestr(),(unsigned long long)_myAddress.toInt());
			}
			_readyLock.unlock();
		}
	} catch (std::exception &e) {
		fprintf(stderr, "ERROR: Error initializing networks: %s", e.what());
		exit(-1);
	}
}

void PostgreSQL::initializeMembers(pqxx::connection &conn)
{
	try {
		if (!conn.is_open()) {
			fprintf(stderr, "Bad Database Connection in initializeMembers\n");
			exit(1);
		}

		pqxx::work w(conn);
		pqxx::result res = w.exec(
			"SELECT m.id AS id, m.network_id AS network_id, m.active_bridge AS active_bridge, "
			"   m.authorized AS authorized, m.capabilities AS capabilities, "
			"   EXTRACT(EPOCH FROM m.creation_time AT TIME ZONE 'UTC')*1000 AS creation_time, m.identity AS identity, "
			"	EXTRACT(EPOCH FROM m.last_authorized_time AT TIME ZONE 'UTC')*1000 AS last_authorized_time, "
			"	EXTRACT(EPOCH FROM m.last_deauthorized_time AT TIME ZONE 'UTC')*1000 AS last_deauthorized_time, "
			"	m.remote_trace_level AS remote_trace_level, m.remote_trace_target AS remote_trace_target, "
			"   m.tags AS tags, m.v_major AS v_major, m.v_minor AS v_minor, m.v_rev AS v_rev, "
			"   m.v_proto AS v_proto, m.no_auto_assign_ips AS no_auto_assign_ips, m.revision AS revision"
			"FROM ztc_member m "
			"INNER JOIN ztc_network n "
			"	ON n.id = m.network_id "
			"WHERE n.controller_id = '" + w.esc(_myAddressStr) + "' AND m.deleted = false"
		);

		for(pqxx::result::const_iterator row = res.begin(); row != res.end(); ++row) {
			json empty;
			json config;

			std::string networkId = row["network_id"].as<std::string>();
			std::string memberId = row["id"].as<std::string>();

			config["id"] = memberId;
			config["nwid"] = networkId;
			config["activeBridge"] = row["active_bridge"].as<bool>();
			config["authorized"] = row["authorized"].as<bool>();
			try {
				config["capabilities"] = json::parse(row["capabilities"].as<std::string>());
			} catch(std::exception &e) {
				config["capabilities"] = json::array();
			}
			try {
				config["creationTime"] = row["creation_time"].as<uint64_t>();
			} catch(std::exception &e) {
				config["creationTime"] = 0ULL;
			}
			config["identity"] = row["identity"].as<std::string>();
			try {
				config["lastAuthorizedTime"] = row["last_authorized_time"].as<uint64_t>();
			} catch(std::exception &e) {
				config["lastAuthorizedTime"] = 0ULL;
			}
			try {
				config["lastDeauthorizedTime"] = row["last_deauthorized_time"].as<uint64_t>();
			} catch(std::exception &e) {
				config["lastDeauthorizedTime"] = 0ULL;
			}
			try {
				config["remoteTraceLevel"] = row["remote_trace_level"].as<int>();
			} catch(std::exception &e) {
				config["remoteTraceLevel"] = 0;
			}
			config["remoteTraceTarget"] = (row["remote_trace_target"].is_null() ? nullptr : row["remote_trace_target"].as<std::string>());
			try {
				config["tags"] = json::parse(row["tags"].as<std::string>());
			} catch(std::exception &e) {
				config["tags"] = json::array();
			}
			try {
				config["vMajor"] = row["v_major"].as<int>();
			} catch(std::exception &e) {
				config["vMajor"] = -1;
			}
			try {
				config["vMinor"] = row["v_minor"].as<int>();
			} catch (std::exception &e) {
				config["vMinor"] = -1;
			}
			try {
				config["vRev"] = row["v_rev"].as<int>();
			} catch (std::exception &e) {
				config["vRev"] = -1;
			}
			try {
				config["vProto"] = row["v_proto"].as<int>();
			} catch (std::exception &e) {
				config["vProto"] = -1;
			}
			config["noAutoAssignIps"] = row["no_auto_assign_ips"].as<bool>();
			try {
				config["revision"] = row["revision"].as<uint64_t>();
			} catch (std::exception &e) {
				config["revision"] = 0ULL;
				//fprintf(stderr, "Error updating revision (member): %s\n", PQgetvalue(res, i, 17));
			}
			config["objtype"] = "member";
			config["ipAssignments"] = json::array();

			pqxx::work w2(conn);
			pqxx::result r2 = w2.exec(
				"SELECT address FROM ztc_member_ip_assignment WHERE member_id = '"+w2.esc(memberId)+"' AND network_id = '"+w2.esc(networkId)+"'"
			);
			for(pqxx::result::const_iterator it = r2.begin(); it != r2.end(); ++it) {
				config["ipAssignments"].push_back(it["address"].as<std::string>());
			}
			w2.commit();

			_memberChanged(empty, config, false);
		}
		w.commit();

		if (++this->_ready == 2) {
			if (_waitNoticePrinted) {
				fprintf(stderr,"[%s] NOTICE: %.10llx controller PostgreSQL data download complete." ZT_EOL_S,_timestr(),(unsigned long long)_myAddress.toInt());
			}
			_readyLock.unlock();
		}
	} catch (std::exception &e) {
		fprintf(stderr, "ERROR: Error initializing members: %s\n", e.what());
		exit(-1);
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

	fprintf(stderr, "Heartbeat connection opening");
	pqxx::connection conn(_connString);
	if (!conn.is_open()) {
		fprintf(stderr, "Connection to database failed: heartbeat\n");
		exit(1);
	}
	fprintf(stderr, "Heartbeat connection opened");
	conn.prepare("heartbeat", 
		"INSERT INTO ztc_controller (id, cluster_host, last_alive, public_identity, v_major, v_minor, v_rev, v_build) " 
		"VALUES ($1, $2, TO_TIMESTAMP($3::double precision/1000), $4, $5, $6, $7, $8) "
		"ON CONFLICT (id) DO UPDATE SET cluster_host = EXCLUDED.cluster_host, last_alive = EXCLUDED.last_alive, "
		"public_identity = EXCLUDED.public_identity, v_major = EXCLUDED.v_major, v_minor = EXCLUDED.v_minor, "
		"v_rev = EXCLUDED.v_rev, v_build = EXCLUDED.v_rev"
	);

	while (_run == 1) {
		try {
			pqxx::work w(conn);
			pqxx::result res = w.prepared("heartbeat")(controllerId)(hostname)
				(OSUtils::now())(publicIdentity)(ZEROTIER_ONE_VERSION_MAJOR)
				(ZEROTIER_ONE_VERSION_MINOR)(ZEROTIER_ONE_VERSION_REVISION)
				(ZEROTIER_ONE_VERSION_BUILD).exec();
			w.commit();
		} catch (std::exception &e) {
			fprintf(stderr, "Error inserting heartbeat: %s\n", e.what());
			exit(1);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}
}

void PostgreSQL::membersDbWatcher()
{
	try {
		pqxx::connection conn(_connString);
		if (!conn.is_open()) {
			fprintf(stderr, "Connection to database failed: membersDbWatcher\n");
			exit(1);
		}

		initializeMembers(conn);

		char buf[11] = {0};
		std::string cmd = "member_" + std::string(_myAddress.toString(buf));
		_MemberNotificationReceiver receiver(conn, cmd, this);
		while(_run == 1) {
			conn.await_notification(5, 0);
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		conn.disconnect();
	} catch (std::exception &e) {
		fprintf(stderr, "Exception in membersDbWatcher: %s\n", e.what());
		exit(1);
	}
}

void PostgreSQL::networksDbWatcher()
{
	try {
		pqxx::connection conn(_connString);
		if (!conn.is_open()) {
			fprintf(stderr, "Connection to database failed: networksDbWatcher\n");
			exit(1);
		}

		initializeNetworks(conn);

		char buf[11] = {0};
		std::string cmd = "network_" + std::string(_myAddress.toString(buf));
		_NetworkNotificationReceiver receiver(conn, cmd, this);
		while(_run == 1) {
			conn.await_notification(5, 0);
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	} catch(std::exception &e) {
		fprintf(stderr, "Exception in networksDbWatcher: %s\n", e.what());
		exit(1);
	}
}

void PostgreSQL::commitThread()
{
	pqxx::connection conn(_connString);
	if (!conn.is_open()) {
		fprintf(stderr, "ERROR: Connection to database failed: commitThread\n");
		exit(1);
	}

	conn.prepare("insert_member",
		"INSERT INTO ztc_member (id, network_id, active_bridge, authorized, capabilities, "
		"identity, last_authorized_time, last_deauthorized_time, no_auto_assign_ips, "
		"remote_trace_level, remote_trace_target, revision, tags, v_major, v_minor, v_rev, v_proto) "
		"VALUES ($1, $2, $3, $4, $5, $6, "
		"TO_TIMESTAMP($7::double precision/1000), TO_TIMESTAMP($8::double precision/1000), "
		"$9, $10, (CASE WHEN $11='' THEN NULL ELSE $1 END), $12, $13, $14, $15, $16, $17) ON CONFLICT (network_id, id) DO UPDATE SET "
		"active_bridge = EXCLUDED.active_bridge, authorized = EXCLUDED.authorized, capabilities = EXCLUDED.capabilities, "
		"identity = EXCLUDED.identity, last_authorized_time = EXCLUDED.last_authorized_time, "
		"last_deauthorized_time = EXCLUDED.last_deauthorized_time, no_auto_assign_ips = EXCLUDED.no_auto_assign_ips, "
		"remote_trace_level = EXCLUDED.remote_trace_level, remote_trace_target = EXCLUDED.remote_trace_target, "
		"revision = EXCLUDED.revision+1, tags = EXCLUDED.tags, v_major = EXCLUDED.v_major, "
		"v_minor = EXCLUDED.v_minor, v_rev = EXCLUDED.v_rev, v_proto = EXCLUDED.v_proto");
	conn.prepare("delete_ip_assignments",
		"DELETE FROM ztc_member_ip_assignment WHERE member_id = $1 AND network_id = $2");
	conn.prepare("insert_ip_assignments",
		"INSERT INTO ztc_member_ip_assignment (member_id, network_id, address) VALUES ($1, $2, $3)");

	conn.prepare("update_network", 
		"UPDATE ztc_network SET controller_id = $2, capabilities = $3, enable_broadcast = $4, "
		"last_updated = $5, mtu = $6, multicast_limit = $7, name = $8, private = $9, "
		"remote_trace_level = $10, remote_trace_target = $11, rules = $12, rules_source = $13, "
		"tags = $14, v4_assign_mode = $15, v6_assign_mode = $16 "
		"WHERE id = $1");
	conn.prepare("delete_network_ip_pool",
		"DELETE FROM ztc_network_assignment_pool WHERE network_id = $1");
	conn.prepare("insert_network_ip_pool",
		"INSERT INTO ztc_network_assignment_pool (network_id, ip_range_start, ip_range_end) VALUES ($1, $2, $3)");
	conn.prepare("delete_network_route", "DELETE FROM ztc_network_route WHERE network_id = $1");
	conn.prepare("insert_network_route", "INSERT INTO ztc_network_route (network_id, address, bits, via) VALUES ($1, $2, $3, $4)");
	conn.prepare("delete_network", "UPDATE ztc_network SET deleted = true WHERE id = $1");
	conn.prepare("delete_member", "UPDATE ztc_member SET hidden = true, deleted = true WHERE id = $1 AND network_id = $2");

	json *config = nullptr;
	while(_commitQueue.get(config)&(_run == 1)) {
		if (!config) {
			continue;
		}
		try { 
			const std::string objtype = (*config)["objtype"];
			if (objtype == "member") {
				std::string memberId = (*config)["id"];
				std::string networkId = (*config)["nwid"];
				std::string target("");
				if (!(*config)["remoteTraceTarget"].is_null()) {
					target = (*config)["remoteTraceTarget"];
				}
				std::string identity = (*config)["identity"];

				try {
					std::string caps = OSUtils::jsonDump((*config)["capabilities"], -1);
					std::string tags = OSUtils::jsonDump((*config)["tags"], -1);

					pqxx::work w(conn);
					pqxx::result res = w.prepared("insert_member")(memberId)(networkId)
						((bool)(*config)["activeBridge"])((bool)(*config)["authorized"])(caps)
						(identity)((long long)(*config)["lastAuthorizedTime"])((long long)(*config)["lastDeauthorizedTime"])
						((bool)(*config)["noAutoAssignIps"])((int)(*config)["remoteTraceLevel"])
						(target, !target.empty())
						((int)(*config)["revision"])(tags)((int)(*config)["vMajor"])((int)(*config)["vMinor"])
						((int)(*config)["vRev"])((int)(*config)["vProto"]).exec();
					w.commit();
				} catch (std::exception &e) {
					fprintf(stderr, "Exception upserting member: %s\n", e.what());
					delete config;
					config = nullptr;
					continue;
				}

				try {
					pqxx::work w(conn);
					pqxx::result res = w.prepared("delete_ip_assignments")(memberId)(networkId).exec();
					
					for (auto i = (*config)["ipAssignments"].begin(); i != (*config)["ipAssignments"].end(); ++i) {
						std::string addr = *i;
						pqxx::result res2 = w.prepared("insert_ip_assignments")(memberId)(networkId)(addr).exec();
					}
					w.commit();
				} catch (std::exception &e) {
					fprintf(stderr, "Error assigning member IP addresses: %s\n", e.what());
					delete config;
					config = nullptr;
				}

				const uint64_t nwidInt = OSUtils::jsonIntHex((*config)["nwid"], 0ULL);
				const uint64_t memberidInt = OSUtils::jsonIntHex((*config)["id"], 0ULL);
				if (nwidInt && memberidInt) {
					nlohmann::json nwOrig;
					nlohmann::json memOrig;

					nlohmann::json memNew(*config);
					
					get(nwidInt, nwOrig, memberidInt, memOrig);
			
					_memberChanged(memOrig, memNew, (this->_ready>=2));
				} else {
					fprintf(stderr, "Can't notify of change.  Error parsing nwid or memberid: %lu-%lu\n", nwidInt, memberidInt);
				}
			} else if (objtype == "network") {
				std::string id = (*config)["id"];
				std::string controllerId = _myAddressStr.c_str();
				std::string name = (*config)["name"];
				std::string rulesSource = (*config)["rulesSource"];
				std::string caps = OSUtils::jsonDump((*config)["capabilitles"], -1);
				std::string rtraceLevel = std::to_string((int)(*config)["remoteTraceLevel"]);
				std::string rules = OSUtils::jsonDump((*config)["rules"], -1);
				std::string tags = OSUtils::jsonDump((*config)["tags"], -1);
				std::string v4mode = OSUtils::jsonDump((*config)["v4AssignMode"],-1);
				std::string v6mode = OSUtils::jsonDump((*config)["v6AssignMode"], -1);
				std::string target = "";
				if (!(*config)["remoteTraceTarget"].is_null()) {
					target = (*config)["remoteTraceTarget"];
				}
				try {
					pqxx::work w(conn);
					pqxx::result res = w.prepared("update_network")(id)(controllerId)(caps)((bool)(*config)["enableBroadcast"])
						(OSUtils::now())((int)(*config)["mtu"])((int)(*config)["multicastLimit"])(name)((bool)(*config)["private"])
						((int)(*config)["remoteTraceLevel"])(target, !target.empty())
						(rules)(rulesSource)(tags)(v4mode)(v6mode).exec();
					w.commit();
				} catch (std::exception &e) {
					fprintf(stderr, "Error updating network config: %s\n", e.what());
					delete config;
					config = nullptr;
					continue;
				}

				try {
					pqxx::work w(conn);
					pqxx::result res = w.prepared("delete_network_ip_pool")(id).exec();

					auto pool = (*config)["ipAssignmentPools"];
					for (auto i = pool.begin(); i != pool.end(); ++i) {
						std::string start = (*i)["ipRangeStart"];
						std::string end = (*i)["ipRangeEnd"];

						pqxx::result r2 = w.prepared("insert_nework_ip_pool")(id)(start)(end).exec();
					}


					pqxx::result res2 = w.prepared("delete_network_route")(id).exec();

					auto routes = (*config)["routes"];
					for (auto i = routes.begin(); i != routes.end(); ++i) {
						std::string t = (*i)["target"];
						std::vector<std::string> target;
						std::istringstream f(t);
						std::string s;
						while(std::getline(f, s, '/')) {
							target.push_back(s);
						}
						if (target.empty() || target.size() != 2) {
							continue;
						}
						std::string targetAddr = target[0];
						int targetBits = std::stoi(target[1]);
						std::string via = "";
						if (!(*i)["via"].is_null()) {
							via = (*i)["via"];
						}
						
						pqxx::result res3 = w.prepared("insert_network_route")(id)(targetAddr)(targetBits)
							(via, !via.empty()).exec();
					}

					w.commit();

				} catch (std::exception &e) {
					fprintf(stderr, "Error updating network IP pool: %s\n", e.what());
				}

				const uint64_t nwidInt = OSUtils::jsonIntHex((*config)["nwid"], 0ULL);
				if (nwidInt) {
					nlohmann::json nwOrig;
					nlohmann::json nwNew(*config);

					get(nwidInt, nwOrig);

					_networkChanged(nwOrig, nwNew, true);
				} else {
					fprintf(stderr, "Can't notify network changed: %lu\n", nwidInt);
				}
			} else if (objtype == "trace") {
				fprintf(stderr, "ERROR: Trace not yet implemented");
			} else if (objtype == "_delete_network") {
				try {
					std::string networkId = (*config)["nwid"];
					
					pqxx::work w(conn);
					pqxx::result res = w.prepared("delete_network")(networkId).exec();
					w.commit();
				} catch (std::exception &e) {
					fprintf(stderr, "ERROR: Error deleting network: %s\n", e.what());
				}
			} else if (objtype == "_delete_member") {
				try {
					std::string memberId = (*config)["id"];
					std::string networkId = (*config)["nwid"];

					pqxx::work w(conn);
					pqxx::result res = w.prepared("delete_member")(memberId)(networkId).exec();
					w.commit();
				} catch (std::exception &e) {
					fprintf(stderr, "ERROR: Error deleting member: %s\n", e.what());
				}
			} else {
				fprintf(stderr, "ERROR: unknown objtype");
			}
		} catch (std::exception &e) {
			fprintf(stderr, "ERROR: Error getting objtype: %s\n", e.what());
		}
		delete config;
		config = nullptr;

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

void PostgreSQL::onlineNotificationThread()
{
	pqxx::connection conn(_connString);
	if(!conn.is_open()) {
		fprintf(stderr, "Connection to database failed: onlineNotificationThread\n");
		exit(1);
	}
	_connected = 1;

	int64_t	lastUpdatedNetworkStatus = 0;
	std::unordered_map< std::pair<uint64_t,uint64_t>,int64_t,_PairHasher > lastOnlineCumulative;
	
	while (_run == 1) {
		// map used to send notifications to front end
		std::unordered_map<std::string, std::vector<std::string>> updateMap;

		std::unordered_map< std::pair<uint64_t,uint64_t>,std::pair<int64_t,InetAddress>,_PairHasher > lastOnline;
		{
			std::lock_guard<std::mutex> l(_lastOnline_l);
			lastOnline.swap(_lastOnline);
		}

		try {
			pqxx::work w(conn);
			pqxx::pipeline p(w, "Member Update Pipeline");
			for (auto i=lastOnline.begin(); i != lastOnline.end(); ++i) {
				uint64_t nwid_i = i->first.first;
				char nwidTmp[64];
				char memTmp[64];
				char ipTmp[64];
				OSUtils::ztsnprintf(nwidTmp,sizeof(nwidTmp), "%.16llx", nwid_i);
				OSUtils::ztsnprintf(memTmp,sizeof(memTmp), "%.10llx", i->first.second);

				auto found = _networks.find(nwid_i);
				if (found == _networks.end()) {
					continue; // skip members trying to join non-existant networks
				}

				lastOnlineCumulative[i->first] = i->second.first;
				

				std::string networkId(nwidTmp);
				std::string memberId(memTmp);

				std::vector<std::string> &members = updateMap[networkId];
				members.push_back(memberId);

				int64_t ts = i->second.first;
				std::string ipAddr = i->second.second.toIpString(ipTmp);
				std::string timestamp = std::to_string(ts);

				std::stringstream ss;
				ss << "INSERT INTO ztc_member_status (network_id, member_id, address, last_updated) VALUES ("
				<< "'" << w.esc(networkId) << "', "
				<< "'" << w.esc(memberId) << "', "
				<< "'" << w.esc(ipAddr) << "', "
				<< timestamp << ") "
				<< "ON CONFLICT (network_id, member_id) DO UPDATE SET address = EXCLUDED.address, last_updated = EXCLUDED.last_updated";
				p.insert(ss.str());
			}
			p.complete();
			w.commit();
		} catch (std::exception &e) {
			fprintf(stderr, "Error updating member status: %s\n", e.what());
		}

		try {
			const int64_t now = OSUtils::now();
			if ((now - lastUpdatedNetworkStatus) > 10000) {
				pqxx::work w(conn);
				pqxx::pipeline p(w, "Network Update Pipeline");
				lastUpdatedNetworkStatus = now;

				std::vector<std::pair<uint64_t, std::shared_ptr<_Network>>> networks;
				{
					std::lock_guard<std::mutex> l(_networks_l);
					for (auto i = _networks.begin(); i != _networks.end(); ++i) {
						networks.push_back(*i);
					}
				}

				for (auto i = networks.begin(); i != networks.end(); ++i) {
					char tmp[64];
					Utils::hex(i->first, tmp);

					std::string networkId(tmp);

					std::vector<std::string> &_notUsed = updateMap[networkId];
					(void)_notUsed;

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

					std::string bc = std::to_string(bridgeCount);
					std::string amc = std::to_string(authMemberCount);
					std::string omc = std::to_string(onlineMemberCount);
					std::string tmc = std::to_string(totalMemberCount);
					std::string timestamp = std::to_string(ts);

					std::stringstream ss;
					ss << "INSERT INTO ztc_network_status (network_id, bridge_count, authorized_member_count, "
					   << "online_member_count, total_member_count, last_modified) VALUES ("
					   << "'" << w.esc(networkId) << "', "
					   << bridgeCount << ", "
					   << authMemberCount << ", "
					   << onlineMemberCount << ", "
					   << totalMemberCount << ", "
					   << "TO_TIMESTAMP(" << ts << "::double precision/1000)) "
					   << "ON CONFLICT (network_id) DO UPDATE SET bridge_count = EXCLUDED.bridge_count, "
					   << "authorized_member_count = EXCLUDED.authorized_member_count, online_member_count = EXCLUDED.online_member_count, "
					   << "total_member_count = EXCLUDED.total_member_count, last_modified = EXCLUDED.last_modified"; 
					p.insert(ss.str());
				}
				p.complete();
				w.commit();
			}
		} catch (std::exception &e) {
			fprintf(stderr, "Error updating network status: %s\n", e.what());
		}

		try {
			pqxx::work w(conn);
			pqxx::pipeline p(w, "Notification Sender");
			for (auto it = updateMap.begin(); it != updateMap.end(); ++it) {
				std::string networkId = it->first;
				std::vector<std::string> members = it->second;
				std::stringstream queryBuilder;

				std::string membersStr = ::join(members, ",");

				queryBuilder << "NOTIFY controller, '" << networkId << ":" << membersStr << "'";
				std::string query = queryBuilder.str();

				p.insert(query);
			}
			p.complete();
			w.commit();
		} catch (std::exception &e) {
			fprintf(stderr, "Error notifying webapp: %s\n", e.what());
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}
#endif //ZT_CONTROLLER_USE_LIBPQ
