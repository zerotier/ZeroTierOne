/*
 * Copyright (c)2025 ZeroTier, Inc.
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

#include "CV2.hpp"

#ifdef ZT_CONTROLLER_USE_LIBPQ

#include "../node/Constants.hpp"
#include "../node/SHA512.hpp"
#include "../version.h"
#include "CtlUtil.hpp"
#include "EmbeddedNetworkController.hpp"
#include "opentelemetry/trace/provider.h"

#include <chrono>
#include <climits>
#include <iomanip>
#include <libpq-fe.h>
#include <sstream>

using json = nlohmann::json;

namespace {

}

using namespace ZeroTier;

CV2::CV2(const Identity& myId, const char* path, int listenPort) : DB(), _pool(), _myId(myId), _myAddress(myId.address()), _ready(0), _connected(1), _run(1), _waitNoticePrinted(false), _listenPort(listenPort)
{
	auto provider = opentelemetry::trace::Provider::GetTracerProvider();
	auto tracer = provider->GetTracer("cv2");
	auto span = tracer->StartSpan("cv2::CV2");
	auto scope = tracer->WithActiveSpan(span);

	fprintf(stderr, "CV2::CV2\n");
	char myAddress[64];
	_myAddressStr = myId.address().toString(myAddress);

	_connString = std::string(path);

	auto f = std::make_shared<PostgresConnFactory>(_connString);
	_pool = std::make_shared<ConnectionPool<PostgresConnection> >(15, 5, std::static_pointer_cast<ConnectionFactory>(f));

	memset(_ssoPsk, 0, sizeof(_ssoPsk));
	char* const ssoPskHex = getenv("ZT_SSO_PSK");
#ifdef ZT_TRACE
	fprintf(stderr, "ZT_SSO_PSK: %s\n", ssoPskHex);
#endif
	if (ssoPskHex) {
		// SECURITY: note that ssoPskHex will always be null-terminated if libc actually
		// returns something non-NULL. If the hex encodes something shorter than 48 bytes,
		// it will be padded at the end with zeroes. If longer, it'll be truncated.
		Utils::unhex(ssoPskHex, _ssoPsk, sizeof(_ssoPsk));
	}

	_readyLock.lock();

	fprintf(stderr, "[%s] NOTICE: %.10llx controller PostgreSQL waiting for initial data download..." ZT_EOL_S, ::_timestr(), (unsigned long long)_myAddress.toInt());
	_waitNoticePrinted = true;

	initializeNetworks();
	initializeMembers();

	_heartbeatThread = std::thread(&CV2::heartbeat, this);
	_membersDbWatcher = std::thread(&CV2::membersDbWatcher, this);
	_networksDbWatcher = std::thread(&CV2::networksDbWatcher, this);
	for (int i = 0; i < ZT_CENTRAL_CONTROLLER_COMMIT_THREADS; ++i) {
		_commitThread[i] = std::thread(&CV2::commitThread, this);
	}
	_onlineNotificationThread = std::thread(&CV2::onlineNotificationThread, this);
}

CV2::~CV2()
{
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

bool CV2::waitForReady()
{
	while (_ready < 2) {
		_readyLock.lock();
		_readyLock.unlock();
	}
	return true;
}

bool CV2::isReady()
{
	return (_ready == 2) && _connected;
}

void CV2::_memberChanged(nlohmann::json& old, nlohmann::json& memberConfig, bool notifyListeners)
{
	DB::_memberChanged(old, memberConfig, notifyListeners);
}

void CV2::_networkChanged(nlohmann::json& old, nlohmann::json& networkConfig, bool notifyListeners)
{
	DB::_networkChanged(old, networkConfig, notifyListeners);
}

bool CV2::save(nlohmann::json& record, bool notifyListeners)
{
	auto provider = opentelemetry::trace::Provider::GetTracerProvider();
	auto tracer = provider->GetTracer("cv2");
	auto span = tracer->StartSpan("cv2::save");
	auto scope = tracer->WithActiveSpan(span);

	bool modified = false;
	try {
		if (! record.is_object()) {
			fprintf(stderr, "record is not an object?!?\n");
			return false;
		}
		const std::string objtype = record["objtype"];
		if (objtype == "network") {
			auto nspan = tracer->StartSpan("cv2::save::network");
			auto nscope = tracer->WithActiveSpan(nspan);

			// fprintf(stderr, "network save\n");
			const uint64_t nwid = OSUtils::jsonIntHex(record["id"], 0ULL);
			if (nwid) {
				nlohmann::json old;
				get(nwid, old);
				if ((! old.is_object()) || (! _compareRecords(old, record))) {
					record["revision"] = OSUtils::jsonInt(record["revision"], 0ULL) + 1ULL;
					_commitQueue.post(std::pair<nlohmann::json, bool>(record, notifyListeners));
					modified = true;
				}
			}
		}
		else if (objtype == "member") {
			auto mspan = tracer->StartSpan("cv2::save::member");
			auto mscope = tracer->WithActiveSpan(mspan);

			std::string networkId = record["nwid"];
			std::string memberId = record["id"];
			const uint64_t nwid = OSUtils::jsonIntHex(record["nwid"], 0ULL);
			const uint64_t id = OSUtils::jsonIntHex(record["id"], 0ULL);
			// fprintf(stderr, "member save %s-%s\n", networkId.c_str(), memberId.c_str());
			if ((id) && (nwid)) {
				nlohmann::json network, old;
				get(nwid, network, id, old);
				if ((! old.is_object()) || (! _compareRecords(old, record))) {
					// fprintf(stderr, "commit queue post\n");
					record["revision"] = OSUtils::jsonInt(record["revision"], 0ULL) + 1ULL;
					_commitQueue.post(std::pair<nlohmann::json, bool>(record, notifyListeners));
					modified = true;
				}
				else {
					// fprintf(stderr, "no change\n");
				}
			}
		}
		else {
			fprintf(stderr, "uhh waaat\n");
		}
	}
	catch (std::exception& e) {
		fprintf(stderr, "Error on PostgreSQL::save: %s\n", e.what());
	}
	catch (...) {
		fprintf(stderr, "Unknown error on PostgreSQL::save\n");
	}
	return modified;
}

void CV2::eraseNetwork(const uint64_t networkId)
{
	auto provider = opentelemetry::trace::Provider::GetTracerProvider();
	auto tracer = provider->GetTracer("cv2");
	auto span = tracer->StartSpan("cv2::eraseNetwork");
	auto scope = tracer->WithActiveSpan(span);
	char networkIdStr[17];
	std::string nwid = Utils::hex(networkId, networkIdStr);
	span->SetAttribute("network_id", nwid);

	fprintf(stderr, "CV2::eraseNetwork\n");
	waitForReady();
	std::pair<nlohmann::json, bool> tmp;
	tmp.first["id"] = nwid;
	tmp.first["objtype"] = "_delete_network";
	tmp.second = true;
	_commitQueue.post(tmp);
	// nlohmann::json nullJson;
	//_networkChanged(tmp.first, nullJson, isReady());
}

void CV2::eraseMember(const uint64_t networkId, const uint64_t memberId)
{
	auto provider = opentelemetry::trace::Provider::GetTracerProvider();
	auto tracer = provider->GetTracer("cv2");
	auto span = tracer->StartSpan("cv2::eraseMember");
	auto scope = tracer->WithActiveSpan(span);
	char networkIdStr[17];
	char memberIdStr[11];
	span->SetAttribute("network_id", Utils::hex(networkId, networkIdStr));
	span->SetAttribute("member_id", Utils::hex10(memberId, memberIdStr));

	fprintf(stderr, "PostgreSQL::eraseMember\n");
	char tmp2[24];
	waitForReady();
	std::pair<nlohmann::json, bool> tmp, nw;
	Utils::hex(networkId, tmp2);
	tmp.first["nwid"] = tmp2;
	Utils::hex(memberId, tmp2);
	tmp.first["id"] = tmp2;
	tmp.first["objtype"] = "_delete_member";
	tmp.second = true;
	_commitQueue.post(tmp);
	// nlohmann::json nullJson;
	//_memberChanged(tmp.first, nullJson, isReady());
}

void CV2::nodeIsOnline(const uint64_t networkId, const uint64_t memberId, const InetAddress& physicalAddress, const char* osArch)
{
	auto provider = opentelemetry::trace::Provider::GetTracerProvider();
	auto tracer = provider->GetTracer("cv2");
	auto span = tracer->StartSpan("cv2::nodeIsOnline");
	auto scope = tracer->WithActiveSpan(span);
	char networkIdStr[17];
	char memberIdStr[11];
	char ipAddressStr[INET6_ADDRSTRLEN];
	span->SetAttribute("network_id", Utils::hex(networkId, networkIdStr));
	span->SetAttribute("member_id", Utils::hex10(memberId, memberIdStr));
	span->SetAttribute("physical_address", ipAddressStr);
	span->SetAttribute("os_arch", osArch);

	std::lock_guard<std::mutex> l(_lastOnline_l);
	NodeOnlineRecord& i = _lastOnline[std::pair<uint64_t, uint64_t>(networkId, memberId)];
	i.lastSeen = OSUtils::now();
	if (physicalAddress) {
		i.physicalAddress = physicalAddress;
	}
	i.osArch = std::string(osArch);
}

void CV2::nodeIsOnline(const uint64_t networkId, const uint64_t memberId, const InetAddress& physicalAddress)
{
	this->nodeIsOnline(networkId, memberId, physicalAddress, "unknown/unknown");
}

AuthInfo CV2::getSSOAuthInfo(const nlohmann::json& member, const std::string& redirectURL)
{
	// TODO: Redo this for CV2
	auto provider = opentelemetry::trace::Provider::GetTracerProvider();
	auto tracer = provider->GetTracer("cv2");
	auto span = tracer->StartSpan("cv2::getSSOAuthInfo");
	auto scope = tracer->WithActiveSpan(span);

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

	char authenticationURL[4096] = { 0 };
	AuthInfo info;
	info.enabled = true;

	// if (memberId == "a10dccea52" && networkId == "8056c2e21c24673d") {
	//	fprintf(stderr, "invalid authinfo for grant's machine\n");
	//	info.version=1;
	//	return info;
	// }
	//  fprintf(stderr, "PostgreSQL::updateMemberOnLoad: %s-%s\n", networkId.c_str(), memberId.c_str());
	std::shared_ptr<PostgresConnection> c;
	try {
		// 		c = _pool->borrow();
		// 		pqxx::work w(*c->c);

		// 		char nonceBytes[16] = {0};
		// 		std::string nonce = "";

		// 		// check if the member exists first.
		// 		pqxx::row count = w.exec_params1("SELECT count(id) FROM ztc_member WHERE id = $1 AND network_id = $2 AND deleted = false", memberId, networkId);
		// 		if (count[0].as<int>() == 1) {
		// 			// get active nonce, if exists.
		// 			pqxx::result r = w.exec_params("SELECT nonce FROM ztc_sso_expiry "
		// 				"WHERE network_id = $1 AND member_id = $2 "
		// 				"AND ((NOW() AT TIME ZONE 'UTC') <= authentication_expiry_time) AND ((NOW() AT TIME ZONE 'UTC') <= nonce_expiration)",
		// 				networkId, memberId);

		// 			if (r.size() == 0) {
		// 				// no active nonce.
		// 				// find an unused nonce, if one exists.
		// 				pqxx::result r = w.exec_params("SELECT nonce FROM ztc_sso_expiry "
		// 					"WHERE network_id = $1 AND member_id = $2 "
		// 					"AND authentication_expiry_time IS NULL AND ((NOW() AT TIME ZONE 'UTC') <= nonce_expiration)",
		// 					networkId, memberId);

		// 				if (r.size() == 1) {
		// 					// we have an existing nonce.  Use it
		// 					nonce = r.at(0)[0].as<std::string>();
		// 					Utils::unhex(nonce.c_str(), nonceBytes, sizeof(nonceBytes));
		// 				} else if (r.empty()) {
		// 					// create a nonce
		// 					Utils::getSecureRandom(nonceBytes, 16);
		// 					char nonceBuf[64] = {0};
		// 					Utils::hex(nonceBytes, sizeof(nonceBytes), nonceBuf);
		// 					nonce = std::string(nonceBuf);

		// 					pqxx::result ir = w.exec_params0("INSERT INTO ztc_sso_expiry "
		// 						"(nonce, nonce_expiration, network_id, member_id) VALUES "
		// 						"($1, TO_TIMESTAMP($2::double precision/1000), $3, $4)",
		// 						nonce, OSUtils::now() + 300000, networkId, memberId);

		// 					w.commit();
		// 				}  else {
		// 					// > 1 ?!?  Thats an error!
		// 					fprintf(stderr, "> 1 unused nonce!\n");
		// 					exit(6);
		// 				}
		// 			} else if (r.size() == 1) {
		// 				nonce = r.at(0)[0].as<std::string>();
		// 				Utils::unhex(nonce.c_str(), nonceBytes, sizeof(nonceBytes));
		// 			} else {
		// 				// more than 1 nonce in use?  Uhhh...
		// 				fprintf(stderr, "> 1 nonce in use for network member?!?\n");
		// 				exit(7);
		// 			}

		// 			r = w.exec_params(
		// 				"SELECT oc.client_id, oc.authorization_endpoint, oc.issuer, oc.provider, oc.sso_impl_version "
		// 				"FROM ztc_network AS n "
		// 				"INNER JOIN ztc_org o "
		// 				"  ON o.owner_id = n.owner_id "
		// 			    "LEFT OUTER JOIN ztc_network_oidc_config noc "
		// 				"  ON noc.network_id = n.id "
		// 				"LEFT OUTER JOIN ztc_oidc_config oc "
		// 				"  ON noc.client_id = oc.client_id AND oc.org_id = o.org_id "
		// 				"WHERE n.id = $1 AND n.sso_enabled = true", networkId);

		// 			std::string client_id = "";
		// 			std::string authorization_endpoint = "";
		// 			std::string issuer = "";
		// 			std::string provider = "";
		// 			uint64_t sso_version = 0;

		// 			if (r.size() == 1) {
		// 				client_id = r.at(0)[0].as<std::optional<std::string>>().value_or("");
		// 				authorization_endpoint = r.at(0)[1].as<std::optional<std::string>>().value_or("");
		// 				issuer = r.at(0)[2].as<std::optional<std::string>>().value_or("");
		// 				provider = r.at(0)[3].as<std::optional<std::string>>().value_or("");
		// 				sso_version = r.at(0)[4].as<std::optional<uint64_t>>().value_or(1);
		// 			} else if (r.size() > 1) {
		// 				fprintf(stderr, "ERROR: More than one auth endpoint for an organization?!?!? NetworkID: %s\n", networkId.c_str());
		// 			} else {
		// 				fprintf(stderr, "No client or auth endpoint?!?\n");
		// 			}

		// 			info.version = sso_version;

		// 			// no catch all else because we don't actually care if no records exist here. just continue as normal.
		// 			if ((!client_id.empty())&&(!authorization_endpoint.empty())) {

		// 				uint8_t state[48];
		// 				HMACSHA384(_ssoPsk, nonceBytes, sizeof(nonceBytes), state);
		// 				char state_hex[256];
		// 				Utils::hex(state, 48, state_hex);

		// 				if (info.version == 0) {
		// 					char url[2048] = {0};
		// 					OSUtils::ztsnprintf(url, sizeof(authenticationURL),
		// 						"%s?response_type=id_token&response_mode=form_post&scope=openid+email+profile&redirect_uri=%s&nonce=%s&state=%s&client_id=%s",
		// 						authorization_endpoint.c_str(),
		// 						url_encode(redirectURL).c_str(),
		// 						nonce.c_str(),
		// 						state_hex,
		// 						client_id.c_str());
		// 					info.authenticationURL = std::string(url);
		// 				} else if (info.version == 1) {
		// 					info.ssoClientID = client_id;
		// 					info.issuerURL = issuer;
		// 					info.ssoProvider = provider;
		// 					info.ssoNonce = nonce;
		// 					info.ssoState = std::string(state_hex) + "_" +networkId;
		// 					info.centralAuthURL = redirectURL;
		// #ifdef ZT_DEBUG
		// 					fprintf(
		// 						stderr,
		// 						"ssoClientID: %s\nissuerURL: %s\nssoNonce: %s\nssoState: %s\ncentralAuthURL: %s\nprovider: %s\n",
		// 						info.ssoClientID.c_str(),
		// 						info.issuerURL.c_str(),
		// 						info.ssoNonce.c_str(),
		// 						info.ssoState.c_str(),
		// 						info.centralAuthURL.c_str(),
		// 						provider.c_str());
		// #endif
		// 				}
		// 			}  else {
		// 				fprintf(stderr, "client_id: %s\nauthorization_endpoint: %s\n", client_id.c_str(), authorization_endpoint.c_str());
		// 			}
		// 		}

		// 		_pool->unborrow(c);
	}
	catch (std::exception& e) {
		fprintf(stderr, "ERROR: Error updating member on load for network %s: %s\n", networkId.c_str(), e.what());
		span->SetStatus(opentelemetry::trace::StatusCode::kError, e.what());
	}

	return info;   // std::string(authenticationURL);
}

void CV2::initializeNetworks()
{
	auto provider = opentelemetry::trace::Provider::GetTracerProvider();
	auto tracer = provider->GetTracer("cv2");
	auto span = tracer->StartSpan("cv2::initializeNetworks");
	auto scope = tracer->WithActiveSpan(span);

	fprintf(stderr, "Initializing networks...\n");

	try {
		char qbuf[2048];
		sprintf(
			qbuf,
			"SELECT id, name, configuration , (EXTRACT(EPOCH FROM creation_time AT TIME ZONE 'UTC')*1000)::bigint, "
			"(EXTRACT(EPOCH FROM last_modified AT TIME ZONE 'UTC')*1000)::bigint, revision "
			"FROM networks_ctl WHERE controller_id = '%s'",
			_myAddressStr.c_str());

		auto c = _pool->borrow();
		pqxx::work w(*c->c);

		fprintf(stderr, "Load networks from psql...\n");
		auto stream = pqxx::stream_from::query(w, qbuf);
		std::tuple<
			std::string	  // network ID
			,
			std::optional<std::string>	 // name
			,
			std::string	  // configuration
			,
			std::optional<uint64_t>	  // creation_time
			,
			std::optional<uint64_t>	  // last_modified
			,
			std::optional<uint64_t>	  // revision
			>
			row;
		uint64_t count = 0;
		uint64_t total = 0;
		while (stream >> row) {
			auto start = std::chrono::high_resolution_clock::now();

			json empty;
			json config;

			initNetwork(config);

			std::string nwid = std::get<0>(row);
			std::string name = std::get<1>(row).value_or("");
			json cfgtmp = json::parse(std::get<2>(row));
			std::optional<uint64_t> created_at = std::get<3>(row);
			std::optional<uint64_t> last_modified = std::get<4>(row);
			std::optional<uint64_t> revision = std::get<5>(row);

			config["id"] = nwid;
			config["name"] = name;
			config["creationTime"] = created_at.value_or(0);
			config["lastModified"] = last_modified.value_or(0);
			config["revision"] = revision.value_or(0);
			config["capabilities"] = cfgtmp["capabilities"].is_array() ? cfgtmp["capabilities"] : json::array();
			config["enableBroadcast"] = cfgtmp["enableBroadcast"].is_boolean() ? cfgtmp["enableBroadcast"].get<bool>() : false;
			config["mtu"] = cfgtmp["mtu"].is_number() ? cfgtmp["mtu"].get<int32_t>() : 2800;
			config["multicastLimit"] = cfgtmp["multicastLimit"].is_number() ? cfgtmp["multicastLimit"].get<int32_t>() : 64;
			config["private"] = cfgtmp["private"].is_boolean() ? cfgtmp["private"].get<bool>() : true;
			config["remoteTraceLevel"] = cfgtmp["remoteTraceLevel"].is_number() ? cfgtmp["remoteTraceLevel"].get<int32_t>() : 0;
			config["remoteTraceTarget"] = cfgtmp["remoteTraceTarget"].is_string() ? cfgtmp["remoteTraceTarget"].get<std::string>() : "";
			config["revision"] = revision.value_or(0);
			config["rules"] = cfgtmp["rules"].is_array() ? cfgtmp["rules"] : json::array();
			config["tags"] = cfgtmp["tags"].is_array() ? cfgtmp["tags"] : json::array();
			if (cfgtmp["v4AssignMode"].is_object()) {
				config["v4AssignMode"] = cfgtmp["v4AssignMode"];
			}
			else {
				config["v4AssignMode"] = json::object();
				config["v4AssignMode"]["zt"] = true;
			}
			if (cfgtmp["v6AssignMode"].is_object()) {
				config["v6AssignMode"] = cfgtmp["v6AssignMode"];
			}
			else {
				config["v6AssignMode"] = json::object();
				config["v6AssignMode"]["zt"] = true;
				config["v6AssignMode"]["6plane"] = true;
				config["v6AssignMode"]["rfc4193"] = false;
			}
			config["ssoEnabled"] = cfgtmp["ssoEnabled"].is_boolean() ? cfgtmp["ssoEnabled"].get<bool>() : false;
			config["objtype"] = "network";
			config["routes"] = cfgtmp["routes"].is_array() ? cfgtmp["routes"] : json::array();
			config["clientId"] = cfgtmp["clientId"].is_string() ? cfgtmp["clientId"].get<std::string>() : "";
			config["authorizationEndpoint"] = cfgtmp["authorizationEndpoint"].is_string() ? cfgtmp["authorizationEndpoint"].get<std::string>() : nullptr;
			config["provider"] = cfgtmp["ssoProvider"].is_string() ? cfgtmp["ssoProvider"].get<std::string>() : "";
			if (! cfgtmp["dns"].is_object()) {
				cfgtmp["dns"] = json::object();
				cfgtmp["dns"]["domain"] = "";
				cfgtmp["dns"]["servers"] = json::array();
			}
			else {
				config["dns"] = cfgtmp["dns"];
			}
			config["ipAssignmentPools"] = cfgtmp["ipAssignmentPools"].is_array() ? cfgtmp["ipAssignmentPools"] : json::array();

			Metrics::network_count++;

			_networkChanged(empty, config, false);

			auto end = std::chrono::high_resolution_clock::now();
			auto dur = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
			;
			total += dur.count();
			++count;
			if (count > 0 && count % 10000 == 0) {
				fprintf(stderr, "Averaging %lu us per network\n", (total / count));
			}
		}

		w.commit();
		_pool->unborrow(c);
		fprintf(stderr, "done.\n");

		if (++this->_ready == 2) {
			if (_waitNoticePrinted) {
				fprintf(stderr, "[%s] NOTICE: %.10llx controller PostgreSQL data download complete." ZT_EOL_S, _timestr(), (unsigned long long)_myAddress.toInt());
			}
			_readyLock.unlock();
		}
		fprintf(stderr, "network init done\n");
	}
	catch (std::exception& e) {
		fprintf(stderr, "ERROR: Error initializing networks: %s\n", e.what());
		span->SetStatus(opentelemetry::trace::StatusCode::kError, e.what());
		std::this_thread::sleep_for(std::chrono::milliseconds(5000));
		exit(-1);
	}
}

void CV2::initializeMembers()
{
	auto provider = opentelemetry::trace::Provider::GetTracerProvider();
	auto tracer = provider->GetTracer("cv2");
	auto span = tracer->StartSpan("cv2::initializeMembers");
	auto scope = tracer->WithActiveSpan(span);

	std::string memberId;
	std::string networkId;
	try {
		char qbuf[2048];
		sprintf(
			qbuf,
			"SELECT nm.device_id, nm.network_id, nm.authorized, nm.active_bridge, nm.ip_assignments, nm.no_auto_assign_ips, "
			"nm.sso_exempt, (EXTRACT(EPOCH FROM nm.authentication_expiry_time AT TIME ZONE 'UTC')*1000)::bigint, "
			"(EXTRACT(EPOCH FROM nm.creation_time AT TIME ZONE 'UTC')*1000)::bigint, nm.identity, "
			"(EXTRACT(EPOCH FROM nm.last_authorized_time AT TIME ZONE 'UTC')*1000)::bigint, "
			"(EXTRACT(EPOCH FROM nm.last_deauthorized_time AT TIME ZONE 'UTC')*1000)::bigint, "
			"nm.remote_trace_level, nm.remote_trace_target, nm.revision, nm.capabilities, nm.tags "
			"FROM network_memberships_ctl nm "
			"INNER JOIN networks_ctl n "
			"  ON nm.network_id = n.id "
			"WHERE n.controller_id = '%s'",
			_myAddressStr.c_str());

		auto c = _pool->borrow();
		pqxx::work w(*c->c);
		fprintf(stderr, "Load members from psql...\n");
		auto stream = pqxx::stream_from::query(w, qbuf);
		std::tuple<
			std::string	  // device ID
			,
			std::string	  // network ID
			,
			bool   // authorized
			,
			std::optional<bool>	  // active_bridge
			,
			std::optional<std::string>	 // ip_assignments
			,
			std::optional<bool>	  // no_auto_assign_ips
			,
			std::optional<bool>	  // sso_exempt
			,
			std::optional<uint64_t>	  // authentication_expiry_time
			,
			std::optional<uint64_t>	  // creation_time
			,
			std::optional<std::string>	 // identity
			,
			std::optional<uint64_t>	  // last_authorized_time
			,
			std::optional<uint64_t>	  // last_deauthorized_time
			,
			std::optional<int32_t>	 // remote_trace_level
			,
			std::optional<std::string>	 // remote_trace_target
			,
			std::optional<uint64_t>	  // revision
			,
			std::optional<std::string>	 // capabilities
			,
			std::optional<std::string>	 // tags
			>
			row;

		uint64_t count = 0;
		uint64_t total = 0;
		while (stream >> row) {
			auto start = std::chrono::high_resolution_clock::now();
			json empty;
			json config;

			initMember(config);

			memberId = std::get<0>(row);
			networkId = std::get<1>(row);
			bool authorized = std::get<2>(row);
			std::optional<bool> active_bridge = std::get<3>(row);
			std::string ip_assignments = std::get<4>(row).value_or("");
			std::optional<bool> no_auto_assign_ips = std::get<5>(row);
			std::optional<bool> sso_exempt = std::get<6>(row);
			std::optional<uint64_t> authentication_expiry_time = std::get<7>(row);
			std::optional<uint64_t> creation_time = std::get<8>(row);
			std::optional<std::string> identity = std::get<9>(row);
			std::optional<uint64_t> last_authorized_time = std::get<10>(row);
			std::optional<uint64_t> last_deauthorized_time = std::get<11>(row);
			std::optional<int32_t> remote_trace_level = std::get<12>(row);
			std::optional<std::string> remote_trace_target = std::get<13>(row);
			std::optional<uint64_t> revision = std::get<14>(row);
			std::optional<std::string> capabilities = std::get<15>(row);
			std::optional<std::string> tags = std::get<16>(row);

			config["objtype"] = "member";
			config["id"] = memberId;
			config["address"] = identity.value_or("");
			config["nwid"] = networkId;
			config["authorized"] = authorized;
			config["activeBridge"] = active_bridge.value_or(false);
			config["ipAssignments"] = json::array();
			if (ip_assignments != "{}") {
				std::string tmp = ip_assignments.substr(1, ip_assignments.length() - 2);
				std::vector<std::string> addrs = split(tmp, ',');
				for (auto it = addrs.begin(); it != addrs.end(); ++it) {
					config["ipAssignments"].push_back(*it);
				}
			}
			config["capabilities"] = json::parse(capabilities.value_or("[]"));
			config["creationTime"] = creation_time.value_or(0);
			config["lastAuthorizedTime"] = last_authorized_time.value_or(0);
			config["lastDeauthorizedTime"] = last_deauthorized_time.value_or(0);
			config["noAutoAssignIPs"] = no_auto_assign_ips.value_or(false);
			config["remoteTraceLevel"] = remote_trace_level.value_or(0);
			config["remoteTraceTarget"] = remote_trace_target.value_or(nullptr);
			config["revision"] = revision.value_or(0);
			config["ssoExempt"] = sso_exempt.value_or(false);
			config["authenticationExpiryTime"] = authentication_expiry_time.value_or(0);
			config["tags"] = json::parse(tags.value_or("[]"));

			Metrics::member_count++;

			_memberChanged(empty, config, false);

			memberId = "";
			networkId = "";

			auto end = std::chrono::high_resolution_clock::now();
			auto dur = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
			total += dur.count();
			++count;
			if (count > 0 && count % 10000 == 0) {
				fprintf(stderr, "Averaging %lu us per member\n", (total / count));
			}
		}
		if (count > 0) {
			fprintf(stderr, "Took %lu us per member to load\n", (total / count));
		}

		stream.complete();
		w.commit();
		_pool->unborrow(c);
		fprintf(stderr, "done.\n");

		if (++this->_ready == 2) {
			if (_waitNoticePrinted) {
				fprintf(stderr, "[%s] NOTICE: %.10llx controller PostgreSQL data download complete." ZT_EOL_S, _timestr(), (unsigned long long)_myAddress.toInt());
			}
			_readyLock.unlock();
		}
		fprintf(stderr, "member init done\n");
	}
	catch (std::exception& e) {
		fprintf(stderr, "ERROR: Error initializing member: %s-%s %s\n", networkId.c_str(), memberId.c_str(), e.what());
		span->SetStatus(opentelemetry::trace::StatusCode::kError, e.what());
		exit(-1);
	}
}

void CV2::heartbeat()
{
	char publicId[1024];
	char hostnameTmp[1024];
	_myId.toString(false, publicId);
	if (gethostname(hostnameTmp, sizeof(hostnameTmp)) != 0) {
		hostnameTmp[0] = (char)0;
	}
	else {
		for (int i = 0; i < (int)sizeof(hostnameTmp); ++i) {
			if ((hostnameTmp[i] == '.') || (hostnameTmp[i] == 0)) {
				hostnameTmp[i] = (char)0;
				break;
			}
		}
	}
	const char* controllerId = _myAddressStr.c_str();
	const char* publicIdentity = publicId;
	const char* hostname = hostnameTmp;

	while (_run == 1) {
		auto provider = opentelemetry::trace::Provider::GetTracerProvider();
		auto tracer = provider->GetTracer("cv2");
		auto span = tracer->StartSpan("cv2::heartbeat");
		auto scope = tracer->WithActiveSpan(span);

		auto c = _pool->borrow();
		int64_t ts = OSUtils::now();

		if (c->c) {
			std::string major = std::to_string(ZEROTIER_ONE_VERSION_MAJOR);
			std::string minor = std::to_string(ZEROTIER_ONE_VERSION_MINOR);
			std::string rev = std::to_string(ZEROTIER_ONE_VERSION_REVISION);
			std::string version = major + "." + minor + "." + rev;
			std::string versionStr = "v" + version;

			try {
				pqxx::work w { *c->c };
				w.exec_params0(
					"INSERT INTO controllers_ctl (id, hostname, last_heartbeat, public_identity, version) VALUES "
					"($1, $2, TO_TIMESTAMP($3::double precision/1000), $4, $5) "
					"ON CONFLICT (id) DO UPDATE SET hostname = EXCLUDED.hostname, last_heartbeat = EXCLUDED.last_heartbeat, "
					"public_identity = EXCLUDED.public_identity, version = EXCLUDED.version",
					controllerId,
					hostname,
					ts,
					publicIdentity,
					versionStr);
				w.commit();
			}
			catch (std::exception& e) {
				fprintf(stderr, "ERROR: Error in heartbeat: %s\n", e.what());
				span->SetStatus(opentelemetry::trace::StatusCode::kError, e.what());
				continue;
			}
			catch (...) {
				fprintf(stderr, "ERROR: Unknown error in heartbeat\n");
				span->SetStatus(opentelemetry::trace::StatusCode::kError, "Unknown error in heartbeat");
				continue;
			}
		}

		_pool->unborrow(c);
		span->End();

		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
	fprintf(stderr, "Exited heartbeat thread\n");
}

void CV2::membersDbWatcher()
{
	auto c = _pool->borrow();

	std::string stream = "member_" + _myAddressStr;

	fprintf(stderr, "Listening to member stream: %s\n", stream.c_str());
	MemberNotificationReceiver<CV2> m(this, *c->c, stream);

	while (_run == 1) {
		c->c->await_notification(5, 0);
	}

	_pool->unborrow(c);

	fprintf(stderr, "Exited membersDbWatcher\n");
}

void CV2::networksDbWatcher()
{
	std::string stream = "network_" + _myAddressStr;

	fprintf(stderr, "Listening to member stream: %s\n", stream.c_str());

	auto c = _pool->borrow();

	NetworkNotificationReceiver<CV2> n(this, *c->c, stream);

	while (_run == 1) {
		c->c->await_notification(5, 0);
	}

	_pool->unborrow(c);
	fprintf(stderr, "Exited networksDbWatcher\n");
}

void CV2::commitThread()
{
	fprintf(stderr, "%s: commitThread start\n", _myAddressStr.c_str());
	std::pair<nlohmann::json, bool> qitem;
	while (_commitQueue.get(qitem) && (_run == 1)) {
		auto provider = opentelemetry::trace::Provider::GetTracerProvider();
		auto tracer = provider->GetTracer("cv2");
		auto span = tracer->StartSpan("cv2::commitThread");
		auto scope = tracer->WithActiveSpan(span);

		// fprintf(stderr, "commitThread tick\n");
		if (! qitem.first.is_object()) {
			fprintf(stderr, "not an object\n");
			continue;
		}

		std::shared_ptr<PostgresConnection> c;
		try {
			c = _pool->borrow();
		}
		catch (std::exception& e) {
			fprintf(stderr, "ERROR: %s\n", e.what());
			continue;
		}

		if (! c) {
			fprintf(stderr, "Error getting database connection\n");
			continue;
		}

		Metrics::pgsql_commit_ticks++;
		try {
			nlohmann::json& config = (qitem.first);
			const std::string objtype = config["objtype"];
			if (objtype == "member") {
				auto mspan = tracer->StartSpan("cv2::commitThread::member");
				auto mscope = tracer->WithActiveSpan(span);

				// fprintf(stderr, "%s: commitThread: member\n", _myAddressStr.c_str());
				std::string memberId;
				std::string networkId;
				try {
					pqxx::work w(*c->c);

					memberId = config["id"];
					networkId = config["nwid"];

					std::string target = "NULL";
					if (! config["remoteTraceTarget"].is_null()) {
						target = config["remoteTraceTarget"];
					}

					pqxx::row nwrow = w.exec_params1("SELECT COUNT(id) FROM networks WHERE id = $1", networkId);
					int nwcount = nwrow[0].as<int>();

					if (nwcount != 1) {
						fprintf(stderr, "network %s does not exist.  skipping member upsert\n", networkId.c_str());
						w.abort();
						_pool->unborrow(c);
						continue;
					}

					// only needed for hooks, and no hooks for now
					// pqxx::row mrow = w.exec_params1("SELECT COUNT(id) FROM device_networks WHERE device_id = $1 AND network_id = $2", memberId, networkId);
					// int membercount = mrow[0].as<int>();
					// bool isNewMember = (membercount == 0);

					pqxx::result res = w.exec_params0(
						"INSERT INTO network_memberships_ctl (device_id, network_id, authorized, active_bridge, ip_assignments, "
						"no_auto_assign_ips, sso_exempt, authentication_expiry_time, capabilities, creation_time, "
						"identity, last_authorized_time, last_deauthorized_time, "
						"remote_trace_level, remote_trace_target, revision, tags, version_major, version_minor, "
						"version_revision, version_protocol) "
						"VALUES ($1, $2, $3, $4, $5, $6, $7, TO_TIMESTAMP($8::double precision/1000), $9, "
						"TO_TIMESTAMP($10::double precision/1000), $11, TO_TIMESTAMP($12::double precision/1000), "
						"TO_TIMESTAMP($13::double precision/1000), $14, $15, $16, $17, $18, $19, $20, $21) "
						"ON CONFLICT (device_id, network_id) DO UPDATE SET "
						"authorized = EXCLUDED.authorized, active_bridge = EXCLUDED.active_bridge, "
						"ip_assignments = EXCLUDED.ip_assignments, no_auto_assign_ips = EXCLUDED.no_auto_assign_ips, "
						"sso_exempt = EXCLUDED.sso_exempt, authentication_expiry_time = EXCLUDED.authentication_expiry_time, "
						"capabilities = EXCLUDED.capabilities, creation_time = EXCLUDED.creation_time, "
						"identity = EXCLUDED.identity, last_authorized_time = EXCLUDED.last_authorized_time, "
						"last_deauthorized_time = EXCLUDED.last_deauthorized_time, "
						"remote_trace_level = EXCLUDED.remote_trace_level, remote_trace_target = EXCLUDED.remote_trace_target, "
						"revision = EXCLUDED.revision, tags = EXCLUDED.tags, version_major = EXCLUDED.version_major, "
						"version_minor = EXCLUDED.version_minor, version_revision = EXCLUDED.version_revision, "
						"version_protocol = EXCLUDED.version_protocol",
						memberId,
						networkId,
						(bool)config["authorized"],
						(bool)config["activeBridge"],
						config["ipAssignments"].get<std::vector<std::string> >(),
						(bool)config["noAutoAssignIps"],
						(bool)config["ssoExempt"],
						(uint64_t)config["authenticationExpiryTime"],
						OSUtils::jsonDump(config["capabilities"], -1),
						(uint64_t)config["creationTime"],
						OSUtils::jsonString(config["identity"], ""),
						(uint64_t)config["lastAuthorizedTime"],
						(uint64_t)config["lastDeauthorizedTime"],
						(int)config["remoteTraceLevel"],
						target,
						(uint64_t)config["revision"],
						OSUtils::jsonDump(config["tags"], -1),
						(int)config["vMajor"],
						(int)config["vMinor"],
						(int)config["vRev"],
						(int)config["vProto"]);

					w.commit();

					// No hooks for now
					// if (_smee != NULL && isNewMember) {
					// 	pqxx::row row = w.exec_params1(
					// 		"SELECT "
					// 		"	count(h.hook_id) "
					// 		"FROM "
					// 		"	ztc_hook h "
					// 		"	INNER JOIN ztc_org o ON o.org_id = h.org_id "
					// 		"   INNER JOIN ztc_network n ON n.owner_id = o.owner_id "
					// 		" WHERE "
					// 		"n.id = $1 ",
					// 		networkId
					// 	);
					// 	int64_t hookCount = row[0].as<int64_t>();
					// 	if (hookCount > 0) {
					// 		notifyNewMember(networkId, memberId);
					// 	}
					// }

					const uint64_t nwidInt = OSUtils::jsonIntHex(config["nwid"], 0ULL);
					const uint64_t memberidInt = OSUtils::jsonIntHex(config["id"], 0ULL);
					if (nwidInt && memberidInt) {
						nlohmann::json nwOrig;
						nlohmann::json memOrig;

						nlohmann::json memNew(config);

						get(nwidInt, nwOrig, memberidInt, memOrig);

						_memberChanged(memOrig, memNew, qitem.second);
					}
					else {
						fprintf(stderr, "%s: Can't notify of change.  Error parsing nwid or memberid: %llu-%llu\n", _myAddressStr.c_str(), (unsigned long long)nwidInt, (unsigned long long)memberidInt);
					}
				}
				catch (pqxx::data_exception& e) {
					std::string cfgDump = OSUtils::jsonDump(config, 2);
					fprintf(stderr, "Member save %s-%s: %s\n", networkId.c_str(), memberId.c_str(), cfgDump.c_str());

					const pqxx::sql_error* s = dynamic_cast<const pqxx::sql_error*>(&e);
					fprintf(stderr, "%s ERROR: Error updating member: %s\n", _myAddressStr.c_str(), e.what());
					if (s) {
						fprintf(stderr, "%s ERROR: SQL error: %s\n", _myAddressStr.c_str(), s->query().c_str());
					}
					mspan->SetStatus(opentelemetry::trace::StatusCode::kError, "pqxx::data_exception");
					mspan->SetAttribute("error", e.what());
					mspan->SetAttribute("config", cfgDump);
				}
				catch (std::exception& e) {
					std::string cfgDump = OSUtils::jsonDump(config, 2);
					fprintf(stderr, "%s ERROR: Error updating member %s-%s: %s\njsonDump: %s\n", _myAddressStr.c_str(), networkId.c_str(), memberId.c_str(), e.what(), cfgDump.c_str());
					mspan->SetStatus(opentelemetry::trace::StatusCode::kError, "std::exception");
					mspan->SetAttribute("error", e.what());
					mspan->SetAttribute("config", cfgDump);
				}
			}
			else if (objtype == "network") {
				auto nspan = tracer->StartSpan("cv2::commitThread::network");
				auto nscope = tracer->WithActiveSpan(span);

				try {
					// fprintf(stderr, "%s: commitThread: network\n", _myAddressStr.c_str());
					pqxx::work w(*c->c);

					std::string id = config["id"];

					// network must already exist
					pqxx::result res = w.exec_params0(
						"INSERT INTO networks_ctl (id, name, configuration, controller_id, revision) "
						"VALUES ($1, $2, $3, $4, $5) "
						"ON CONFLICT (id) DO UPDATE SET "
						"name = EXCLUDED.name, configuration = EXCLUDED.configuration, revision = EXCLUDED.revision+1",
						id,
						OSUtils::jsonString(config["name"], ""),
						OSUtils::jsonDump(config, -1),
						_myAddressStr,
						((uint64_t)config["revision"]));

					w.commit();

					const uint64_t nwidInt = OSUtils::jsonIntHex(config["nwid"], 0ULL);
					if (nwidInt) {
						nlohmann::json nwOrig;
						nlohmann::json nwNew(config);

						get(nwidInt, nwOrig);

						_networkChanged(nwOrig, nwNew, qitem.second);
					}
					else {
						fprintf(stderr, "%s: Can't notify network changed: %llu\n", _myAddressStr.c_str(), (unsigned long long)nwidInt);
					}
				}
				catch (pqxx::data_exception& e) {
					const pqxx::sql_error* s = dynamic_cast<const pqxx::sql_error*>(&e);
					fprintf(stderr, "%s ERROR: Error updating network: %s\n", _myAddressStr.c_str(), e.what());
					if (s) {
						fprintf(stderr, "%s ERROR: SQL error: %s\n", _myAddressStr.c_str(), s->query().c_str());
					}
					nspan->SetStatus(opentelemetry::trace::StatusCode::kError, "pqxx::data_exception");
					nspan->SetAttribute("error", e.what());
					nspan->SetAttribute("config", OSUtils::jsonDump(config, 2));
				}
				catch (std::exception& e) {
					fprintf(stderr, "%s ERROR: Error updating network: %s\n", _myAddressStr.c_str(), e.what());
					nspan->SetStatus(opentelemetry::trace::StatusCode::kError, "std::exception");
					nspan->SetAttribute("error", e.what());
					nspan->SetAttribute("config", OSUtils::jsonDump(config, 2));
				}
			}
			else if (objtype == "_delete_network") {
				auto dspan = tracer->StartSpan("cv2::commitThread::delete_network");
				auto dscope = tracer->WithActiveSpan(span);

				// fprintf(stderr, "%s: commitThread: delete network\n", _myAddressStr.c_str());
				try {
					pqxx::work w(*c->c);
					std::string networkId = config["id"];
					fprintf(stderr, "Deleting network %s\n", networkId.c_str());
					w.exec_params0("DELETE FROM network_memberships_ctl WHERE network_id = $1", networkId);
					w.exec_params0("DELETE FROM networks_ctl WHERE id = $1", networkId);

					w.commit();

					uint64_t nwidInt = OSUtils::jsonIntHex(config["nwid"], 0ULL);
					json oldConfig;
					get(nwidInt, oldConfig);
					json empty;
					_networkChanged(oldConfig, empty, qitem.second);
				}
				catch (std::exception& e) {
					fprintf(stderr, "%s ERROR: Error deleting network: %s\n", _myAddressStr.c_str(), e.what());
					dspan->SetStatus(opentelemetry::trace::StatusCode::kError, "std::exception");
					dspan->SetAttribute("error", e.what());
					dspan->SetAttribute("config", OSUtils::jsonDump(config, 2));
				}
			}
			else if (objtype == "_delete_member") {
				auto dspan = tracer->StartSpan("cv2::commitThread::delete_member");
				auto dscope = tracer->WithActiveSpan(span);

				// fprintf(stderr, "%s commitThread: delete member\n", _myAddressStr.c_str());
				try {
					pqxx::work w(*c->c);

					std::string memberId = config["id"];
					std::string networkId = config["nwid"];

					pqxx::result res = w.exec_params0("DELETE FROM network_memberships_ctl WHERE device_id = $1 AND network_id = $2", memberId, networkId);

					w.commit();

					uint64_t nwidInt = OSUtils::jsonIntHex(config["nwid"], 0ULL);
					uint64_t memberidInt = OSUtils::jsonIntHex(config["id"], 0ULL);

					nlohmann::json networkConfig;
					nlohmann::json oldConfig;

					get(nwidInt, networkConfig, memberidInt, oldConfig);
					json empty;
					_memberChanged(oldConfig, empty, qitem.second);
				}
				catch (std::exception& e) {
					fprintf(stderr, "%s ERROR: Error deleting member: %s\n", _myAddressStr.c_str(), e.what());
					dspan->SetStatus(opentelemetry::trace::StatusCode::kError, "std::exception");
					dspan->SetAttribute("error", e.what());
					dspan->SetAttribute("config", OSUtils::jsonDump(config, 2));
				}
			}
			else {
				fprintf(stderr, "%s ERROR: unknown objtype\n", _myAddressStr.c_str());
			}
		}
		catch (std::exception& e) {
			fprintf(stderr, "%s ERROR: Error getting objtype: %s\n", _myAddressStr.c_str(), e.what());
			span->SetStatus(opentelemetry::trace::StatusCode::kError, "std::exception");
			span->SetAttribute("error", e.what());
		}
		_pool->unborrow(c);
		c.reset();
	}

	fprintf(stderr, "%s commitThread finished\n", _myAddressStr.c_str());
}

void CV2::onlineNotificationThread()
{
	waitForReady();

	_connected = 1;

	nlohmann::json jtmp1, jtmp2;
	while (_run == 1) {
		auto provider = opentelemetry::trace::Provider::GetTracerProvider();
		auto tracer = provider->GetTracer("cv2");
		auto span = tracer->StartSpan("cv2::onlineNotificationThread");
		auto scope = tracer->WithActiveSpan(span);

		auto c = _pool->borrow();
		auto c2 = _pool->borrow();

		try {
			fprintf(stderr, "%s onlineNotificationThread\n", _myAddressStr.c_str());

			std::unordered_map<std::pair<uint64_t, uint64_t>, NodeOnlineRecord, _PairHasher> lastOnline;
			{
				std::lock_guard<std::mutex> l(_lastOnline_l);
				lastOnline.swap(_lastOnline);
			}

			pqxx::work w(*c->c);
			pqxx::work w2(*c2->c);

			bool firstRun = true;
			bool memberAdded = false;
			uint64_t updateCount = 0;

			pqxx::pipeline pipe(w);

			for (auto i = lastOnline.begin(); i != lastOnline.end(); ++i) {
				updateCount++;

				uint64_t nwid_i = i->first.first;
				char nwidTmp[64];
				char memTmp[64];
				char ipTmp[64];

				OSUtils::ztsnprintf(nwidTmp, sizeof(nwidTmp), "%.16llx", nwid_i);
				OSUtils::ztsnprintf(memTmp, sizeof(memTmp), "%.10llx", i->first.second);

				if (! get(nwid_i, jtmp1, i->first.second, jtmp2)) {
					continue;	// skip non existent networks/members
				}

				std::string networkId(nwidTmp);
				std::string memberId(memTmp);

				try {
					pqxx::row r = w2.exec_params1("SELECT device_id, network_id FROM network_memberships_ctl WHERE network_id = $1 AND device_id = $2", networkId, memberId);
				}
				catch (pqxx::unexpected_rows& e) {
					continue;
				}

				int64_t ts = i->second.lastSeen;
				std::string ipAddr = i->second.physicalAddress.toIpString(ipTmp);
				std::string timestamp = std::to_string(ts);
				std::string osArch = i->second.osArch;
				std::vector<std::string> osArchSplit = split(osArch, '/');
				std::string os = osArchSplit[0];
				std::string arch = osArchSplit[1];

				if (ipAddr.empty()) {
					ipAddr = "relayed";
				}

				json record = {
					{ ipAddr, ts },
				};

				std::string device_network_insert = "INSERT INTO network_memberships_ctl (device_id, network_id, last_seen, os, arch) "
													"VALUES ('"
													+ w2.esc(memberId) + "', '" + w2.esc(networkId) + "', '" + w2.esc(record.dump())
													+ "'::JSONB, "
													  "'"
													+ w2.esc(os) + "', '" + w2.esc(arch)
													+ "') "
													  "ON CONFLICT (device_id, network_id) DO UPDATE SET os = EXCLUDED.os, arch = EXCLUDED.arch, "
													  "last_seen = network_memberships_ctl.last_seen || EXCLUDED.last_seen";
				pipe.insert(device_network_insert);

				Metrics::pgsql_node_checkin++;
			}

			pipe.complete();
			;
			w2.commit();
			w.commit();
			fprintf(stderr, "%s: Updated online status of %lu members\n", _myAddressStr.c_str(), updateCount);
		}
		catch (std::exception& e) {
			fprintf(stderr, "%s ERROR: Error in onlineNotificationThread: %s\n", _myAddressStr.c_str(), e.what());
			span->SetStatus(opentelemetry::trace::StatusCode::kError, "std::exception");
			span->SetAttribute("error", e.what());
		}
		catch (...) {
			fprintf(stderr, "%s ERROR: Unknown error in onlineNotificationThread\n", _myAddressStr.c_str());
			span->SetStatus(opentelemetry::trace::StatusCode::kError, "unknown");
		}
		_pool->unborrow(c2);
		_pool->unborrow(c);
		span->End();

		std::this_thread::sleep_for(std::chrono::seconds(10));
	}

	fprintf(stderr, "%s: Fell out of run loop in onlineNotificationThread\n", _myAddressStr.c_str());
	if (_run == 1) {
		fprintf(stderr, "ERROR: %s onlineNotificationThread should still be running! Exiting Controller.\n", _myAddressStr.c_str());
		exit(6);
	}
}
#endif	 // ZT_CONTROLLER_USE_LIBPQ