/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#include "PostgreSQL.hpp"

#ifdef ZT_CONTROLLER_USE_LIBPQ

#include "../node/Constants.hpp"
#include "../node/SHA512.hpp"
#include "EmbeddedNetworkController.hpp"
#include "../version.h"
#include "Redis.hpp"

#include <smeeclient.h>

#include <libpq-fe.h>
#include <sstream>
#include <iomanip>
#include <climits>
#include <chrono>


// #define REDIS_TRACE 1

using json = nlohmann::json;

namespace {

static const int DB_MINIMUM_VERSION = 38;

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

/*
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
*/

std::vector<std::string> split(std::string str, char delim){
	std::istringstream iss(str);
	std::vector<std::string> tokens;
	std::string item;
	while(std::getline(iss, item, delim)) {
		tokens.push_back(item);
	}
	return tokens;
}

std::string url_encode(const std::string &value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (std::string::const_iterator i = value.begin(), n = value.end(); i != n; ++i) {
        std::string::value_type c = (*i);

        // Keep alphanumeric and other accepted characters intact
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
            continue;
        }

        // Any other characters are percent-encoded
        escaped << std::uppercase;
        escaped << '%' << std::setw(2) << int((unsigned char) c);
        escaped << std::nouppercase;
    }

    return escaped.str();
}

} // anonymous namespace

using namespace ZeroTier;


MemberNotificationReceiver::MemberNotificationReceiver(PostgreSQL *p, pqxx::connection &c, const std::string &channel)
	: pqxx::notification_receiver(c, channel)
	, _psql(p)
{
	fprintf(stderr, "initialize MemberNotificationReceiver\n");
}
	

void MemberNotificationReceiver::operator() (const std::string &payload, int packend_pid) {
	fprintf(stderr, "Member Notification received: %s\n", payload.c_str());
	Metrics::pgsql_mem_notification++;
	json tmp(json::parse(payload));
	json &ov = tmp["old_val"];
	json &nv = tmp["new_val"];
	json oldConfig, newConfig;
	if (ov.is_object()) oldConfig = ov;
	if (nv.is_object()) newConfig = nv;
	if (oldConfig.is_object() || newConfig.is_object()) {
		_psql->_memberChanged(oldConfig,newConfig,(_psql->_ready>=2));
		fprintf(stderr, "payload sent\n");
	}
}


NetworkNotificationReceiver::NetworkNotificationReceiver(PostgreSQL *p, pqxx::connection &c, const std::string &channel)
	: pqxx::notification_receiver(c, channel)
	, _psql(p)
{
	fprintf(stderr, "initialize NetworkNotificationReceiver\n");
}

void NetworkNotificationReceiver::operator() (const std::string &payload, int packend_pid) {
	fprintf(stderr, "Network Notification received: %s\n", payload.c_str());
	Metrics::pgsql_net_notification++;
	json tmp(json::parse(payload));
	json &ov = tmp["old_val"];
	json &nv = tmp["new_val"];
	json oldConfig, newConfig;
	if (ov.is_object()) oldConfig = ov;
	if (nv.is_object()) newConfig = nv;
	if (oldConfig.is_object() || newConfig.is_object()) {
		_psql->_networkChanged(oldConfig,newConfig,(_psql->_ready>=2));
		fprintf(stderr, "payload sent\n");
	}
}

using Attrs = std::vector<std::pair<std::string, std::string>>;
using Item = std::pair<std::string, Attrs>;
using ItemStream = std::vector<Item>;



PostgreSQL::PostgreSQL(const Identity &myId, const char *path, int listenPort, RedisConfig *rc)
	: DB()
	, _pool()
	, _myId(myId)
	, _myAddress(myId.address())
	, _ready(0)
	, _connected(1)
	, _run(1)
	, _waitNoticePrinted(false)
	, _listenPort(listenPort)
	, _rc(rc)
	, _redis(NULL)
	, _cluster(NULL)
	, _redisMemberStatus(false)
	, _smee(NULL)
{
	char myAddress[64];
	_myAddressStr = myId.address().toString(myAddress);
	_connString = std::string(path);
	auto f = std::make_shared<PostgresConnFactory>(_connString);
	_pool = std::make_shared<ConnectionPool<PostgresConnection> >(
		15, 5, std::static_pointer_cast<ConnectionFactory>(f));
	
	memset(_ssoPsk, 0, sizeof(_ssoPsk));
	char *const ssoPskHex = getenv("ZT_SSO_PSK");
#ifdef ZT_TRACE
	fprintf(stderr, "ZT_SSO_PSK: %s\n", ssoPskHex);
#endif
	if (ssoPskHex) {
		// SECURITY: note that ssoPskHex will always be null-terminated if libc actually
		// returns something non-NULL. If the hex encodes something shorter than 48 bytes,
		// it will be padded at the end with zeroes. If longer, it'll be truncated.
		Utils::unhex(ssoPskHex, _ssoPsk, sizeof(_ssoPsk));
	}
	const char *redisMemberStatus = getenv("ZT_REDIS_MEMBER_STATUS");
	if (redisMemberStatus && (strcmp(redisMemberStatus, "true") == 0)) {
	    _redisMemberStatus = true;
		fprintf(stderr, "Using redis for member status\n");
	}

	auto c = _pool->borrow();
	pqxx::work txn{*c->c};

	pqxx::row r{txn.exec1("SELECT version FROM ztc_database")};
	int dbVersion = r[0].as<int>();
	txn.commit();

	if (dbVersion < DB_MINIMUM_VERSION) {
		fprintf(stderr, "Central database schema version too low.  This controller version requires a minimum schema version of %d. Please upgrade your Central instance", DB_MINIMUM_VERSION);
		exit(1);
	}
	_pool->unborrow(c);

	if (_rc != NULL) {
		sw::redis::ConnectionOptions opts;
		sw::redis::ConnectionPoolOptions poolOpts;
		opts.host = _rc->hostname;
		opts.port = _rc->port;
		opts.password = _rc->password;
		opts.db = 0;
		opts.keep_alive = true;
		opts.connect_timeout = std::chrono::seconds(3);
		poolOpts.size = 25;
		poolOpts.wait_timeout = std::chrono::seconds(5);
		poolOpts.connection_lifetime = std::chrono::minutes(3);
		poolOpts.connection_idle_time = std::chrono::minutes(1);
		if (_rc->clusterMode) {
			fprintf(stderr, "Using Redis in Cluster Mode\n");
			_cluster = std::make_shared<sw::redis::RedisCluster>(opts, poolOpts);
		} else {
			fprintf(stderr, "Using Redis in Standalone Mode\n");
			_redis = std::make_shared<sw::redis::Redis>(opts, poolOpts);
		}
	}

	_readyLock.lock();
	
	fprintf(stderr, "[%s] NOTICE: %.10llx controller PostgreSQL waiting for initial data download..." ZT_EOL_S, ::_timestr(), (unsigned long long)_myAddress.toInt());
	_waitNoticePrinted = true;

	initializeNetworks();
	initializeMembers();

	_heartbeatThread = std::thread(&PostgreSQL::heartbeat, this);
	_membersDbWatcher = std::thread(&PostgreSQL::membersDbWatcher, this);
	_networksDbWatcher = std::thread(&PostgreSQL::networksDbWatcher, this);
	for (int i = 0; i < ZT_CENTRAL_CONTROLLER_COMMIT_THREADS; ++i) {
		_commitThread[i] = std::thread(&PostgreSQL::commitThread, this);
	}
	_onlineNotificationThread = std::thread(&PostgreSQL::onlineNotificationThread, this);

	configureSmee();
}

PostgreSQL::~PostgreSQL()
{
	if (_smee != NULL) {
		smeeclient::smee_client_delete(_smee);
		_smee = NULL;
	}

	_run = 0;
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	_heartbeatThread.join();
	_membersDbWatcher.join();
	_networksDbWatcher.join();
	_commitQueue.stop();
	for (int i = 0; i < ZT_CENTRAL_CONTROLLER_COMMIT_THREADS; ++i) {
		_commitThread[i].join();
	}
	_onlineNotificationThread.join();
}

void PostgreSQL::configureSmee() 
{
	const char *TEMPORAL_HOST = "ZT_TEMPORAL_HOST";
	const char *TEMPORAL_PORT = "ZT_TEMPORAL_PORT";
	const char *TEMPORAL_NAMESPACE = "ZT_TEMPORAL_NAMESPACE";
	const char *SMEE_TASK_QUEUE = "ZT_SMEE_TASK_QUEUE";

	const char *host = getenv(TEMPORAL_HOST);
	const char *port = getenv(TEMPORAL_PORT);
	const char *ns = getenv(TEMPORAL_NAMESPACE);
	const char *task_queue = getenv(SMEE_TASK_QUEUE);

	if (host != NULL && port != NULL && ns != NULL && task_queue != NULL) {
		fprintf(stderr, "creating smee client\n");
		std::string hostPort = std::string(host) + std::string(":") + std::string(port);
		this->_smee = smeeclient::smee_client_new(hostPort.c_str(), ns, task_queue);
	} else {
		fprintf(stderr, "Smee client not configured\n");
	}
}

bool PostgreSQL::waitForReady()
{
	while (_ready < 2) {
		_readyLock.lock();
		_readyLock.unlock();
	}
	return true;
}

bool PostgreSQL::isReady()
{
	return ((_ready == 2)&&(_connected));
}

bool PostgreSQL::save(nlohmann::json &record,bool notifyListeners)
{
	bool modified = false;
	try {
		if (!record.is_object()) {
			fprintf(stderr, "record is not an object?!?\n");
			return false;
		}
		const std::string objtype = record["objtype"];
		if (objtype == "network") {
			//fprintf(stderr, "network save\n");
			const uint64_t nwid = OSUtils::jsonIntHex(record["id"],0ULL);
			if (nwid) {
				nlohmann::json old;
				get(nwid,old);
				if ((!old.is_object())||(!_compareRecords(old,record))) {
					record["revision"] = OSUtils::jsonInt(record["revision"],0ULL) + 1ULL;
					_commitQueue.post(std::pair<nlohmann::json,bool>(record,notifyListeners));
					modified = true;
				}
			}
		} else if (objtype == "member") {
			std::string networkId = record["nwid"];
			std::string memberId = record["id"];
			const uint64_t nwid = OSUtils::jsonIntHex(record["nwid"],0ULL);
			const uint64_t id = OSUtils::jsonIntHex(record["id"],0ULL);
			//fprintf(stderr, "member save %s-%s\n", networkId.c_str(), memberId.c_str());
			if ((id)&&(nwid)) {
				nlohmann::json network,old;
				get(nwid,network,id,old);
				if ((!old.is_object())||(!_compareRecords(old,record))) {
					//fprintf(stderr, "commit queue post\n");
					record["revision"] = OSUtils::jsonInt(record["revision"],0ULL) + 1ULL;
					_commitQueue.post(std::pair<nlohmann::json,bool>(record,notifyListeners));
					modified = true;
				} else {
					//fprintf(stderr, "no change\n");
				}
			}
		} else {
			fprintf(stderr, "uhh waaat\n");
		}
	} catch (std::exception &e) {
		fprintf(stderr, "Error on PostgreSQL::save: %s\n", e.what());
	} catch (...) {
		fprintf(stderr, "Unknown error on PostgreSQL::save\n");
	}
	return modified;
}

void PostgreSQL::eraseNetwork(const uint64_t networkId)
{
	fprintf(stderr, "PostgreSQL::eraseNetwork\n");
	char tmp2[24];
	waitForReady();
	Utils::hex(networkId, tmp2);
	std::pair<nlohmann::json,bool> tmp;
	tmp.first["id"] = tmp2;
	tmp.first["objtype"] = "_delete_network";
	tmp.second = true;
	_commitQueue.post(tmp);
	nlohmann::json nullJson;
	_networkChanged(tmp.first, nullJson, true);
}

void PostgreSQL::eraseMember(const uint64_t networkId, const uint64_t memberId)
{
	fprintf(stderr, "PostgreSQL::eraseMember\n");
	char tmp2[24];
	waitForReady();
	std::pair<nlohmann::json,bool> tmp, nw;
	Utils::hex(networkId, tmp2);
	tmp.first["nwid"] = tmp2;
	Utils::hex(memberId, tmp2);
	tmp.first["id"] = tmp2;
	tmp.first["objtype"] = "_delete_member";
	tmp.second = true;
	_commitQueue.post(tmp);
	nlohmann::json nullJson;
	_memberChanged(tmp.first, nullJson, true);
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

AuthInfo PostgreSQL::getSSOAuthInfo(const nlohmann::json &member, const std::string &redirectURL)
{
	Metrics::db_get_sso_info++;
	// NONCE is just a random character string.  no semantic meaning
	// state = HMAC SHA384 of Nonce based on shared sso key
	// 
	// need nonce timeout in database? make sure it's used within X time
	// X is 5 minutes for now.  Make configurable later?
	//
	// how do we tell when a nonce is used? if auth_expiration_time is set
	std::string networkId = member["nwid"];
	std::string memberId = member["id"];


	char authenticationURL[4096] = {0};
	AuthInfo info;
	info.enabled = true;

	//if (memberId == "a10dccea52" && networkId == "8056c2e21c24673d") {
	//	fprintf(stderr, "invalid authinfo for grant's machine\n");
	//	info.version=1;
	//	return info;
	//}
	// fprintf(stderr, "PostgreSQL::updateMemberOnLoad: %s-%s\n", networkId.c_str(), memberId.c_str());
	std::shared_ptr<PostgresConnection> c;
	try {
		c = _pool->borrow();
		pqxx::work w(*c->c);

		char nonceBytes[16] = {0};
		std::string nonce = "";

		// check if the member exists first.
		pqxx::row count = w.exec_params1("SELECT count(id) FROM ztc_member WHERE id = $1 AND network_id = $2 AND deleted = false", memberId, networkId);
		if (count[0].as<int>() == 1) {
			// get active nonce, if exists.
			pqxx::result r = w.exec_params("SELECT nonce FROM ztc_sso_expiry "
				"WHERE network_id = $1 AND member_id = $2 "
				"AND ((NOW() AT TIME ZONE 'UTC') <= authentication_expiry_time) AND ((NOW() AT TIME ZONE 'UTC') <= nonce_expiration)",
				networkId, memberId);

			if (r.size() == 0) {
				// no active nonce.
				// find an unused nonce, if one exists.
				pqxx::result r = w.exec_params("SELECT nonce FROM ztc_sso_expiry "
					"WHERE network_id = $1 AND member_id = $2 "
					"AND authentication_expiry_time IS NULL AND ((NOW() AT TIME ZONE 'UTC') <= nonce_expiration)",
					networkId, memberId);

				if (r.size() == 1) {
					// we have an existing nonce.  Use it
					nonce = r.at(0)[0].as<std::string>();
					Utils::unhex(nonce.c_str(), nonceBytes, sizeof(nonceBytes));
				} else if (r.empty()) {
					// create a nonce
					Utils::getSecureRandom(nonceBytes, 16);
					char nonceBuf[64] = {0};
					Utils::hex(nonceBytes, sizeof(nonceBytes), nonceBuf);
					nonce = std::string(nonceBuf);

					pqxx::result ir = w.exec_params0("INSERT INTO ztc_sso_expiry "
						"(nonce, nonce_expiration, network_id, member_id) VALUES "
						"($1, TO_TIMESTAMP($2::double precision/1000), $3, $4)",
						nonce, OSUtils::now() + 300000, networkId, memberId);

					w.commit();
				}  else {
					// > 1 ?!?  Thats an error!
					fprintf(stderr, "> 1 unused nonce!\n");
					exit(6);
				}
			} else if (r.size() == 1) {
				nonce = r.at(0)[0].as<std::string>();
				Utils::unhex(nonce.c_str(), nonceBytes, sizeof(nonceBytes));
			} else {
				// more than 1 nonce in use?  Uhhh...
				fprintf(stderr, "> 1 nonce in use for network member?!?\n");
				exit(7);
			}

			r = w.exec_params(
				"SELECT oc.client_id, oc.authorization_endpoint, oc.issuer, oc.provider, oc.sso_impl_version "
				"FROM ztc_network AS n "
				"INNER JOIN ztc_org o "
				"  ON o.owner_id = n.owner_id "
			    "LEFT OUTER JOIN ztc_network_oidc_config noc "
				"  ON noc.network_id = n.id "
				"LEFT OUTER JOIN ztc_oidc_config oc "
				"  ON noc.client_id = oc.client_id AND oc.org_id = o.org_id "
				"WHERE n.id = $1 AND n.sso_enabled = true", networkId);
		
			std::string client_id = "";
			std::string authorization_endpoint = "";
			std::string issuer = "";
			std::string provider = "";
			uint64_t sso_version = 0;

			if (r.size() == 1) {
				client_id = r.at(0)[0].as<std::optional<std::string>>().value_or("");
				authorization_endpoint = r.at(0)[1].as<std::optional<std::string>>().value_or("");
				issuer = r.at(0)[2].as<std::optional<std::string>>().value_or("");
				provider = r.at(0)[3].as<std::optional<std::string>>().value_or("");
				sso_version = r.at(0)[4].as<std::optional<uint64_t>>().value_or(1);
			} else if (r.size() > 1) {
				fprintf(stderr, "ERROR: More than one auth endpoint for an organization?!?!? NetworkID: %s\n", networkId.c_str());
			} else {
				fprintf(stderr, "No client or auth endpoint?!?\n");
			}
		
			info.version = sso_version;
			
			// no catch all else because we don't actually care if no records exist here. just continue as normal.
			if ((!client_id.empty())&&(!authorization_endpoint.empty())) {
				
				uint8_t state[48];
				HMACSHA384(_ssoPsk, nonceBytes, sizeof(nonceBytes), state);
				char state_hex[256];
				Utils::hex(state, 48, state_hex);
				
				if (info.version == 0) {
					char url[2048] = {0};
					OSUtils::ztsnprintf(url, sizeof(authenticationURL),
						"%s?response_type=id_token&response_mode=form_post&scope=openid+email+profile&redirect_uri=%s&nonce=%s&state=%s&client_id=%s",
						authorization_endpoint.c_str(),
						url_encode(redirectURL).c_str(),
						nonce.c_str(),
						state_hex,
						client_id.c_str());
					info.authenticationURL = std::string(url);
				} else if (info.version == 1) {
					info.ssoClientID = client_id;
					info.issuerURL = issuer;
					info.ssoProvider = provider;
					info.ssoNonce = nonce;
					info.ssoState = std::string(state_hex) + "_" +networkId;
					info.centralAuthURL = redirectURL;
#ifdef ZT_DEBUG
					fprintf(
						stderr,
						"ssoClientID: %s\nissuerURL: %s\nssoNonce: %s\nssoState: %s\ncentralAuthURL: %s\nprovider: %s\n",
						info.ssoClientID.c_str(),
						info.issuerURL.c_str(),
						info.ssoNonce.c_str(),
						info.ssoState.c_str(),
						info.centralAuthURL.c_str(),
						provider.c_str());
#endif
				}
			}  else {
				fprintf(stderr, "client_id: %s\nauthorization_endpoint: %s\n", client_id.c_str(), authorization_endpoint.c_str());
			}
		}

		_pool->unborrow(c);
	} catch (std::exception &e) {
		fprintf(stderr, "ERROR: Error updating member on load for network %s: %s\n", networkId.c_str(), e.what());
	}

	return info; //std::string(authenticationURL);
}

void PostgreSQL::initializeNetworks()
{
	try {
		std::string setKey = "networks:{" + _myAddressStr + "}";
		
		fprintf(stderr, "Initializing Networks...\n");

		if (_redisMemberStatus) {
			fprintf(stderr, "Init Redis for networks...\n");
			try {
				if (_rc->clusterMode) {
					_cluster->del(setKey);
				} else {
					_redis->del(setKey);
				}
			} catch (sw::redis::Error &e) {
				// ignore. if this key doesn't exist, there's no reason to delete it
			}
		}

		std::unordered_set<std::string> networkSet;

		char qbuf[2048] = {0};
		sprintf(qbuf,
			"SELECT n.id, (EXTRACT(EPOCH FROM n.creation_time AT TIME ZONE 'UTC')*1000)::bigint as creation_time, n.capabilities, "
			"n.enable_broadcast, (EXTRACT(EPOCH FROM n.last_modified AT TIME ZONE 'UTC')*1000)::bigint AS last_modified, n.mtu, n.multicast_limit, n.name, n.private, n.remote_trace_level, "
			"n.remote_trace_target, n.revision, n.rules, n.tags, n.v4_assign_mode, n.v6_assign_mode, n.sso_enabled, (CASE WHEN n.sso_enabled THEN noc.client_id ELSE NULL END) as client_id, "
			"(CASE WHEN n.sso_enabled THEN oc.authorization_endpoint ELSE NULL END) as authorization_endpoint, "
			"(CASE WHEN n.sso_enabled THEN oc.provider ELSE NULL END) as provider, d.domain, d.servers, "
			"ARRAY(SELECT CONCAT(host(ip_range_start),'|', host(ip_range_end)) FROM ztc_network_assignment_pool WHERE network_id = n.id) AS assignment_pool, "
			"ARRAY(SELECT CONCAT(host(address),'/',bits::text,'|',COALESCE(host(via), 'NULL'))FROM ztc_network_route WHERE network_id = n.id) AS routes "
			"FROM ztc_network n "
			"LEFT OUTER JOIN ztc_org o "
			" ON o.owner_id = n.owner_id "
			"LEFT OUTER JOIN ztc_network_oidc_config noc "
			"	ON noc.network_id = n.id "
			"LEFT OUTER JOIN ztc_oidc_config oc "
			"	ON noc.client_id = oc.client_id AND oc.org_id = o.org_id "
			"LEFT OUTER JOIN ztc_network_dns d "
			"	ON d.network_id = n.id "
			"WHERE deleted = false AND controller_id = '%s'", _myAddressStr.c_str());
		auto c = _pool->borrow();
		auto c2 = _pool->borrow();
		pqxx::work w{*c->c};

		fprintf(stderr, "Load networks from psql...\n");
		auto stream = pqxx::stream_from::query(w, qbuf);

		std::tuple<
		      std::string 					// network ID
			, std::optional<int64_t> 		// creationTime
			, std::optional<std::string>	// capabilities
			, std::optional<bool>			// enableBroadcast
			, std::optional<uint64_t>		// lastModified
			, std::optional<int>			// mtu
			, std::optional<int>			// multicastLimit
			, std::optional<std::string>	// name
			, bool							// private
			, std::optional<int>			// remoteTraceLevel
			, std::optional<std::string>	// remoteTraceTarget
			, std::optional<uint64_t>		// revision
			, std::optional<std::string>	// rules
			, std::optional<std::string>	// tags
			, std::optional<std::string>	// v4AssignMode
			, std::optional<std::string>	// v6AssignMode
			, std::optional<bool>			// ssoEnabled
			, std::optional<std::string>	// clientId
			, std::optional<std::string>	// authorizationEndpoint
			, std::optional<std::string>    // ssoProvider
			, std::optional<std::string>	// domain
			, std::optional<std::string>	// servers
			, std::string					// assignmentPoolString
			, std::string					// routeString
		> row;

		uint64_t count = 0;
		auto tmp = std::chrono::high_resolution_clock::now();
		uint64_t total = 0;
		while (stream >> row) {
			auto start = std::chrono::high_resolution_clock::now();

			json empty;
			json config;

			initNetwork(config);

			std::string nwid = std::get<0>(row);
			std::optional<int64_t> creationTime = std::get<1>(row);
			std::optional<std::string> capabilities = std::get<2>(row);
			std::optional<bool> enableBroadcast = std::get<3>(row);
			std::optional<uint64_t> lastModified = std::get<4>(row);
			std::optional<int> mtu = std::get<5>(row);
			std::optional<int> multicastLimit = std::get<6>(row);
			std::optional<std::string> name = std::get<7>(row);
			bool isPrivate = std::get<8>(row);
			std::optional<int> remoteTraceLevel = std::get<9>(row);
			std::optional<std::string> remoteTraceTarget = std::get<10>(row);
			std::optional<uint64_t> revision = std::get<11>(row);
			std::optional<std::string> rules = std::get<12>(row);
			std::optional<std::string> tags = std::get<13>(row);
			std::optional<std::string> v4AssignMode = std::get<14>(row);
			std::optional<std::string> v6AssignMode = std::get<15>(row);
			std::optional<bool> ssoEnabled = std::get<16>(row);
			std::optional<std::string> clientId = std::get<17>(row);
			std::optional<std::string> authorizationEndpoint = std::get<18>(row);
			std::optional<std::string> ssoProvider = std::get<19>(row);
			std::optional<std::string> dnsDomain = std::get<20>(row);
			std::optional<std::string> dnsServers = std::get<21>(row);
			std::string assignmentPoolString = std::get<22>(row);
			std::string routesString = std::get<23>(row);
			
		 	config["id"] = nwid;
		 	config["nwid"] = nwid;
			config["creationTime"] = creationTime.value_or(0);
			config["capabilities"] = json::parse(capabilities.value_or("[]"));
			config["enableBroadcast"] = enableBroadcast.value_or(false);
			config["lastModified"] = lastModified.value_or(0);
			config["mtu"] = mtu.value_or(2800);
			config["multicastLimit"] = multicastLimit.value_or(64);
		 	config["name"] = name.value_or(""); 
		 	config["private"] = isPrivate;
	 		config["remoteTraceLevel"] = remoteTraceLevel.value_or(0);
			config["remoteTraceTarget"] = remoteTraceTarget.value_or("");
			config["revision"] = revision.value_or(0);
		 	config["rules"] = json::parse(rules.value_or("[]"));
		 	config["tags"] = json::parse(tags.value_or("[]"));
		 	config["v4AssignMode"] = json::parse(v4AssignMode.value_or("{}"));
		 	config["v6AssignMode"] = json::parse(v6AssignMode.value_or("{}"));
		 	config["ssoEnabled"] = ssoEnabled.value_or(false);
		 	config["objtype"] = "network";
		 	config["ipAssignmentPools"] = json::array();
		 	config["routes"] = json::array();
			config["clientId"] = clientId.value_or("");
			config["authorizationEndpoint"] = authorizationEndpoint.value_or("");
			config["provider"] = ssoProvider.value_or("");

			networkSet.insert(nwid);

			if (dnsDomain.has_value()) {
				std::string serverList = dnsServers.value();
				json obj;
				auto servers = json::array();
				if (serverList.rfind("{",0) != std::string::npos) {
					serverList = serverList.substr(1, serverList.size()-2);
					std::stringstream ss(serverList);
					while(ss.good()) {
						std::string server;
						std::getline(ss, server, ',');
						servers.push_back(server);
					}
				}
				obj["domain"] = dnsDomain.value();
				obj["servers"] = servers;
				config["dns"] = obj;
			}

			config["ipAssignmentPools"] = json::array();	
			if (assignmentPoolString != "{}") {
				std::string tmp = assignmentPoolString.substr(1, assignmentPoolString.size()-2);
				std::vector<std::string> assignmentPools = split(tmp, ',');
				for (auto it = assignmentPools.begin(); it != assignmentPools.end(); ++it) {
					std::vector<std::string> r = split(*it, '|');
					json ip;
					ip["ipRangeStart"] = r[0];
					ip["ipRangeEnd"] = r[1];
					config["ipAssignmentPools"].push_back(ip);
				}
			}

			config["routes"] = json::array();
			if (routesString != "{}") {
				std::string tmp = routesString.substr(1, routesString.size()-2);
				std::vector<std::string> routes = split(tmp, ',');
				for (auto it = routes.begin(); it != routes.end(); ++it) {
					std::vector<std::string> r = split(*it, '|');
					json route;
					route["target"] = r[0];
					route["via"] = ((route["via"] == "NULL")? nullptr : r[1]);
					config["routes"].push_back(route);
				}
			}

			Metrics::network_count++;

		 	_networkChanged(empty, config, false);

			auto end = std::chrono::high_resolution_clock::now();
			auto dur = std::chrono::duration_cast<std::chrono::microseconds>(end - start);;
			total += dur.count();
			++count;
			if (count > 0 && count % 10000 == 0) {
				fprintf(stderr, "Averaging %llu us per network\n", (total/count));
			}
		}

		if (count > 0) {
			fprintf(stderr, "Took %llu us per network to load\n", (total/count));
		}
		stream.complete();

		w.commit();
		_pool->unborrow(c2);
		_pool->unborrow(c);
		fprintf(stderr, "done.\n");

		if (!networkSet.empty()) {
			if (_redisMemberStatus) {
				fprintf(stderr, "adding networks to redis...\n");
				if (_rc->clusterMode) {
					auto tx = _cluster->transaction(_myAddressStr, true, false);
					tx.sadd(setKey, networkSet.begin(), networkSet.end());
					tx.exec();
				} else {
					auto tx = _redis->transaction(true, false);
					tx.sadd(setKey, networkSet.begin(), networkSet.end());
					tx.exec();
				}
				fprintf(stderr, "done.\n");
			}
		}

		if (++this->_ready == 2) {
			if (_waitNoticePrinted) {
				fprintf(stderr,"[%s] NOTICE: %.10llx controller PostgreSQL data download complete." ZT_EOL_S,_timestr(),(unsigned long long)_myAddress.toInt());
			}
			_readyLock.unlock();
		}
		fprintf(stderr, "network init done.\n");
	} catch (sw::redis::Error &e) {
		fprintf(stderr, "ERROR: Error initializing networks in Redis: %s\n", e.what());
		std::this_thread::sleep_for(std::chrono::milliseconds(5000));
		exit(-1);
	} catch (std::exception &e) {
		fprintf(stderr, "ERROR: Error initializing networks: %s\n", e.what());
		std::this_thread::sleep_for(std::chrono::milliseconds(5000));
		exit(-1);
	}
}

void PostgreSQL::initializeMembers()
{
	std::string memberId;
	std::string networkId;
	try {
		std::unordered_map<std::string, std::string> networkMembers;
		fprintf(stderr, "Initializing Members...\n");

		std::string setKeyBase = "network-nodes-all:{" + _myAddressStr + "}:";

		if (_redisMemberStatus) {
			fprintf(stderr, "Initialize Redis for members...\n");
			std::unique_lock<std::shared_mutex> l(_networks_l);
			std::unordered_set<std::string> deletes;
			for ( auto it : _networks) {
				uint64_t nwid_i = it.first;
				char nwidTmp[64] = {0};
				OSUtils::ztsnprintf(nwidTmp, sizeof(nwidTmp), "%.16llx", nwid_i);
				std::string nwid(nwidTmp);
				std::string key = setKeyBase + nwid;
				deletes.insert(key);
			}

			if (!deletes.empty()) {
				try {
					if (_rc->clusterMode) {
						auto tx = _cluster->transaction(_myAddressStr, true, false);
						for (std::string k : deletes) {
							tx.del(k);
						}
						tx.exec();
					} else {
						auto tx = _redis->transaction(true, false);
						for (std::string k : deletes) {
							tx.del(k);
						}
						tx.exec();
					}
				} catch (sw::redis::Error &e) {
					// ignore
				}
			}
		}

		char qbuf[2048];
		sprintf(qbuf,
			"SELECT m.id, m.network_id, m.active_bridge, m.authorized, m.capabilities, "
				"(EXTRACT(EPOCH FROM m.creation_time AT TIME ZONE 'UTC')*1000)::bigint, m.identity, "
				"(EXTRACT(EPOCH FROM m.last_authorized_time AT TIME ZONE 'UTC')*1000)::bigint, "
				"(EXTRACT(EPOCH FROM m.last_deauthorized_time AT TIME ZONE 'UTC')*1000)::bigint, "
				"m.remote_trace_level, m.remote_trace_target, m.tags, m.v_major, m.v_minor, m.v_rev, m.v_proto, "
				"m.no_auto_assign_ips, m.revision, m.sso_exempt, "
				"(CASE WHEN n.sso_enabled = TRUE AND m.sso_exempt = FALSE THEN "
				" ( "
				"	SELECT (EXTRACT(EPOCH FROM e.authentication_expiry_time)*1000)::bigint "
				"	FROM ztc_sso_expiry e "
				"	INNER JOIN ztc_network n1 "
				"	ON n1.id = e.network_id  AND n1.deleted = TRUE "
				"	WHERE e.network_id = m.network_id AND e.member_id = m.id AND n.sso_enabled = TRUE AND e.authentication_expiry_time IS NOT NULL "
				"	ORDER BY e.authentication_expiry_time DESC LIMIT 1 "
				" ) "
				" ELSE NULL "
				" END) AS authentication_expiry_time, "
				"ARRAY(SELECT DISTINCT address FROM ztc_member_ip_assignment WHERE member_id = m.id AND network_id = m.network_id) AS assigned_addresses "
			"FROM ztc_member m "
			"INNER JOIN ztc_network n "
			"	ON n.id = m.network_id "
			"WHERE n.controller_id = '%s' AND n.deleted = FALSE AND m.deleted = FALSE", _myAddressStr.c_str());
		auto c = _pool->borrow();
		auto c2 = _pool->borrow();
		pqxx::work w{*c->c};

		fprintf(stderr, "Load members from psql...\n");
		auto stream = pqxx::stream_from::query(w, qbuf);

		std::tuple<
			  std::string					// memberId
			, std::string					// memberId
			, std::optional<bool>			// activeBridge
			, std::optional<bool>			// authorized
			, std::optional<std::string>	// capabilities
			, std::optional<uint64_t>		// creationTime
			, std::optional<std::string>	// identity
			, std::optional<uint64_t>		// lastAuthorizedTime
			, std::optional<uint64_t>		// lastDeauthorizedTime
			, std::optional<int>			// remoteTraceLevel
			, std::optional<std::string>	// remoteTraceTarget
			, std::optional<std::string>	// tags
			, std::optional<int>			// vMajor
			, std::optional<int>			// vMinor
			, std::optional<int>			// vRev
			, std::optional<int>			// vProto
			, std::optional<bool>			// noAutoAssignIps
			, std::optional<uint64_t>		// revision
			, std::optional<bool>			// ssoExempt
			, std::optional<uint64_t>		// authenticationExpiryTime
			, std::string					// assignedAddresses
		> row;

		uint64_t count = 0;
		auto tmp = std::chrono::high_resolution_clock::now();
		uint64_t total = 0;
		while (stream >> row) {
			auto start = std::chrono::high_resolution_clock::now();
			json empty;
			json config;
			
			initMember(config);

			memberId = std::get<0>(row);
			networkId = std::get<1>(row);
			std::optional<bool> activeBridge = std::get<2>(row);
			std::optional<bool> authorized = std::get<3>(row);
			std::optional<std::string> capabilities = std::get<4>(row);
			std::optional<uint64_t> creationTime = std::get<5>(row);
			std::optional<std::string> identity = std::get<6>(row);
			std::optional<uint64_t> lastAuthorizedTime = std::get<7>(row);
			std::optional<uint64_t> lastDeauthorizedTime = std::get<8>(row);
			std::optional<int> remoteTraceLevel = std::get<9>(row);
			std::optional<std::string> remoteTraceTarget = std::get<10>(row);
			std::optional<std::string> tags = std::get<11>(row);
			std::optional<int> vMajor = std::get<12>(row);
			std::optional<int> vMinor = std::get<13>(row);
			std::optional<int> vRev = std::get<14>(row);
			std::optional<int> vProto = std::get<15>(row);
			std::optional<bool> noAutoAssignIps = std::get<16>(row);
			std::optional<uint64_t> revision = std::get<17>(row);
			std::optional<bool> ssoExempt = std::get<18>(row);
			std::optional<uint64_t> authenticationExpiryTime = std::get<19>(row);
			std::string assignedAddresses = std::get<20>(row);

			networkMembers.insert(std::pair<std::string, std::string>(setKeyBase+networkId, memberId));

			config["id"] = memberId;
			config["address"] = memberId;
			config["nwid"] = networkId;
			config["activeBridge"] = activeBridge.value_or(false);
			config["authorized"] = authorized.value_or(false);
			config["capabilities"] = json::parse(capabilities.value_or("[]"));
			config["creationTime"] = creationTime.value_or(0);
			config["identity"] = identity.value_or("");
			config["lastAuthorizedTime"] = lastAuthorizedTime.value_or(0);
			config["lastDeauthorizedTime"] = lastDeauthorizedTime.value_or(0);
			config["remoteTraceLevel"] = remoteTraceLevel.value_or(0);
		 	config["remoteTraceTarget"] = remoteTraceTarget.value_or("");
			config["tags"] = json::parse(tags.value_or("[]"));
			config["vMajor"] = vMajor.value_or(-1);
			config["vMinor"] = vMinor.value_or(-1);
			config["vRev"] = vRev.value_or(-1);
			config["vProto"] = vProto.value_or(-1);
			config["noAutoAssignIps"] = noAutoAssignIps.value_or(false);
			config["revision"] = revision.value_or(0);
			config["ssoExempt"] = ssoExempt.value_or(false);
			config["authenticationExpiryTime"] = authenticationExpiryTime.value_or(0);
			config["objtype"] = "member";
			config["ipAssignments"] = json::array();

			if (assignedAddresses != "{}") {
				std::string tmp = assignedAddresses.substr(1, assignedAddresses.size()-2);
				std::vector<std::string> addrs = split(tmp, ',');
				for (auto it = addrs.begin(); it != addrs.end(); ++it) {
					config["ipAssignments"].push_back(*it);
				}
			}

			Metrics::member_count++;

			_memberChanged(empty, config, false);

			memberId = "";
			networkId = "";

			auto end = std::chrono::high_resolution_clock::now();
			auto dur = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
			total += dur.count();
			++count;
			if (count > 0 && count % 10000 == 0) {
				fprintf(stderr, "Averaging %llu us per member\n", (total/count));
			}
		}
		if (count > 0) {
			fprintf(stderr, "Took %llu us per member to load\n", (total/count));
		}

		stream.complete();

		w.commit();
		_pool->unborrow(c2);
		_pool->unborrow(c);
		fprintf(stderr, "done.\n");

		if (!networkMembers.empty()) {
			if (_redisMemberStatus) {
				fprintf(stderr, "Load member data into redis...\n");
				if (_rc->clusterMode) {
					auto tx = _cluster->transaction(_myAddressStr, true, false);
					for (auto it : networkMembers) {
						tx.sadd(it.first, it.second);
					}
					tx.exec();
				} else {
					auto tx = _redis->transaction(true, false);
					for (auto it : networkMembers) {
						tx.sadd(it.first, it.second);
					}
					tx.exec();
				}
				fprintf(stderr, "done.\n");
			}
		}

		fprintf(stderr, "Done loading members...\n");

		if (++this->_ready == 2) {
			if (_waitNoticePrinted) {
				fprintf(stderr,"[%s] NOTICE: %.10llx controller PostgreSQL data download complete." ZT_EOL_S,_timestr(),(unsigned long long)_myAddress.toInt());
			}
			_readyLock.unlock();
		}
	} catch (sw::redis::Error &e) {
		fprintf(stderr, "ERROR: Error initializing members (redis): %s\n", e.what());
		exit(-1);
	} catch (std::exception &e) {
		fprintf(stderr, "ERROR: Error initializing member: %s-%s %s\n", networkId.c_str(), memberId.c_str(), e.what());
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
		for (int i = 0; i < (int)sizeof(hostnameTmp); ++i) {
			if ((hostnameTmp[i] == '.')||(hostnameTmp[i] == 0)) {
				hostnameTmp[i] = (char)0;
				break;
			}
		}
	}
	const char *controllerId = _myAddressStr.c_str();
	const char *publicIdentity = publicId;
	const char *hostname = hostnameTmp;

	while (_run == 1) {
		// fprintf(stderr, "%s: heartbeat\n", controllerId);
		auto c = _pool->borrow();
		int64_t ts = OSUtils::now();

		if(c->c) {
			std::string major = std::to_string(ZEROTIER_ONE_VERSION_MAJOR);
			std::string minor = std::to_string(ZEROTIER_ONE_VERSION_MINOR);
			std::string rev = std::to_string(ZEROTIER_ONE_VERSION_REVISION);
			std::string build = std::to_string(ZEROTIER_ONE_VERSION_BUILD);
			std::string now = std::to_string(ts);
			std::string host_port = std::to_string(_listenPort);
			std::string use_redis = (_rc != NULL) ? "true" : "false";
			std::string redis_mem_status = (_redisMemberStatus) ? "true" : "false";
			
			try {
				pqxx::work w{*c->c};

				pqxx::result res =
					w.exec0("INSERT INTO ztc_controller (id, cluster_host, last_alive, public_identity, v_major, v_minor, v_rev, v_build, host_port, use_redis, redis_member_status) "
							"VALUES ("+w.quote(controllerId)+", "+w.quote(hostname)+", TO_TIMESTAMP("+now+"::double precision/1000), "+
							w.quote(publicIdentity)+", "+major+", "+minor+", "+rev+", "+build+", "+host_port+", "+use_redis+", "+redis_mem_status+") "
							"ON CONFLICT (id) DO UPDATE SET cluster_host = EXCLUDED.cluster_host, last_alive = EXCLUDED.last_alive, "
							"public_identity = EXCLUDED.public_identity, v_major = EXCLUDED.v_major, v_minor = EXCLUDED.v_minor, "
							"v_rev = EXCLUDED.v_rev, v_build = EXCLUDED.v_rev, host_port = EXCLUDED.host_port, "
							"use_redis = EXCLUDED.use_redis, redis_member_status = EXCLUDED.redis_member_status");
				w.commit();
			} catch (std::exception &e) {
				fprintf(stderr, "%s: Heartbeat update failed: %s\n", controllerId, e.what());
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
				continue;
			}

		}
		_pool->unborrow(c);

		try {
			if (_redisMemberStatus) {
				if (_rc->clusterMode) {
					_cluster->zadd("controllers", "controllerId", ts);
				} else {
					_redis->zadd("controllers", "controllerId", ts);
				}
			}
		} catch (sw::redis::Error &e) {
			fprintf(stderr, "ERROR: Redis error in heartbeat thread: %s\n", e.what());
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}
	fprintf(stderr, "Exited heartbeat thread\n");
}

void PostgreSQL::membersDbWatcher()
{
	if (_rc) {
		_membersWatcher_Redis();
	} else {
		_membersWatcher_Postgres();
	}

	if (_run == 1) {
		fprintf(stderr, "ERROR: %s membersDbWatcher should still be running! Exiting Controller.\n", _myAddressStr.c_str());
		exit(9);
	}
	fprintf(stderr, "Exited membersDbWatcher\n");
}

void PostgreSQL::_membersWatcher_Postgres() {
	auto c = _pool->borrow();

	std::string stream = "member_" + _myAddressStr;

	fprintf(stderr, "Listening to member stream: %s\n", stream.c_str());
	MemberNotificationReceiver m(this, *c->c, stream);

	while(_run == 1) {
		c->c->await_notification(5, 0);
	}

	_pool->unborrow(c);
}

void PostgreSQL::_membersWatcher_Redis() {
	char buf[11] = {0};
	std::string key = "member-stream:{" + std::string(_myAddress.toString(buf)) + "}";
	std::string lastID = "0";
	fprintf(stderr, "Listening to member stream: %s\n", key.c_str());
	while (_run == 1) {
		try {
			json tmp;
			std::unordered_map<std::string, ItemStream> result;
			if (_rc->clusterMode) {
				_cluster->xread(key, lastID, std::chrono::seconds(1), 0, std::inserter(result, result.end()));
			} else {
				_redis->xread(key, lastID, std::chrono::seconds(1), 0, std::inserter(result, result.end()));
			}
			if (!result.empty()) {
				for (auto element : result) {
	#ifdef REDIS_TRACE
					fprintf(stdout, "Received notification from: %s\n", element.first.c_str());
	#endif
					for (auto rec : element.second) {
						std::string id = rec.first;
						auto attrs = rec.second;
	#ifdef REDIS_TRACE
						fprintf(stdout, "Record ID: %s\n", id.c_str());
						fprintf(stdout, "attrs len: %lu\n", attrs.size());
	#endif
						for (auto a : attrs) {
	#ifdef REDIS_TRACE
							fprintf(stdout, "key: %s\nvalue: %s\n", a.first.c_str(), a.second.c_str());
	#endif
							try {
								tmp = json::parse(a.second);
								json &ov = tmp["old_val"];
								json &nv = tmp["new_val"];
								json oldConfig, newConfig;
								if (ov.is_object()) oldConfig = ov;
								if (nv.is_object()) newConfig = nv;
								if (oldConfig.is_object()||newConfig.is_object()) {
									_memberChanged(oldConfig,newConfig,(this->_ready >= 2));
								}
							} catch (...) {
								fprintf(stderr, "json parse error in networkWatcher_Redis\n");
							}
						}
						if (_rc->clusterMode) {
							_cluster->xdel(key, id);
						} else {
							_redis->xdel(key, id);
						}
						lastID = id;
						Metrics::redis_mem_notification++;
					}
				}
			}
		} catch (sw::redis::Error &e) {
			fprintf(stderr, "Error in Redis members watcher: %s\n", e.what());
		}
	}
	fprintf(stderr, "membersWatcher ended\n");
}

void PostgreSQL::networksDbWatcher()
{
	if (_rc) {
		_networksWatcher_Redis();
	} else {
		_networksWatcher_Postgres();
	}

	if (_run == 1) {
		fprintf(stderr, "ERROR: %s networksDbWatcher should still be running! Exiting Controller.\n", _myAddressStr.c_str());
		exit(8);
	}
	fprintf(stderr, "Exited networksDbWatcher\n");
}

void PostgreSQL::_networksWatcher_Postgres() {
	std::string stream = "network_" + _myAddressStr;

	fprintf(stderr, "Listening to member stream: %s\n", stream.c_str());
	
	auto c = _pool->borrow();

	NetworkNotificationReceiver n(this, *c->c, stream);

	while(_run == 1) {
		c->c->await_notification(5,0);
	}
}

void PostgreSQL::_networksWatcher_Redis() {
	char buf[11] = {0};
	std::string key = "network-stream:{" + std::string(_myAddress.toString(buf)) + "}";
	std::string lastID = "0";
	while (_run == 1) {
		try {
			json tmp;
			std::unordered_map<std::string, ItemStream> result;
			if (_rc->clusterMode) {
				_cluster->xread(key, lastID, std::chrono::seconds(1), 0, std::inserter(result, result.end()));
			} else {
				_redis->xread(key, lastID, std::chrono::seconds(1), 0, std::inserter(result, result.end()));
			}
			
			if (!result.empty()) {
				for (auto element : result) {
#ifdef REDIS_TRACE
					fprintf(stdout, "Received notification from: %s\n", element.first.c_str());
#endif
					for (auto rec : element.second) {
						std::string id = rec.first;
						auto attrs = rec.second;
#ifdef REDIS_TRACE
						fprintf(stdout, "Record ID: %s\n", id.c_str());
						fprintf(stdout, "attrs len: %lu\n", attrs.size());
#endif
						for (auto a : attrs) {
#ifdef REDIS_TRACE
							fprintf(stdout, "key: %s\nvalue: %s\n", a.first.c_str(), a.second.c_str());
#endif
							try {
								tmp = json::parse(a.second);
								json &ov = tmp["old_val"];
								json &nv = tmp["new_val"];
								json oldConfig, newConfig;
								if (ov.is_object()) oldConfig = ov;
								if (nv.is_object()) newConfig = nv;
								if (oldConfig.is_object()||newConfig.is_object()) {
									_networkChanged(oldConfig,newConfig,(this->_ready >= 2));
								}
							} catch (...) {
								fprintf(stderr, "json parse error in networkWatcher_Redis\n");
							}
						}
						if (_rc->clusterMode) {
							_cluster->xdel(key, id);
						} else {
							_redis->xdel(key, id);
						}
						lastID = id;
					}
					Metrics::redis_net_notification++;
				}
			}
		} catch (sw::redis::Error &e) {
			fprintf(stderr, "Error in Redis networks watcher: %s\n", e.what());
		}
	}
	fprintf(stderr, "networksWatcher ended\n");
}

void PostgreSQL::commitThread()
{
	fprintf(stderr, "%s: commitThread start\n", _myAddressStr.c_str());
	std::pair<nlohmann::json,bool> qitem;
	while(_commitQueue.get(qitem)&(_run == 1)) {
		//fprintf(stderr, "commitThread tick\n");
		if (!qitem.first.is_object()) {
			fprintf(stderr, "not an object\n");
			continue;
		}

		std::shared_ptr<PostgresConnection> c;
		try {
			c = _pool->borrow();
		} catch (std::exception &e) {
			fprintf(stderr, "ERROR: %s\n", e.what());
			continue;
		}

		if (!c) {
			fprintf(stderr, "Error getting database connection\n");
			continue;
		}
		
		Metrics::pgsql_commit_ticks++;
		try {
			nlohmann::json &config = (qitem.first);
			const std::string objtype = config["objtype"];
			if (objtype == "member") {
				// fprintf(stderr, "%s: commitThread: member\n", _myAddressStr.c_str());
				std::string memberId;
				std::string networkId;
				try {
					pqxx::work w(*c->c);

					memberId = config["id"];
					networkId = config["nwid"];
					
					std::string target = "NULL";
					if (!config["remoteTraceTarget"].is_null()) {
						target = config["remoteTraceTarget"];
					}
					
					pqxx::row nwrow = w.exec_params1("SELECT COUNT(id) FROM ztc_network WHERE id = $1", networkId);
					int nwcount = nwrow[0].as<int>();

					if (nwcount != 1) {
						fprintf(stderr, "network %s does not exist.  skipping member upsert\n", networkId.c_str());
						w.abort();
						_pool->unborrow(c);
						continue;
					}


					pqxx::row mrow = w.exec_params1("SELECT COUNT(id) FROM ztc_member WHERE id = $1 AND network_id = $2", memberId, networkId);
					int membercount = mrow[0].as<int>();

					bool isNewMember = false;
					if (membercount == 0) {
						// new member
						isNewMember = true;
						pqxx::result res = w.exec_params0(
							"INSERT INTO ztc_member (id, network_id, active_bridge, authorized, capabilities, "
							"identity, last_authorized_time, last_deauthorized_time, no_auto_assign_ips, "
							"remote_trace_level, remote_trace_target, revision, tags, v_major, v_minor, v_rev, v_proto) "
							"VALUES ($1, $2, $3, $4, $5, $6, "
							"TO_TIMESTAMP($7::double precision/1000), TO_TIMESTAMP($8::double precision/1000), "
							"$9, $10, $11, $12, $13, $14, $15, $16, $17)",
							memberId,
							networkId,
							(bool)config["activeBridge"],
							(bool)config["authorized"],
							OSUtils::jsonDump(config["capabilities"], -1),
							OSUtils::jsonString(config["identity"], ""),
							(uint64_t)config["lastAuthorizedTime"],
							(uint64_t)config["lastDeauthorizedTime"],
							(bool)config["noAutoAssignIps"],
							(int)config["remoteTraceLevel"],
							target,
							(uint64_t)config["revision"],
							OSUtils::jsonDump(config["tags"], -1),
							(int)config["vMajor"],
							(int)config["vMinor"],
							(int)config["vRev"],
							(int)config["vProto"]);
					} else {
						// existing member
						pqxx::result res = w.exec_params0(
							"UPDATE ztc_member "
							"SET active_bridge = $3, authorized = $4, capabilities = $5, identity = $6, "
							"last_authorized_time = TO_TIMESTAMP($7::double precision/1000), "
							"last_deauthorized_time = TO_TIMESTAMP($8::double precision/1000), "
							"no_auto_assign_ips = $9, remote_trace_level = $10, remote_trace_target= $11, "
							"revision = $12, tags = $13, v_major = $14, v_minor = $15, v_rev = $16, v_proto = $17 "
							"WHERE id = $1 AND network_id = $2",
							memberId,
							networkId,
							(bool)config["activeBridge"],
							(bool)config["authorized"],
							OSUtils::jsonDump(config["capabilities"], -1),
							OSUtils::jsonString(config["identity"], ""),
							(uint64_t)config["lastAuthorizedTime"],
							(uint64_t)config["lastDeauthorizedTime"],
							(bool)config["noAutoAssignIps"],
							(int)config["remoteTraceLevel"],
							target,
							(uint64_t)config["revision"],
							OSUtils::jsonDump(config["tags"], -1),
							(int)config["vMajor"],
							(int)config["vMinor"],
							(int)config["vRev"],
							(int)config["vProto"]
						);
					}

					if (!isNewMember) {
						pqxx::result res = w.exec_params0("DELETE FROM ztc_member_ip_assignment WHERE member_id = $1 AND network_id = $2",
							memberId, networkId);
					}

					std::vector<std::string> assignments;
					bool ipAssignError = false;
					for (auto i = config["ipAssignments"].begin(); i != config["ipAssignments"].end(); ++i) {
						std::string addr = *i;

						if (std::find(assignments.begin(), assignments.end(), addr) != assignments.end()) {
							continue;
						}

						pqxx::result res = w.exec_params0(
							"INSERT INTO ztc_member_ip_assignment (member_id, network_id, address) VALUES ($1, $2, $3) ON CONFLICT (network_id, member_id, address) DO NOTHING",
							memberId, networkId, addr);

						assignments.push_back(addr);
					}
					if (ipAssignError) {
						fprintf(stderr, "%s: ipAssignError\n", _myAddressStr.c_str());
						w.abort();
						_pool->unborrow(c);
						c.reset();
						continue;
					}

					w.commit();

					if (_smee != NULL && isNewMember) {
						notifyNewMember(networkId, memberId);
					} else {
						if (_smee == NULL) {
							fprintf(stderr, "smee is NULL\n");
						}
						if (!isNewMember) {
							fprintf(stderr, "nt a new member\n");
						}
					}

					const uint64_t nwidInt = OSUtils::jsonIntHex(config["nwid"], 0ULL);
					const uint64_t memberidInt = OSUtils::jsonIntHex(config["id"], 0ULL);
					if (nwidInt && memberidInt) {
						nlohmann::json nwOrig;
						nlohmann::json memOrig;

						nlohmann::json memNew(config);

						get(nwidInt, nwOrig, memberidInt, memOrig);

						_memberChanged(memOrig, memNew, qitem.second);
					} else {
						fprintf(stderr, "%s: Can't notify of change.  Error parsing nwid or memberid: %llu-%llu\n", _myAddressStr.c_str(), (unsigned long long)nwidInt, (unsigned long long)memberidInt);
					}
				} catch (std::exception &e) {
					fprintf(stderr, "%s ERROR: Error updating member %s-%s: %s\n", _myAddressStr.c_str(), networkId.c_str(), memberId.c_str(), e.what());
				}
			} else if (objtype == "network") {
				try {
					// fprintf(stderr, "%s: commitThread: network\n", _myAddressStr.c_str());
					pqxx::work w(*c->c);

					std::string id = config["id"];
					std::string remoteTraceTarget = "";
					if(!config["remoteTraceTarget"].is_null()) {
						remoteTraceTarget = config["remoteTraceTarget"];
					}
					std::string rulesSource = "";
					if (config["rulesSource"].is_string()) {
						rulesSource = config["rulesSource"];
					}

					// This ugly query exists because when we want to mirror networks to/from
					// another data store (e.g. FileDB or LFDB) it is possible to get a network
					// that doesn't exist in Central's database. This does an upsert and sets
					// the owner_id to the "first" global admin in the user DB if the record
					// did not previously exist. If the record already exists owner_id is left
					// unchanged, so owner_id should be left out of the update clause.
					pqxx::result res = w.exec_params0(
						"INSERT INTO ztc_network (id, creation_time, owner_id, controller_id, capabilities, enable_broadcast, "
						"last_modified, mtu, multicast_limit, name, private, "
						"remote_trace_level, remote_trace_target, rules, rules_source, "
						"tags, v4_assign_mode, v6_assign_mode, sso_enabled) VALUES ("
						"$1, TO_TIMESTAMP($5::double precision/1000), "
						"(SELECT user_id AS owner_id FROM ztc_global_permissions WHERE authorize = true AND del = true AND modify = true AND read = true LIMIT 1),"
						"$2, $3, $4, TO_TIMESTAMP($5::double precision/1000), "
						"$6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16, $17) "
						"ON CONFLICT (id) DO UPDATE set controller_id = EXCLUDED.controller_id, "
						"capabilities = EXCLUDED.capabilities, enable_broadcast = EXCLUDED.enable_broadcast, "
						"last_modified = EXCLUDED.last_modified, mtu = EXCLUDED.mtu, "
						"multicast_limit = EXCLUDED.multicast_limit, name = EXCLUDED.name, "
						"private = EXCLUDED.private, remote_trace_level = EXCLUDED.remote_trace_level, "
						"remote_trace_target = EXCLUDED.remote_trace_target, rules = EXCLUDED.rules, "
						"rules_source = EXCLUDED.rules_source, tags = EXCLUDED.tags, "
						"v4_assign_mode = EXCLUDED.v4_assign_mode, v6_assign_mode = EXCLUDED.v6_assign_mode, "
						"sso_enabled = EXCLUDED.sso_enabled",
						id,
						_myAddressStr,
						OSUtils::jsonDump(config["capabilities"], -1),
						(bool)config["enableBroadcast"],
						OSUtils::now(),
						(int)config["mtu"],
						(int)config["multicastLimit"],
						OSUtils::jsonString(config["name"],""),
						(bool)config["private"],
						(int)config["remoteTraceLevel"],
						remoteTraceTarget,
						OSUtils::jsonDump(config["rules"], -1),
						rulesSource,
						OSUtils::jsonDump(config["tags"], -1),
						OSUtils::jsonDump(config["v4AssignMode"],-1),
						OSUtils::jsonDump(config["v6AssignMode"], -1),
						OSUtils::jsonBool(config["ssoEnabled"], false));

					res = w.exec_params0("DELETE FROM ztc_network_assignment_pool WHERE network_id = $1", 0);

					auto pool = config["ipAssignmentPools"];
					bool err = false;
					for (auto i = pool.begin(); i != pool.end(); ++i) {
						std::string start = (*i)["ipRangeStart"];
						std::string end = (*i)["ipRangeEnd"];

						res = w.exec_params0(
							"INSERT INTO ztc_network_assignment_pool (network_id, ip_range_start, ip_range_end) "
							"VALUES ($1, $2, $3)", id, start, end);
					}

					res = w.exec_params0("DELETE FROM ztc_network_route WHERE network_id = $1", id);

					auto routes = config["routes"];
					err = false;
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
						std::string targetBits = target[1];
						std::string via = "NULL";
						if (!(*i)["via"].is_null()) {
							via = (*i)["via"];
						}

						res = w.exec_params0("INSERT INTO ztc_network_route (network_id, address, bits, via) VALUES ($1, $2, $3, $4)",
							id, targetAddr, targetBits, (via == "NULL" ? NULL : via.c_str()));
					}
					if (err) {
						fprintf(stderr, "%s: route add error\n", _myAddressStr.c_str());
						w.abort();
						_pool->unborrow(c);
						continue;
					}

					auto dns = config["dns"];
					std::string domain = dns["domain"];
					std::stringstream servers;
					servers << "{";
					for (auto j = dns["servers"].begin(); j < dns["servers"].end(); ++j) {
						servers << *j;
						if ( (j+1) != dns["servers"].end()) {
							servers << ",";
						}
					}
					servers << "}";

					std::string s = servers.str();

					res = w.exec_params0("INSERT INTO ztc_network_dns (network_id, domain, servers) VALUES ($1, $2, $3) ON CONFLICT (network_id) DO UPDATE SET domain = EXCLUDED.domain, servers = EXCLUDED.servers",
						id, domain, s);

					w.commit();

					const uint64_t nwidInt = OSUtils::jsonIntHex(config["nwid"], 0ULL);
					if (nwidInt) {
						nlohmann::json nwOrig;
						nlohmann::json nwNew(config);

						get(nwidInt, nwOrig);

						_networkChanged(nwOrig, nwNew, qitem.second);
					} else {
						fprintf(stderr, "%s: Can't notify network changed: %llu\n", _myAddressStr.c_str(), (unsigned long long)nwidInt);
					}
				} catch (std::exception &e) {
					fprintf(stderr, "%s ERROR: Error updating network: %s\n", _myAddressStr.c_str(), e.what());
				}
				if (_redisMemberStatus) {
					try {
						std::string id = config["id"];
						std::string controllerId = _myAddressStr.c_str();
						std::string key = "networks:{" + controllerId + "}";
						if (_rc->clusterMode) {
							_cluster->sadd(key, id);
						} else {
							_redis->sadd(key, id);
						}
					} catch (sw::redis::Error &e) {
						fprintf(stderr, "ERROR: Error adding network to Redis: %s\n", e.what());
					}
				}
			} else if (objtype == "_delete_network") {
				// fprintf(stderr, "%s: commitThread: delete network\n", _myAddressStr.c_str());
				try {
					pqxx::work w(*c->c);

					std::string networkId = config["nwid"];

					pqxx::result res = w.exec_params0("UPDATE ztc_network SET deleted = true WHERE id = $1",
						networkId);

					w.commit();
				} catch (std::exception &e) {
					fprintf(stderr, "%s ERROR: Error deleting network: %s\n", _myAddressStr.c_str(), e.what());
				}
				if (_redisMemberStatus) {
					try {
						std::string id = config["id"];
						std::string controllerId = _myAddressStr.c_str();
						std::string key = "networks:{" + controllerId + "}";
						if (_rc->clusterMode) {
							_cluster->srem(key, id);
							_cluster->del("network-nodes-online:{"+controllerId+"}:"+id);
						} else {
							_redis->srem(key, id);
							_redis->del("network-nodes-online:{"+controllerId+"}:"+id);
						}
					} catch (sw::redis::Error &e) {
						fprintf(stderr, "ERROR: Error adding network to Redis: %s\n", e.what());
					}
				}

			} else if (objtype == "_delete_member") {
				// fprintf(stderr, "%s commitThread: delete member\n", _myAddressStr.c_str());
				try {
					pqxx::work w(*c->c);

					std::string memberId = config["id"];
					std::string networkId = config["nwid"];

					pqxx::result res = w.exec_params0(
						"UPDATE ztc_member SET hidden = true, deleted = true WHERE id = $1 AND network_id = $2",
						memberId, networkId);

					w.commit();
				} catch (std::exception &e) {
					fprintf(stderr, "%s ERROR: Error deleting member: %s\n", _myAddressStr.c_str(), e.what());
				}
				if (_redisMemberStatus) {
					try {
						std::string memberId = config["id"];
						std::string networkId = config["nwid"];
						std::string controllerId = _myAddressStr.c_str();
						std::string key = "network-nodes-all:{" + controllerId + "}:" + networkId;
						if (_rc->clusterMode) {
							_cluster->srem(key, memberId);
							_cluster->del("member:{"+controllerId+"}:"+networkId+":"+memberId);
						} else {
							_redis->srem(key, memberId);
							_redis->del("member:{"+controllerId+"}:"+networkId+":"+memberId);
						}
					} catch (sw::redis::Error &e) {
						fprintf(stderr, "ERROR: Error deleting member from Redis: %s\n", e.what());
					}
				}
			} else {
				fprintf(stderr, "%s ERROR: unknown objtype\n", _myAddressStr.c_str());
			}
		} catch (std::exception &e) {
			fprintf(stderr, "%s ERROR: Error getting objtype: %s\n", _myAddressStr.c_str(), e.what());
		}
		_pool->unborrow(c);
		c.reset();
	}

	fprintf(stderr, "%s commitThread finished\n", _myAddressStr.c_str());
}

void PostgreSQL::notifyNewMember(const std::string &networkID, const std::string &memberID) {
	std::shared_ptr<PostgresConnection> c;
	try {	
		c = _pool->borrow();
	} catch (std::exception &e) {
		fprintf(stderr, "ERROR: %s\n", e.what());
		return;
	}

	try {
		pqxx::work w(*c->c);
		
		// TODO: Add check for active subscription

		auto res = w.exec_params("SELECT h.hook_id "
			"FROM ztc_hook h "
			"INNER JOIN ztc_hook_hook_types ht "
				"ON ht.hook_id = h.hook_id "
			"INNER JOIN ztc_org o "
				"ON o.org_id = h.org_id "
			"INNER JOIN ztc_user u "
				"ON u.id = o.owner_id "
			"INNER JOIN ztc_network n "
				"ON n.owner_id = u.id "
			"WHERE n.id = $1 "
			"AND ht.hook_type = 'NETWORK_JOIN'", networkID);

		for (auto const &row: res) {
			std::string hookURL = row[0].as<std::string>();
			smeeclient::smee_client_notify_network_joined(
			_smee,
				networkID.c_str(),
				memberID.c_str(),
				hookURL.c_str(),
				NULL
			);
		}

		_pool->unborrow(c);
	} catch (std::exception &e) {
		fprintf(stderr, "ERROR: %s\n", e.what());
		return;
	}
}

void PostgreSQL::onlineNotificationThread()
{
 	waitForReady();
	if (_redisMemberStatus) {
	    onlineNotification_Redis();
	} else {
	    onlineNotification_Postgres();
	}
}

/**
 * ONLY UNCOMMENT FOR TEMPORARY DB MAINTENANCE
 *
 * This define temporarily turns off writing to the member status table
 * so it can be reindexed when the indexes get too large.
 */

// #define DISABLE_MEMBER_STATUS 1

void PostgreSQL::onlineNotification_Postgres()
{
	_connected = 1;

	nlohmann::json jtmp1, jtmp2;
	while (_run == 1) {
		auto c = _pool->borrow();
		auto c2 = _pool->borrow();
		try {
			fprintf(stderr, "%s onlineNotification_Postgres\n", _myAddressStr.c_str());
			std::unordered_map< std::pair<uint64_t,uint64_t>,std::pair<int64_t,InetAddress>,_PairHasher > lastOnline;
			{
				std::lock_guard<std::mutex> l(_lastOnline_l);
				lastOnline.swap(_lastOnline);
			}

#ifndef DISABLE_MEMBER_STATUS
			pqxx::work w(*c->c);
			pqxx::work w2(*c2->c);

			fprintf(stderr, "online notification tick\n");
			
			bool firstRun = true;
			bool memberAdded = false;
			int updateCount = 0;

			pqxx::pipeline pipe(w);

			for (auto i=lastOnline.begin(); i != lastOnline.end(); ++i) {
				updateCount += 1;
				uint64_t nwid_i = i->first.first;
				char nwidTmp[64];
				char memTmp[64];
				char ipTmp[64];
				OSUtils::ztsnprintf(nwidTmp,sizeof(nwidTmp), "%.16llx", nwid_i);
				OSUtils::ztsnprintf(memTmp,sizeof(memTmp), "%.10llx", i->first.second);

				if(!get(nwid_i, jtmp1, i->first.second, jtmp2)) {
					continue; // skip non existent networks/members
				}

				std::string networkId(nwidTmp);
				std::string memberId(memTmp);

				try {
					pqxx::row r = w2.exec_params1("SELECT id, network_id FROM ztc_member WHERE network_id = $1 AND id = $2",
						networkId, memberId);		
				} catch (pqxx::unexpected_rows &e) {
					continue;
				}

				int64_t ts = i->second.first;
				std::string ipAddr = i->second.second.toIpString(ipTmp);
				std::string timestamp = std::to_string(ts);

				std::stringstream memberUpdate;
				memberUpdate << "INSERT INTO ztc_member_status (network_id, member_id, address, last_updated) VALUES "
					<< "('" << networkId << "', '" << memberId << "', ";
				if (ipAddr.empty()) {
					memberUpdate << "NULL, ";
				} else {
					memberUpdate << "'" << ipAddr << "', ";
				}
				memberUpdate << "TO_TIMESTAMP(" << timestamp << "::double precision/1000)) "
					<< " ON CONFLICT (network_id, member_id) DO UPDATE SET address = EXCLUDED.address, last_updated = EXCLUDED.last_updated";

				pipe.insert(memberUpdate.str());
				Metrics::pgsql_node_checkin++;
			}
			while(!pipe.empty()) {
				pipe.retrieve();
			}

			pipe.complete();
			w.commit();
			fprintf(stderr, "%s: Updated online status of %d members\n", _myAddressStr.c_str(), updateCount);
#endif
		} catch (std::exception &e) {
			fprintf(stderr, "%s: error in onlinenotification thread: %s\n", _myAddressStr.c_str(), e.what());
		} 
		_pool->unborrow(c2);
		_pool->unborrow(c);

		ConnectionPoolStats stats = _pool->get_stats();
		fprintf(stderr, "%s pool stats: in use size: %llu, available size: %llu, total: %llu\n",
			_myAddressStr.c_str(), stats.borrowed_size, stats.pool_size, (stats.borrowed_size + stats.pool_size));

		std::this_thread::sleep_for(std::chrono::seconds(10));
	}
	fprintf(stderr, "%s: Fell out of run loop in onlineNotificationThread\n", _myAddressStr.c_str());
	if (_run == 1) {
		fprintf(stderr, "ERROR: %s onlineNotificationThread should still be running! Exiting Controller.\n", _myAddressStr.c_str());
		exit(6);
	}
}

void PostgreSQL::onlineNotification_Redis()
{
	_connected = 1;
	
	char buf[11] = {0};
	std::string controllerId = std::string(_myAddress.toString(buf));

	while (_run == 1) {
		fprintf(stderr, "onlineNotification tick\n");
		auto start = std::chrono::high_resolution_clock::now();
		uint64_t count = 0;

		std::unordered_map< std::pair<uint64_t,uint64_t>,std::pair<int64_t,InetAddress>,_PairHasher > lastOnline;
		{
			std::lock_guard<std::mutex> l(_lastOnline_l);
			lastOnline.swap(_lastOnline);
		}
		try {
			if (!lastOnline.empty()) {
				if (_rc->clusterMode) {
					auto tx = _cluster->transaction(controllerId, true, false);
					count = _doRedisUpdate(tx, controllerId, lastOnline);
				} else {
					auto tx = _redis->transaction(true, false);
					count = _doRedisUpdate(tx, controllerId, lastOnline);
				}
			}
		} catch (sw::redis::Error &e) {
			fprintf(stderr, "Error in online notification thread (redis): %s\n", e.what());
		}

		auto end = std::chrono::high_resolution_clock::now();
		auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
		auto total = dur.count();

		fprintf(stderr, "onlineNotification ran in %llu ms\n", total);

		std::this_thread::sleep_for(std::chrono::seconds(5));
	}
}

uint64_t PostgreSQL::_doRedisUpdate(sw::redis::Transaction &tx, std::string &controllerId,
	std::unordered_map< std::pair<uint64_t,uint64_t>,std::pair<int64_t,InetAddress>,_PairHasher > &lastOnline) 

{
	nlohmann::json jtmp1, jtmp2;
	uint64_t count = 0;
	for (auto i=lastOnline.begin(); i != lastOnline.end(); ++i) {
		uint64_t nwid_i = i->first.first;
		uint64_t memberid_i = i->first.second;
		char nwidTmp[64];
		char memTmp[64];
		char ipTmp[64];
		OSUtils::ztsnprintf(nwidTmp,sizeof(nwidTmp), "%.16llx", nwid_i);
		OSUtils::ztsnprintf(memTmp,sizeof(memTmp), "%.10llx", memberid_i);

		if (!get(nwid_i, jtmp1, memberid_i, jtmp2)){
			continue;  // skip non existent members/networks
		}

		std::string networkId(nwidTmp);
		std::string memberId(memTmp);

		int64_t ts = i->second.first;
		std::string ipAddr = i->second.second.toIpString(ipTmp);
		std::string timestamp = std::to_string(ts);

		std::unordered_map<std::string, std::string> record = {
			{"id", memberId},
			{"address", ipAddr},
			{"last_updated", std::to_string(ts)}
		};
		tx.zadd("nodes-online:{"+controllerId+"}", memberId, ts)
			.zadd("nodes-online2:{"+controllerId+"}", networkId+"-"+memberId, ts)
			.zadd("network-nodes-online:{"+controllerId+"}:"+networkId, memberId, ts)
			.zadd("active-networks:{"+controllerId+"}", networkId, ts)
			.sadd("network-nodes-all:{"+controllerId+"}:"+networkId, memberId)
			.hmset("member:{"+controllerId+"}:"+networkId+":"+memberId, record.begin(), record.end());
		++count;
		Metrics::redis_node_checkin++;
	}

	// expire records from all-nodes and network-nodes member list
	uint64_t expireOld = OSUtils::now() - 300000;
	
	tx.zremrangebyscore("nodes-online:{"+controllerId+"}",
						sw::redis::RightBoundedInterval<double>(expireOld,
																sw::redis::BoundType::LEFT_OPEN));
	tx.zremrangebyscore("nodes-online2:{"+controllerId+"}",
						sw::redis::RightBoundedInterval<double>(expireOld,
																sw::redis::BoundType::LEFT_OPEN));
	tx.zremrangebyscore("active-networks:{"+controllerId+"}",
						sw::redis::RightBoundedInterval<double>(expireOld,
																sw::redis::BoundType::LEFT_OPEN));
	{
		std::shared_lock<std::shared_mutex> l(_networks_l);
		for (const auto &it : _networks) {
			uint64_t nwid_i = it.first;
			char nwidTmp[64];
			OSUtils::ztsnprintf(nwidTmp,sizeof(nwidTmp), "%.16llx", nwid_i);
			tx.zremrangebyscore("network-nodes-online:{"+controllerId+"}:"+nwidTmp, 
				 sw::redis::RightBoundedInterval<double>(expireOld, sw::redis::BoundType::LEFT_OPEN));
		}
	}
	tx.exec();
	fprintf(stderr, "%s: Updated online status of %d members\n", _myAddressStr.c_str(), count);

	return count;
}


#endif //ZT_CONTROLLER_USE_LIBPQ
