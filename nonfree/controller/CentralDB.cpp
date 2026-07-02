/* (c) ZeroTier, Inc.
 * See LICENSE.txt in nonfree/
 */

#include "CentralDB.hpp"

#ifdef ZT_CONTROLLER_USE_LIBPQ

#include "../../node/Constants.hpp"
#include "../../node/SHA512.hpp"
#include "../../version.h"
#include "BigTableStatusWriter.hpp"
#include "ControllerChangeNotifier.hpp"
#include "ControllerConfig.hpp"
#include "CtlUtil.hpp"
#include "EmbeddedNetworkController.hpp"
#include "OtelCarrier.hpp"
#include "PostgresStatusWriter.hpp"
#include "PubSubListener.hpp"
#include "PubSubWriter.hpp"
#include "Redis.hpp"
#include "RedisListener.hpp"
#include "RedisStatusWriter.hpp"
#include "opentelemetry/context/propagation/global_propagator.h"
#include "opentelemetry/trace/provider.h"

#include <chrono>
#include <climits>
#include <iomanip>
#include <libpq-fe.h>
#include <optional>
#include <pqxx/pqxx>
#include <set>
#include <sstream>

// #define REDIS_TRACE 1

using json = nlohmann::json;

using namespace ZeroTier;

using Attrs = std::vector<std::pair<std::string, std::string> >;
using Item = std::pair<std::string, Attrs>;
using ItemStream = std::vector<Item>;

CentralDB::CentralDB(const Identity& myId,
					 const char* connString,
					 int listenPort,
					 CentralDB::ListenerMode listenMode,
					 CentralDB::StatusWriterMode statusMode,
					 const ControllerConfig* cc)
	: DB()
	, _listenerMode(listenMode)
	, _statusWriterMode(statusMode)
	, _cc(cc)
	, _assignedCentralVersion(cc->assignedCentralVersion)
	, _pool()
	, _myId(myId)
	, _myAddress(myId.address())
	, _ready(0)
	, _connected(1)
	, _run(1)
	, _waitNoticePrinted(false)
	, _listenPort(listenPort)
	, _redis(NULL)
	, _cluster(NULL)
	, _redisMemberStatus(false)
{
	auto provider = opentelemetry::trace::Provider::GetTracerProvider();
	auto tracer = provider->GetTracer("CentralDB");
	auto span = tracer->StartSpan("CentralDB::CentralDB");
	auto scope = tracer->WithActiveSpan(span);

	char myAddress[64];
	_myAddressStr = myId.address().toString(myAddress);
	_connString = std::string(connString);

	auto f = std::make_shared<PostgresConnFactory>(_connString);
	_pool =
		std::make_shared<ConnectionPool<PostgresConnection> >(15, 5, std::static_pointer_cast<ConnectionFactory>(f));

	memset(_ssoPsk, 0, sizeof(_ssoPsk));
	char* const ssoPskHex = getenv("ZT_SSO_PSK");
#ifdef ZT_TRACE
	ZTC_LOG("ZT_SSO_PSK: %s\n", ssoPskHex);
#endif
	if (ssoPskHex) {
		// SECURITY: note that ssoPskHex will always be null-terminated if libc actually
		// returns something non-NULL. If the hex encodes something shorter than 48 bytes,
		// it will be padded at the end with zeroes. If longer, it'll be truncated.
		Utils::unhex(ssoPskHex, _ssoPsk, sizeof(_ssoPsk));
		ZTC_LOG("SSO PSK configured\n");
	}
	const char* redisMemberStatus = getenv("ZT_REDIS_MEMBER_STATUS");
	if (redisMemberStatus && (strcmp(redisMemberStatus, "true") == 0)) {
		_redisMemberStatus = true;
		ZTC_LOG("Using redis for member status\n");
	}

	if ((listenMode == LISTENER_MODE_REDIS || statusMode == STATUS_WRITER_MODE_REDIS) && _cc->redisConfig != NULL) {
		auto innerspan = tracer->StartSpan("CentralDB::CentralDB::configureRedis");
		auto innerscope = tracer->WithActiveSpan(innerspan);

		sw::redis::ConnectionOptions opts;
		sw::redis::ConnectionPoolOptions poolOpts;
		opts.host = _cc->redisConfig->hostname;
		opts.port = _cc->redisConfig->port;
		opts.password = _cc->redisConfig->password;
		opts.db = 0;
		opts.keep_alive = true;
		opts.connect_timeout = std::chrono::seconds(3);
		poolOpts.size = 25;
		poolOpts.wait_timeout = std::chrono::seconds(5);
		poolOpts.connection_lifetime = std::chrono::minutes(3);
		poolOpts.connection_idle_time = std::chrono::minutes(1);
		if (_cc->redisConfig->clusterMode) {
			innerspan->SetAttribute("cluster_mode", "true");
			ZTC_LOG("Using Redis in Cluster Mode\n");
			_cluster = std::make_shared<sw::redis::RedisCluster>(opts, poolOpts);
		}
		else {
			innerspan->SetAttribute("cluster_mode", "false");
			ZTC_LOG("Using Redis in Standalone Mode\n");
			_redis = std::make_shared<sw::redis::Redis>(opts, poolOpts);
		}
	}

	ZTC_LOG("NOTICE: PostgreSQL waiting for initial data download..." ZT_EOL_S);
	_waitNoticePrinted = true;

	initializeNetworks();
	initializeMembers();

	_heartbeatThread = std::thread(&CentralDB::heartbeat, this);

	switch (listenMode) {
		case LISTENER_MODE_REDIS:
			ZTC_LOG("Using Redis for change listeners\n");
			if (_cc->redisConfig != NULL) {
				std::shared_ptr<RedisMemberListener> memberListener;
				std::shared_ptr<RedisNetworkListener> networkListener;
				if (_cc->redisConfig->clusterMode) {
					memberListener = std::make_shared<RedisMemberListener>(_myAddressStr, _cluster, this);
					networkListener = std::make_shared<RedisNetworkListener>(_myAddressStr, _cluster, this);
				}
				else {
					memberListener = std::make_shared<RedisMemberListener>(_myAddressStr, _redis, this);
					networkListener = std::make_shared<RedisNetworkListener>(_myAddressStr, _redis, this);
				}
				// RedisListener spawns its listen thread only when start() is called --
				// it can't start in the base ctor because listen() is pure virtual and
				// the derived object isn't constructed yet. (Postgres/PubSub listeners
				// start in their own ctors.) Without this, Redis listener mode silently
				// never consumes change notifications.
				memberListener->start();
				networkListener->start();
				_membersDbWatcher = memberListener;
				_networksDbWatcher = networkListener;
			}
			else {
				throw std::runtime_error("CentralDB: Redis listener mode selected but no Redis configuration provided");
			}
			break;
		case LISTENER_MODE_PUBSUB:
			ZTC_LOG("Using PubSub for change listeners\n");
			if (cc->pubSubConfig != NULL) {
				_membersDbWatcher = std::make_shared<PubSubMemberListener>(
					_myAddressStr, cc->pubSubConfig->project_id, cc->pubSubConfig->member_change_recv_topic, this);
				_networksDbWatcher = std::make_shared<PubSubNetworkListener>(
					_myAddressStr, cc->pubSubConfig->project_id, cc->pubSubConfig->network_change_recv_topic, this);
				_changeNotifier = std::make_shared<PubSubChangeNotifier>(_myAddressStr, cc->pubSubConfig->project_id,
																		 cc->pubSubConfig->member_change_send_topic,
																		 cc->pubSubConfig->network_change_send_topic);
				if (! cc->pubSubConfig->sso_nonce_publish_topic.empty()) {
					_ssoNonceWriter = std::make_shared<PubSubWriter>(
						cc->pubSubConfig->project_id, cc->pubSubConfig->sso_nonce_publish_topic, _myAddressStr);
				}
				if (! cc->pubSubConfig->sso_auth_subscribe_topic.empty()) {
					_ssoAuthListener = std::make_shared<PubSubSSOListener>(
						_myAddressStr, cc->pubSubConfig->project_id, cc->pubSubConfig->sso_auth_subscribe_topic, _pool);
				}
			}
			else {
				throw std::runtime_error(
					"CentralDB: PubSub listener mode selected but no PubSub configuration provided");
			}
			break;
		case LISTENER_MODE_PGSQL:
		default:
			ZTC_LOG("Using PostgreSQL for change listeners\n");
			_membersDbWatcher = std::make_shared<PostgresMemberListener>(this, _pool, "member_" + _myAddressStr, 5);
			_networksDbWatcher = std::make_shared<PostgresNetworkListener>(this, _pool, "network_" + _myAddressStr, 5);
			break;
	}

	std::shared_ptr<PubSubWriter> pubsubWriter;
	switch (statusMode) {
		case STATUS_WRITER_MODE_REDIS:
			ZTC_LOG("Using Redis for status writer\n");
			if (_cc->redisConfig != NULL) {
				if (_cc->redisConfig->clusterMode) {
					_statusWriter = std::make_shared<RedisStatusWriter>(_cluster, _myAddressStr);
				}
				else {
					_statusWriter = std::make_shared<RedisStatusWriter>(_redis, _myAddressStr);
				}
			}
			else {
				throw std::runtime_error("CentralDB: Redis status mode selected but no Redis configuration provided");
			}
			break;
		case STATUS_WRITER_MODE_BIGTABLE:
			ZTC_LOG("Using BigTable for status writer\n");
			if (cc->bigTableConfig == NULL) {
				throw std::runtime_error(
					"CentralDB: BigTable status mode selected but no BigTable configuration provided");
			}
			if (cc->pubSubConfig == NULL) {
				throw std::runtime_error(
					"CentralDB: BigTable status mode selected but no PubSub configuration provided");
			}

			_statusWriter = std::make_shared<BigTableStatusWriter>(
				cc->bigTableConfig->project_id, cc->bigTableConfig->instance_id, cc->bigTableConfig->table_id);
			break;
		case STATUS_WRITER_MODE_PGSQL:
		default:
			ZTC_LOG("Using PostgreSQL for status writer\n");
			_statusWriter = std::make_shared<PostgresStatusWriter>(_pool);
			break;
	}

	// start background threads
	for (int i = 0; i < ZT_CENTRAL_CONTROLLER_COMMIT_THREADS; ++i) {
		_commitThread[i] = std::thread(&CentralDB::commitThread, this);
	}
	_onlineNotificationThread = std::thread(&CentralDB::onlineNotificationThread, this);
}

CentralDB::~CentralDB()
{
	// Stop the change listeners first. Each holds a raw DB* (this) and calls back into
	// save()/eraseNetwork()/eraseMember()/isReady() from its own worker thread. Those
	// methods touch _readyLock/_readyCv/_ready/_lastOnline/_commitQueue, which are
	// destroyed (reverse declaration order) before the listener shared_ptrs would run
	// their destructors -- so a message in flight during teardown would use freed sync
	// primitives -> SIGSEGV (exit 139). Joining the listener threads here, before any
	// CentralDB state is torn down, closes that window. stop() is idempotent, so the
	// listener destructors' safety-net calls remain harmless.
	if (_membersDbWatcher) {
		_membersDbWatcher->stop();
	}
	if (_networksDbWatcher) {
		_networksDbWatcher->stop();
	}
	if (_ssoAuthListener) {
		_ssoAuthListener->stop();
	}

	_run = 0;

	_heartbeatThread.join();
	_commitQueue.stop();
	for (int i = 0; i < ZT_CENTRAL_CONTROLLER_COMMIT_THREADS; ++i) {
		_commitThread[i].join();
	}
	_onlineNotificationThread.join();
}

bool CentralDB::waitForReady()
{
	// Block until the initial data download has completed (both initializeNetworks and
	// initializeMembers have run), rather than busy-spinning on a mutex.
	std::unique_lock<std::mutex> l(_readyLock);
	_readyCv.wait(l, [this] { return _ready.load() >= 2; });
	return true;
}

bool CentralDB::isReady()
{ return ((_ready == 2) && (_connected)); }

bool CentralDB::save(nlohmann::json& record, bool notifyListeners)
{
	auto provider = opentelemetry::trace::Provider::GetTracerProvider();
	auto tracer = provider->GetTracer("CentralDB");
	auto span = tracer->StartSpan("CentralDB::save");
	auto scope = tracer->WithActiveSpan(span);

	bool modified = false;
	try {
		if (! record.is_object()) {
			ZTC_LOG("record is not an object?!?\n");
			return false;
		}
		const std::string objtype = OSUtils::jsonString(record["objtype"], "");
		if (objtype == "network") {
			auto span = tracer->StartSpan("CentralDB::save::network");
			auto scope = tracer->WithActiveSpan(span);

			const uint64_t nwid = OSUtils::jsonIntHex(record["id"], 0ULL);
			if (nwid) {
				nlohmann::json old;
				get(nwid, old);
				if ((! old.is_object()) || (! _compareRecords(old, record))) {
					record["revision"] = OSUtils::jsonInt(record["revision"], 0ULL) + 1ULL;
					_queueItem qi;
					qi.jsonData = record;
					qi.notifyListeners = notifyListeners;
					OtelCarrier<std::map<std::string, std::string> > carrier(qi.traceContext);
					auto current_ctx = opentelemetry::context::RuntimeContext::GetCurrent();
					auto propagator =
						opentelemetry::context::propagation::GlobalTextMapPropagator::GetGlobalPropagator();
					propagator->Inject(carrier, current_ctx);
					_commitQueue.post(qi);
					modified = true;
				}
			}
		}
		else if (objtype == "member") {
			auto span = tracer->StartSpan("CentralDB::save::member");
			auto scope = tracer->WithActiveSpan(span);

			const uint64_t nwid = OSUtils::jsonIntHex(record["nwid"], 0ULL);
			const uint64_t id = OSUtils::jsonIntHex(record["id"], 0ULL);
			if ((id) && (nwid)) {
				nlohmann::json network, old;
				get(nwid, network, id, old);
				if ((! old.is_object()) || (! _compareRecords(old, record))) {
					record["revision"] = OSUtils::jsonInt(record["revision"], 0ULL) + 1ULL;

					_queueItem qi;
					qi.jsonData = record;
					qi.notifyListeners = notifyListeners;
					OtelCarrier<std::map<std::string, std::string> > carrier(qi.traceContext);
					auto current_ctx = opentelemetry::context::RuntimeContext::GetCurrent();
					auto propagator =
						opentelemetry::context::propagation::GlobalTextMapPropagator::GetGlobalPropagator();
					propagator->Inject(carrier, current_ctx);
					_commitQueue.post(qi);

					modified = true;
				}
				else {
					// fprintf(stderr, "no change\n");
				}
			}
		}
		else {
			ZTC_LOG("uhh waaat\n");
		}
	}
	catch (std::exception& e) {
		ZTC_LOG("Error on CentralDB::save: %s\n", e.what());
	}
	catch (...) {
		ZTC_LOG("Unknown error on CentralDB::save\n");
	}
	return modified;
}

void CentralDB::eraseNetwork(const uint64_t networkId)
{
	auto provider = opentelemetry::trace::Provider::GetTracerProvider();
	auto tracer = provider->GetTracer("CentralDB");
	auto span = tracer->StartSpan("CentralDB::eraseNetwork");
	auto scope = tracer->WithActiveSpan(span);
	char networkIdStr[17];
	span->SetAttribute("network_id", Utils::hex(networkId, networkIdStr));

	char tmp2[24];
	waitForReady();
	Utils::hex(networkId, tmp2);

	_queueItem qi;
	qi.jsonData["id"] = tmp2;
	qi.jsonData["objtype"] = "_delete_network";
	qi.notifyListeners = true;
	OtelCarrier<std::map<std::string, std::string> > carrier(qi.traceContext);
	auto current_ctx = opentelemetry::context::RuntimeContext::GetCurrent();
	auto propagator = opentelemetry::context::propagation::GlobalTextMapPropagator::GetGlobalPropagator();
	propagator->Inject(carrier, current_ctx);
	_commitQueue.post(qi);
	// The commit thread fires _networkChanged once with the real (cached) old config when
	// it processes the _delete_network item, so we don't notify here — doing so would
	// double-notify listeners (and double-decrement metrics) with only a stub config.
}

void CentralDB::eraseMember(const uint64_t networkId, const uint64_t memberId)
{
	auto provider = opentelemetry::trace::Provider::GetTracerProvider();
	auto tracer = provider->GetTracer("CentralDB");
	auto span = tracer->StartSpan("CentralDB::eraseMember");
	auto scope = tracer->WithActiveSpan(span);
	char networkIdStr[17];
	char memberIdStr[11];
	span->SetAttribute("network_id", Utils::hex(networkId, networkIdStr));
	span->SetAttribute("member_id", Utils::hex10(memberId, memberIdStr));

	waitForReady();

	_queueItem qi;
	qi.jsonData["nwid"] = networkIdStr;
	qi.jsonData["id"] = memberIdStr;
	qi.jsonData["objtype"] = "_delete_member";
	qi.notifyListeners = true;
	OtelCarrier<std::map<std::string, std::string> > carrier(qi.traceContext);
	auto current_ctx = opentelemetry::context::RuntimeContext::GetCurrent();
	auto propagator = opentelemetry::context::propagation::GlobalTextMapPropagator::GetGlobalPropagator();
	propagator->Inject(carrier, current_ctx);
	_commitQueue.post(qi);
	// The commit thread fires _memberChanged once with the real (cached) old config when
	// it processes the _delete_member item, so we don't notify here — doing so would
	// double-notify listeners (and double-decrement metrics) with only a stub config.
}

void CentralDB::nodeIsOnline(const uint64_t networkId,
							 const uint64_t memberId,
							 const InetAddress& physicalAddress,
							 const char* osArch)
{
	auto provider = opentelemetry::trace::Provider::GetTracerProvider();
	auto tracer = provider->GetTracer("CentralDB");
	auto span = tracer->StartSpan("CentralDB::nodeIsOnline");
	auto scope = tracer->WithActiveSpan(span);
	char networkIdStr[17];
	char memberIdStr[11];
	char ipStr[INET6_ADDRSTRLEN];
	// osArch may be NULL (public virtual taking a raw const char*); guard so the
	// SetAttribute / std::string construction below can't hit UB on the hot heartbeat path.
	const char* oa = osArch ? osArch : "unknown/unknown";
	span->SetAttribute("network_id", Utils::hex(networkId, networkIdStr));
	span->SetAttribute("member_id", Utils::hex10(memberId, memberIdStr));
	span->SetAttribute("physical_address", physicalAddress.toString(ipStr));
	span->SetAttribute("os_arch", oa);

	std::lock_guard<std::mutex> l(_lastOnline_l);
	NodeOnlineRecord& i = _lastOnline[std::pair<uint64_t, uint64_t>(networkId, memberId)];
	i.lastSeen = OSUtils::now();
	if (physicalAddress) {
		i.physicalAddress = physicalAddress;
	}
	i.osArch = std::string(oa);
}

void CentralDB::nodeIsOnline(const uint64_t networkId, const uint64_t memberId, const InetAddress& physicalAddress)
{ this->nodeIsOnline(networkId, memberId, physicalAddress, "unknown/unknown"); }

AuthInfo CentralDB::getSSOAuthInfo(const nlohmann::json& member, const std::string& redirectURL)
{
	if (_cc->ssoEnabled) {
		auto provider = opentelemetry::trace::Provider::GetTracerProvider();
		auto tracer = provider->GetTracer("CentralDB");
		auto span = tracer->StartSpan("CentralDB::getSSOAuthInfo");
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

		AuthInfo info;
		info.enabled = true;
		try {
			PooledConnection<PostgresConnection> c(_pool);
			pqxx::work w(*c->c);

			char nonceBytes[16] = { 0 };
			std::string nonce = "";

			// check if the member exists first.
			pqxx::row count =
				w.exec("SELECT count(*) FROM network_memberships_ctl WHERE device_id = $1 AND network_id = $2",
					   pqxx::params { memberId, networkId })
					.one_row();
			if (count[0].as<int>() == 1) {
				// Query the network's frontend while the transaction is guaranteed open.
				// It selects which Central's SSO redirect URL the member is sent to and
				// tags the nonce publish for PubSub routing.
				std::string frontend;
				{
					pqxx::result fr =
						w.exec("SELECT frontend FROM networks_ctl WHERE id = $1", pqxx::params { networkId });
					if (fr.size() == 1) {
						frontend = fr.at(0)[0].as<std::optional<std::string> >().value_or("");
					}
				}

				// get active nonce, if exists.
				pqxx::result r = w.exec(
					"SELECT nonce FROM sso_expiry "
					"WHERE network_id = $1 AND device_id = $2 "
					"AND ((NOW() AT TIME ZONE 'UTC') <= authentication_expiry_time) AND ((NOW() AT TIME ZONE 'UTC') <= "
					"nonce_expiration)",
					pqxx::params { networkId, memberId });

				if (r.size() == 0) {
					// no active nonce.
					// find an unused nonce, if one exists.
					pqxx::result r = w.exec(
						"SELECT nonce FROM sso_expiry "
						"WHERE network_id = $1 AND device_id = $2 "
						"AND authentication_expiry_time IS NULL AND ((NOW() AT TIME ZONE 'UTC') <= nonce_expiration)",
						pqxx::params { networkId, memberId });

					if (r.size() == 1) {
						// we have an existing nonce.  Use it
						nonce = r.at(0)[0].as<std::string>();
						Utils::unhex(nonce.c_str(), nonceBytes, sizeof(nonceBytes));
					}
					else if (r.empty()) {
						// create a nonce
						Utils::getSecureRandom(nonceBytes, 16);
						char nonceBuf[64] = { 0 };
						Utils::hex(nonceBytes, sizeof(nonceBytes), nonceBuf);
						nonce = std::string(nonceBuf);

						uint64_t nonceExpiration = OSUtils::now() + 300000ULL;
						pqxx::result ir = w.exec("INSERT INTO sso_expiry "
												 "(nonce, nonce_expiration, network_id, device_id) VALUES "
												 "($1, TO_TIMESTAMP($2::double precision/1000), $3, $4)",
												 pqxx::params { nonce, nonceExpiration, networkId, memberId });

						if (_ssoNonceWriter) {
							_ssoNonceWriter->publishSSONonceUpdate(nonce, nonceExpiration, networkId, memberId,
																   frontend);
						}
					}
					else {
						// > 1 unused nonce is a data anomaly (e.g. a race inserting nonces).
						// Abort and bail out gracefully rather than killing the controller process.
						ZTC_LOG("> 1 unused nonce for network member!\n");
						span->SetStatus(opentelemetry::trace::StatusCode::kError, "more than one unused nonce");
						w.abort();
						return AuthInfo();
					}
				}
				else if (r.size() == 1) {
					nonce = r.at(0)[0].as<std::string>();
					Utils::unhex(nonce.c_str(), nonceBytes, sizeof(nonceBytes));
				}
				else {
					// more than 1 active nonce is a data anomaly. Abort and bail out
					// gracefully rather than killing the controller process.
					ZTC_LOG("> 1 nonce in use for network member?!?\n");
					span->SetStatus(opentelemetry::trace::StatusCode::kError, "more than one active nonce");
					w.abort();
					return AuthInfo();
				}

				r = w.exec(
					"SELECT oc.client_id, oc.authorization_endpoint, oc.issuer, oc.provider, 1 AS sso_impl_version "
					"FROM oidc_config oc "
					"INNER JOIN networks_ctl n "
					" ON oc.client_id = n.configuration->'ssoConfig'->>'ssoClientId' "
					" AND oc.linked_id = n.configuration->'ssoConfig'->>'ssoLinkedId' "
					"WHERE n.id = $1 AND n.configuration->>'ssoEnabled' = 'true' ",
					pqxx::params { networkId });

				std::string client_id = "";
				std::string authorization_endpoint = "";
				std::string issuer = "";
				std::string provider = "";
				uint64_t sso_version = 0;

				if (r.size() == 1) {
					client_id = r.at(0)[0].as<std::optional<std::string> >().value_or("");
					authorization_endpoint = r.at(0)[1].as<std::optional<std::string> >().value_or("");
					issuer = r.at(0)[2].as<std::optional<std::string> >().value_or("");
					provider = r.at(0)[3].as<std::optional<std::string> >().value_or("");
					sso_version = r.at(0)[4].as<std::optional<uint64_t> >().value_or(1);
				}
				else if (r.size() > 1) {
					ZTC_LOG("ERROR: More than one auth endpoint for an organization?!?!? NetworkID: %s\n",
							networkId.c_str());
				}
				else {
					ZTC_LOG("No client or auth endpoint?!?\n");
				}

				info.version = sso_version;

				// Every SSO-enabled network must resolve to the redirect URL configured
				// for the Central frontend ("cv1" or "cv2") it belongs to.
				std::string ssoRedirectURL;
				auto fu = _cc->ssoRedirectURLs.find(frontend);
				if (fu != _cc->ssoRedirectURLs.end()) {
					ssoRedirectURL = fu->second;
				}

				if (ssoRedirectURL.empty()) {
					span->SetStatus(opentelemetry::trace::StatusCode::kError,
									"no SSO redirect URL configured for frontend");
					ZTC_LOG("ERROR: no SSO redirect URL configured for frontend '%s' of network %s\n", frontend.c_str(),
							networkId.c_str());
				}
				// no catch all else because we don't actually care if no records exist here. just continue as normal.
				else if ((! client_id.empty()) && (! authorization_endpoint.empty())) {
					uint8_t state[48];
					HMACSHA384(_ssoPsk, nonceBytes, sizeof(nonceBytes), state);
					char state_hex[256];
					Utils::hex(state, 48, state_hex);

					if (info.version == 0) {
						char url[4096] = { 0 };
						OSUtils::ztsnprintf(
							url, sizeof(url),
							"%s?response_type=id_token&response_mode=form_post&scope=openid+email+profile&redirect_uri="
							"%s&nonce=%s&state=%s&client_id=%s",
							authorization_endpoint.c_str(), url_encode(ssoRedirectURL).c_str(), nonce.c_str(),
							state_hex, client_id.c_str());
						info.authenticationURL = std::string(url);
					}
					else if (info.version == 1) {
						info.ssoClientID = client_id;
						info.issuerURL = issuer;
						info.ssoProvider = provider;
						info.ssoNonce = nonce;
						info.ssoState = std::string(state_hex) + "_" + networkId;
						info.centralAuthURL = ssoRedirectURL;
#ifdef ZT_DEBUG
						fprintf(
							stderr,
							"ssoClientID: %s\nissuerURL: %s\nssoNonce: %s\nssoState: %s\ncentralAuthURL: %s\nprovider: "
							"%s\n",
							info.ssoClientID.c_str(), info.issuerURL.c_str(), info.ssoNonce.c_str(),
							info.ssoState.c_str(), info.centralAuthURL.c_str(), provider.c_str());
#endif
					}
				}
				else {
					ZTC_LOG("client_id: %s\nauthorization_endpoint: %s\n", client_id.c_str(),
							authorization_endpoint.c_str());
				}
			}
			w.commit();
			// c (PooledConnection) returns the connection to the pool on scope exit.
		}
		catch (std::exception& e) {
			span->SetStatus(opentelemetry::trace::StatusCode::kError, e.what());
			ZTC_LOG("ERROR: Error updating member on load for network %s: %s\n", networkId.c_str(), e.what());
		}

		return info;   // std::string(authenticationURL);
	}
	return AuthInfo();
}

void CentralDB::initializeNetworks()
{
	auto provider = opentelemetry::trace::Provider::GetTracerProvider();
	auto tracer = provider->GetTracer("CentralDB");
	auto span = tracer->StartSpan("CentralDB::initializeNetworks");
	auto scope = tracer->WithActiveSpan(span);

	ZTC_LOG("Initializing networks...\n");

	try {
		char qbuf[2048];
		sprintf(qbuf,
				"SELECT id, name, configuration , (EXTRACT(EPOCH FROM creation_time AT TIME ZONE 'UTC')*1000)::bigint, "
				"(EXTRACT(EPOCH FROM last_modified AT TIME ZONE 'UTC')*1000)::bigint, revision, frontend "
				"FROM networks_ctl WHERE controller_id = '%s'",
				_myAddressStr.c_str());

		PooledConnection<PostgresConnection> c(_pool);
		pqxx::work w(*c->c);

		ZTC_LOG("Load networks from psql...\n");
		auto stream = pqxx::stream_from::query(w, qbuf);
		std::tuple<std::string	 // network ID
				   ,
				   std::optional<std::string>	// name
				   ,
				   std::string	 // configuration
				   ,
				   std::optional<uint64_t>	 // creation_time
				   ,
				   std::optional<uint64_t>	 // last_modified
				   ,
				   std::optional<uint64_t>	 // revision
				   ,
				   std::string	 // frontend
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
			std::string frontend = std::get<6>(row);

			config["id"] = nwid;
			config["name"] = name;
			config["creationTime"] = created_at.value_or(0);
			config["lastModified"] = last_modified.value_or(0);
			config["revision"] = revision.value_or(0);
			config["capabilities"] = cfgtmp["capabilities"].is_array() ? cfgtmp["capabilities"] : json::array();
			config["enableBroadcast"] =
				cfgtmp["enableBroadcast"].is_boolean() ? cfgtmp["enableBroadcast"].get<bool>() : true;
			config["mtu"] = cfgtmp["mtu"].is_number() ? cfgtmp["mtu"].get<int32_t>() : 2800;
			config["multicastLimit"] =
				cfgtmp["multicastLimit"].is_number() ? cfgtmp["multicastLimit"].get<int32_t>() : 64;
			config["private"] = cfgtmp["private"].is_boolean() ? cfgtmp["private"].get<bool>() : true;
			config["remoteTraceLevel"] =
				cfgtmp["remoteTraceLevel"].is_number() ? cfgtmp["remoteTraceLevel"].get<int32_t>() : 0;
			config["remoteTraceTarget"] =
				cfgtmp["remoteTraceTarget"].is_string() ? cfgtmp["remoteTraceTarget"].get<std::string>() : "";
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
			nlohmann::json ssocfg = cfgtmp["ssoConfig"].is_object() ? cfgtmp["ssoConfig"] : nlohmann::json::object();
			ssocfg["ssoClientId"] = ssocfg["ssoClientId"].is_string() ? ssocfg["ssoClientId"].get<std::string>() : "";
			ssocfg["ssoLinkedId"] = ssocfg["ssoLinkedId"].is_string() ? ssocfg["ssoLinkedId"].get<std::string>() : "";
			config["ssoConfig"] = ssocfg;
			config["objtype"] = "network";
			config["routes"] = cfgtmp["routes"].is_array() ? cfgtmp["routes"] : json::array();
			if (! cfgtmp["dns"].is_object()) {
				config["dns"] = json::object();
				config["dns"]["domain"] = "";
				config["dns"]["servers"] = json::array();
			}
			else {
				config["dns"] = cfgtmp["dns"];
			}
			config["ipAssignmentPools"] =
				cfgtmp["ipAssignmentPools"].is_array() ? cfgtmp["ipAssignmentPools"] : json::array();
			config["frontend"] = frontend;

			Metrics::network_count++;

			_networkChanged(empty, config, false);

			auto end = std::chrono::high_resolution_clock::now();
			auto dur = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

			total += dur.count();
			++count;
			if (count > 0 && count % 10000 == 0) {
				ZTC_LOG("Averaging %llu us per network\n", (unsigned long long)(total / count));
			}
		}

		w.commit();
		// c (PooledConnection) returns the connection to the pool on scope exit.
		ZTC_LOG("done.\n");

		if (++this->_ready == 2) {
			if (_waitNoticePrinted) {
				ZTC_LOG("NOTICE: PostgreSQL data download complete." ZT_EOL_S);
			}
			std::lock_guard<std::mutex> l(_readyLock);
			_readyCv.notify_all();
		}
		ZTC_LOG("network init done\n");
	}
	catch (std::exception& e) {
		ZTC_LOG("ERROR: Error initializing networks: %s\n", e.what());
		span->SetStatus(opentelemetry::trace::StatusCode::kError, e.what());
		std::this_thread::sleep_for(std::chrono::milliseconds(5000));
		exit(-1);
	}
}

void CentralDB::initializeMembers()
{
	auto provider = opentelemetry::trace::Provider::GetTracerProvider();
	auto tracer = provider->GetTracer("CentralDB");
	auto span = tracer->StartSpan("CentralDB::initializeMembers");
	auto scope = tracer->WithActiveSpan(span);

	std::string memberId;
	std::string networkId;
	try {
		std::unordered_map<std::string, std::string> networkMembers;
		ZTC_LOG("Initializing Members...\n");

		std::string setKeyBase = "network-nodes-all:{" + _myAddressStr + "}:";

		if (_redisMemberStatus) {
			ZTC_LOG("Initialize Redis for members...\n");
			std::unique_lock<std::shared_mutex> l(_networks_l);
			std::unordered_set<std::string> deletes;
			for (auto it : _networks) {
				uint64_t nwid_i = it.first;
				char nwidTmp[64] = { 0 };
				OSUtils::ztsnprintf(nwidTmp, sizeof(nwidTmp), "%.16llx", nwid_i);
				std::string nwid(nwidTmp);
				std::string key = setKeyBase + nwid;
				deletes.insert(key);
			}

			if (! deletes.empty()) {
				try {
					if (_cc->redisConfig->clusterMode) {
						auto tx = _cluster->transaction(_myAddressStr, true, false);
						for (std::string k : deletes) {
							tx.del(k);
						}
						tx.exec();
					}
					else {
						auto tx = _redis->transaction(true, false);
						for (std::string k : deletes) {
							tx.del(k);
						}
						tx.exec();
					}
				}
				catch (sw::redis::Error& e) {
					// ignore
				}
			}
		}

		char qbuf[2048];
		sprintf(qbuf,
				"SELECT nm.device_id, nm.network_id, nm.authorized, nm.active_bridge, nm.ip_assignments, "
				"nm.no_auto_assign_ips, "
				"nm.sso_exempt, (EXTRACT(EPOCH FROM nm.authentication_expiry_time AT TIME ZONE 'UTC')*1000)::bigint, "
				"(EXTRACT(EPOCH FROM nm.creation_time AT TIME ZONE 'UTC')*1000)::bigint, nm.identity, "
				"(EXTRACT(EPOCH FROM nm.last_authorized_time AT TIME ZONE 'UTC')*1000)::bigint, "
				"(EXTRACT(EPOCH FROM nm.last_deauthorized_time AT TIME ZONE 'UTC')*1000)::bigint, "
				"nm.remote_trace_level, nm.remote_trace_target, nm.revision, nm.capabilities, nm.tags, "
				"nm.frontend, nm.version_major, nm.version_minor, nm.version_revision, nm.version_protocol "
				"FROM network_memberships_ctl nm "
				"INNER JOIN networks_ctl n "
				"  ON nm.network_id = n.id "
				"WHERE n.controller_id = '%s'",
				_myAddressStr.c_str());

		PooledConnection<PostgresConnection> c(_pool);
		pqxx::work w(*c->c);
		ZTC_LOG("Load members from psql...\n");
		auto stream = pqxx::stream_from::query(w, qbuf);
		std::tuple<std::string	 // device ID
				   ,
				   std::string	 // network ID
				   ,
				   bool	  // authorized
				   ,
				   std::optional<bool>	 // active_bridge
				   ,
				   std::optional<std::string>	// ip_assignments
				   ,
				   std::optional<bool>	 // no_auto_assign_ips
				   ,
				   std::optional<bool>	 // sso_exempt
				   ,
				   std::optional<uint64_t>	 // authentication_expiry_time
				   ,
				   std::optional<uint64_t>	 // creation_time
				   ,
				   std::optional<std::string>	// identity
				   ,
				   std::optional<uint64_t>	 // last_authorized_time
				   ,
				   std::optional<uint64_t>	 // last_deauthorized_time
				   ,
				   std::optional<int32_t>	// remote_trace_level
				   ,
				   std::optional<std::string>	// remote_trace_target
				   ,
				   std::optional<uint64_t>	 // revision
				   ,
				   std::optional<std::string>	// capabilities
				   ,
				   std::optional<std::string>	// tags
				   ,
				   std::string	 // frontend
				   ,
				   std::optional<int32_t>	// version_major
				   ,
				   std::optional<int32_t>	// version_minor
				   ,
				   std::optional<int32_t>	// version_revision
				   ,
				   std::optional<int32_t>	// version_protocol
				   >
			row;

		auto tmp = std::chrono::high_resolution_clock::now();
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

			networkMembers.insert(std::pair<std::string, std::string>(setKeyBase + networkId, memberId));

			config["objtype"] = "member";
			config["id"] = memberId;
			config["address"] = memberId;
			config["identity"] = identity.value_or("");
			config["nwid"] = networkId;
			config["authorized"] = authorized;
			config["activeBridge"] = active_bridge.value_or(false);
			config["ipAssignments"] = json::array();
			if (! ip_assignments.empty() && ip_assignments != "{}" && ip_assignments != "[]") {
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
			config["noAutoAssignIps"] = no_auto_assign_ips.value_or(false);
			config["remoteTraceLevel"] = remote_trace_level.value_or(0);
			config["remoteTraceTarget"] = remote_trace_target.value_or(nullptr);
			config["revision"] = revision.value_or(0);
			config["ssoExempt"] = sso_exempt.value_or(false);
			config["authenticationExpiryTime"] = authentication_expiry_time.value_or(0);
			config["tags"] = json::parse(tags.value_or("[]"));
			config["frontend"] = std::get<17>(row);
			config["vMajor"] = std::get<18>(row).value_or(-1);
			config["vMinor"] = std::get<19>(row).value_or(-1);
			config["vRev"] = std::get<20>(row).value_or(-1);
			config["vProto"] = std::get<21>(row).value_or(-1);

			Metrics::member_count++;

			_memberChanged(empty, config, false);

			memberId = "";
			networkId = "";

			auto end = std::chrono::high_resolution_clock::now();
			auto dur = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
			total += dur.count();
			++count;
			if (count > 0 && count % 10000 == 0) {
				ZTC_LOG("Averaging %llu us per member\n", (unsigned long long)(total / count));
			}
		}
		if (count > 0) {
			ZTC_LOG("Took %llu us per member to load\n", (unsigned long long)(total / count));
		}

		stream.complete();

		w.commit();
		// c (PooledConnection) returns the connection to the pool on scope exit.
		ZTC_LOG("done.\n");

		if (_listenerMode == LISTENER_MODE_REDIS)
			if (! networkMembers.empty()) {
				if (_redisMemberStatus) {
					ZTC_LOG("Load member data into redis...\n");
					if (_cc->redisConfig->clusterMode) {
						auto tx = _cluster->transaction(_myAddressStr, true, false);
						uint64_t count = 0;
						for (auto it : networkMembers) {
							tx.sadd(it.first, it.second);
							if (++count % 30000 == 0) {
								tx.exec();
								tx = _cluster->transaction(_myAddressStr, true, false);
							}
						}
						tx.exec();
					}
					else {
						auto tx = _redis->transaction(true, false);
						uint64_t count = 0;
						for (auto it : networkMembers) {
							tx.sadd(it.first, it.second);
							if (++count % 30000 == 0) {
								tx.exec();
								tx = _redis->transaction(true, false);
							}
						}
						tx.exec();
					}
					ZTC_LOG("done.\n");
				}
			}

		ZTC_LOG("Done loading members...\n");

		if (++this->_ready == 2) {
			if (_waitNoticePrinted) {
				ZTC_LOG("NOTICE: PostgreSQL data download complete." ZT_EOL_S);
			}
			std::lock_guard<std::mutex> l(_readyLock);
			_readyCv.notify_all();
		}
	}
	catch (sw::redis::Error& e) {
		span->SetStatus(opentelemetry::trace::StatusCode::kError, e.what());
		ZTC_LOG("ERROR: Error initializing members (redis): %s\n", e.what());
		exit(-1);
	}
	catch (std::exception& e) {
		span->SetStatus(opentelemetry::trace::StatusCode::kError, e.what());
		ZTC_LOG("ERROR: Error initializing member: %s-%s %s\n", networkId.c_str(), memberId.c_str(), e.what());
		exit(-1);
	}
}

void CentralDB::heartbeat()
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
		auto tracer = provider->GetTracer("CentralDB");
		auto span = tracer->StartSpan("CentralDB::heartbeat");
		auto scope = tracer->WithActiveSpan(span);

		// fprintf(stderr, "%s: heartbeat\n", controllerId);
		PooledConnection<PostgresConnection> c(_pool);
		int64_t ts = OSUtils::now();

		if (c->c) {
			std::string major = std::to_string(ZEROTIER_ONE_VERSION_MAJOR);
			std::string minor = std::to_string(ZEROTIER_ONE_VERSION_MINOR);
			std::string rev = std::to_string(ZEROTIER_ONE_VERSION_REVISION);
			std::string version = major + "." + minor + "." + rev;
			std::string versionStr = "v" + version;

			try {
				pqxx::work w { *c->c };
				w.exec("INSERT INTO controllers_ctl (id, hostname, last_heartbeat, public_identity, version, "
					   "assigned_central_version) VALUES "
					   "($1, $2, TO_TIMESTAMP($3::double precision/1000), $4, $5, $6) "
					   "ON CONFLICT (id) DO UPDATE SET hostname = EXCLUDED.hostname, last_heartbeat = "
					   "EXCLUDED.last_heartbeat, "
					   "public_identity = EXCLUDED.public_identity, version = EXCLUDED.version, "
					   "assigned_central_version = EXCLUDED.assigned_central_version",
					   pqxx::params { controllerId, hostname, ts, publicIdentity, versionStr, _assignedCentralVersion })
					.no_rows();
				w.commit();
			}
			catch (std::exception& e) {
				ZTC_LOG("Heartbeat update failed: %s\n", e.what());
				c.unborrow();
				span->End();
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
				continue;
			}
		}
		c.unborrow();

		try {
			if (_listenerMode == LISTENER_MODE_REDIS && _redisMemberStatus) {
				if (_cc->redisConfig->clusterMode) {
					_cluster->zadd("controllers", controllerId, ts);
				}
				else {
					_redis->zadd("controllers", controllerId, ts);
				}
			}
		}
		catch (sw::redis::Error& e) {
			ZTC_LOG("ERROR: Redis error in heartbeat thread: %s\n", e.what());
		}

		span->End();
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}
	ZTC_LOG("Exited heartbeat thread\n");
}

void CentralDB::_requeueFailedCommit(_queueItem& qitem)
{
	// The PubSub message that produced this change was already acked when it was
	// enqueued, so a failed DB write would otherwise be lost.  Re-queue it (with
	// capped backoff) so transient failures -- e.g. an AlloyDB maintenance restart
	// -- are retried rather than dropped.  A genuinely poison item is dropped after
	// a bounded number of attempts so it can't spin a commit thread forever.
	if (_run != 1) {
		// shutting down -- don't requeue
		return;
	}
	if (qitem.retryCount >= ZT_CENTRAL_CONTROLLER_MAX_COMMIT_RETRIES) {
		ZTC_LOG("ERROR: dropping %s change after %d failed commit attempts\n",
				OSUtils::jsonString(qitem.jsonData["objtype"], "?").c_str(), qitem.retryCount);
		return;
	}
	++qitem.retryCount;

	// Capped linear backoff, slept in small chunks so shutdown stays responsive.
	int64_t backoffMs = (int64_t)qitem.retryCount * 500LL;
	if (backoffMs > 10000LL) {
		backoffMs = 10000LL;
	}
	for (int64_t slept = 0; slept < backoffMs && _run == 1;) {
		int64_t chunk = (backoffMs - slept) < 250LL ? (backoffMs - slept) : 250LL;
		std::this_thread::sleep_for(std::chrono::milliseconds(chunk));
		slept += chunk;
	}
	if (_run != 1) {
		return;
	}
	_commitQueue.post(qitem);
}

void CentralDB::commitThread()
{
	ZTC_LOG("commitThread start\n");
	_queueItem qitem;
	while (_commitQueue.get(qitem) && (_run == 1)) {
		auto provider = opentelemetry::trace::Provider::GetTracerProvider();
		auto tracer = provider->GetTracer("CentralDB");

		auto propagator = opentelemetry::context::propagation::GlobalTextMapPropagator::GetGlobalPropagator();
		OtelCarrier<std::map<std::string, std::string> > carrier(qitem.traceContext);
		auto current_ctx = opentelemetry::context::RuntimeContext::GetCurrent();
		auto new_context = propagator->Extract(carrier, current_ctx);
		auto remote_span = opentelemetry::trace::GetSpan(new_context);
		auto remote_scope = tracer->WithActiveSpan(remote_span);

		{
			auto span = tracer->StartSpan("CentralDB::commitThread");
			auto scope = tracer->WithActiveSpan(span);

			if (! qitem.jsonData.is_object()) {
				ZTC_LOG("commitThread tick: skipping non-object queue item\n");
				continue;
			}

			// Describe the operation this tick is performing.  nwid/id cover every
			// objtype: member/_delete_member set both; network/_delete_network use id
			// as the network id.
			const std::string tickObjtype = OSUtils::jsonString(qitem.jsonData["objtype"], "?");
			const std::string tickNwid = OSUtils::jsonString(qitem.jsonData["nwid"], "");
			const std::string tickId = OSUtils::jsonString(qitem.jsonData["id"], "");
			span->SetAttribute("objtype", tickObjtype);
			span->SetAttribute("nwid", tickNwid);
			span->SetAttribute("id", tickId);
			span->SetAttribute("retry", qitem.retryCount);
			ZTC_LOG("commitThread tick: objtype=%s nwid=%s id=%s retry=%d notify=%d\n", tickObjtype.c_str(),
					tickNwid.empty() ? "-" : tickNwid.c_str(), tickId.empty() ? "-" : tickId.c_str(), qitem.retryCount,
					qitem.notifyListeners ? 1 : 0);

			bool commitFailed = false;

			std::shared_ptr<PostgresConnection> c;
			try {
				c = _pool->borrow();
			}
			catch (std::exception& e) {
				ZTC_LOG("ERROR: %s\n", e.what());
				_requeueFailedCommit(qitem);
				continue;
			}

			if (! c) {
				ZTC_LOG("Error getting database connection\n");
				_requeueFailedCommit(qitem);
				continue;
			}

			Metrics::pgsql_commit_ticks++;
			try {
				nlohmann::json& config = (qitem.jsonData);
				const std::string objtype = OSUtils::jsonString(config["objtype"], "");
				if (objtype == "member") {
					auto mspan = tracer->StartSpan("CentralDB::commitThread::member");
					auto mscope = tracer->WithActiveSpan(mspan);

					// fprintf(stderr, "%s: commitThread: member\n", _myAddressStr.c_str());
					std::string memberId;
					std::string networkId;

					try {
						pqxx::work w(*c->c);

						memberId = OSUtils::jsonString(config["id"], "");
						networkId = OSUtils::jsonString(config["nwid"], "");

						// Default to "" so a null/absent remoteTraceTarget is written as an
						// empty string rather than SQL NULL.
						std::optional<std::string> target = "";
						if (config["remoteTraceTarget"].is_string()) {
							target = config["remoteTraceTarget"].get<std::string>();
						}

						// get network and the frontend it is assigned to
						// if network does not exist, skip member update
						pqxx::row nwrow =
							w.exec("SELECT COUNT(id), frontend FROM networks_ctl WHERE id = $1 GROUP BY frontend",
								   pqxx::params { networkId })
								.one_row();
						int nwcount = nwrow[0].as<int>();
						std::string frontend = nwrow[1].as<std::string>();

						if (nwcount != 1) {
							ZTC_LOG("network %s does not exist.  skipping member upsert\n", networkId.c_str());
							w.abort();
							_pool->unborrow(c);
							continue;
						}

						pqxx::row mrow =
							w.exec("SELECT COUNT(device_id) FROM network_memberships_ctl WHERE device_id = $1 "
								   "AND network_id = $2",
								   pqxx::params { memberId, networkId })
								.one_row();
						int membercount = mrow[0].as<int>();
						bool isNewMember = (membercount == 0);

						std::string change_source = "controller";
						if (config["change_source"].is_string()) {
							change_source = config["change_source"].get<std::string>();
						}
						if (! isNewMember && change_source != "controller" && frontend != change_source) {
							// if it is not a new member and the change source is not the controller and doesn't match
							// the frontend, don't apply the change.
							w.abort();
							_pool->unborrow(c);
							continue;
						}

						std::vector<std::string> ipAssignments;
						if (config["ipAssignments"].is_array()) {
							for (auto& ip : config["ipAssignments"]) {
								if (ip.is_string()) {
									ipAssignments.push_back(ip.get<std::string>());
								}
							}
						}

						int64_t vMajor = OSUtils::jsonUInt(config["vMajor"], 0);
						int64_t vMinor = OSUtils::jsonUInt(config["vMinor"], 0);
						int64_t vRev = OSUtils::jsonUInt(config["vRev"], 0);
						int64_t vProto = OSUtils::jsonUInt(config["vProto"], 0);
						if (vMajor < 0)
							vMajor = 0;
						if (vMinor < 0)
							vMinor = 0;
						if (vRev < 0)
							vRev = 0;
						if (vProto < 0)
							vProto = 0;

						// Capture old DB state before INSERT so PubSub notifier sees real prior values.
						// Must happen before w.commit() — _getNetworkMember reuses this transaction.
						nlohmann::json oldMember;
						if (! isNewMember && _listenerMode == LISTENER_MODE_PUBSUB
							&& (config["change_source"].is_null() || config["change_source"] == "controller")) {
							oldMember = _getNetworkMember(w, networkId, memberId);
						}

						pqxx::result res =
							w.exec(
								 "INSERT INTO network_memberships_ctl (device_id, network_id, authorized, "
								 "active_bridge, "
								 "ip_assignments, "
								 "no_auto_assign_ips, sso_exempt, authentication_expiry_time, capabilities, "
								 "creation_time, "
								 "identity, last_authorized_time, last_deauthorized_time, "
								 "remote_trace_level, remote_trace_target, revision, tags, version_major, "
								 "version_minor, "
								 "version_revision, version_protocol, frontend) "
								 "VALUES ($1, $2, $3, $4, $5, $6, $7, TO_TIMESTAMP($8::double precision/1000), $9, "
								 "TO_TIMESTAMP($10::double precision/1000), $11, TO_TIMESTAMP($12::double "
								 "precision/1000), "
								 "TO_TIMESTAMP($13::double precision/1000), $14, $15, $16, $17, $18, $19, $20, $21, "
								 "$22) "
								 "ON CONFLICT (device_id, network_id) DO UPDATE SET "
								 "authorized = EXCLUDED.authorized, active_bridge = EXCLUDED.active_bridge, "
								 "ip_assignments = EXCLUDED.ip_assignments, no_auto_assign_ips = "
								 "EXCLUDED.no_auto_assign_ips, "
								 "sso_exempt = EXCLUDED.sso_exempt, authentication_expiry_time = "
								 "EXCLUDED.authentication_expiry_time, "
								 "capabilities = EXCLUDED.capabilities, creation_time = EXCLUDED.creation_time, "
								 "identity = EXCLUDED.identity, last_authorized_time = EXCLUDED.last_authorized_time, "
								 "last_deauthorized_time = EXCLUDED.last_deauthorized_time, "
								 "remote_trace_level = EXCLUDED.remote_trace_level, remote_trace_target = "
								 "EXCLUDED.remote_trace_target, "
								 "revision = EXCLUDED.revision, tags = EXCLUDED.tags, version_major = "
								 "EXCLUDED.version_major, "
								 "version_minor = EXCLUDED.version_minor, version_revision = "
								 "EXCLUDED.version_revision, "
								 "version_protocol = EXCLUDED.version_protocol, "
								 "frontend = EXCLUDED.frontend, last_modified = now()",
								 pqxx::params { memberId,
												networkId,
												OSUtils::jsonBool(config["authorized"], false),
												OSUtils::jsonBool(config["activeBridge"], false),
												ipAssignments,
												OSUtils::jsonBool(config["noAutoAssignIps"], false),
												OSUtils::jsonBool(config["ssoExempt"], false),
												OSUtils::jsonInt(config["authenticationExpiryTime"], 0),
												OSUtils::jsonDump(config["capabilities"], -1),
												OSUtils::jsonInt(config["creationTime"], OSUtils::now()),
												OSUtils::jsonString(config["identity"], ""),
												OSUtils::jsonInt(config["lastAuthorizedTime"], 0),
												OSUtils::jsonInt(config["lastDeauthorizedTime"], 0),
												OSUtils::jsonInt(config["remoteTraceLevel"], 0),
												target,
												OSUtils::jsonInt(config["revision"], 0),
												OSUtils::jsonDump(config["tags"], -1),
												vMajor,
												vMinor,
												vRev,
												vProto,
												frontend })
								.no_rows();

						w.commit();

						if (_listenerMode == LISTENER_MODE_PUBSUB) {
							// Publish change to pubsub stream — oldMember was captured before INSERT
							if (config["change_source"].is_null() || config["change_source"] == "controller") {
								_changeNotifier->notifyMemberChange(oldMember, config, frontend);
							}
						}

						const uint64_t nwidInt = OSUtils::jsonIntHex(config["nwid"], 0ULL);
						const uint64_t memberidInt = OSUtils::jsonIntHex(config["id"], 0ULL);
						if (nwidInt && memberidInt) {
							nlohmann::json nwOrig;
							nlohmann::json memOrig;

							nlohmann::json memNew(config);

							get(nwidInt, nwOrig, memberidInt, memOrig);

							_memberChanged(memOrig, memNew, qitem.notifyListeners);
						}
						else {
							ZTC_LOG("Can't notify of change.  Error parsing nwid or memberid: %llu-%llu\n",
									(unsigned long long)nwidInt, (unsigned long long)memberidInt);
						}
					}
					catch (std::exception& e) {
						ZTC_LOG("ERROR: Error updating member %s-%s: %s\n", networkId.c_str(), memberId.c_str(),
								e.what());
						mspan->SetStatus(opentelemetry::trace::StatusCode::kError, e.what());
						commitFailed = true;
					}
				}
				else if (objtype == "network") {
					auto nspan = tracer->StartSpan("CentralDB::commitThread::network");
					auto nscope = tracer->WithActiveSpan(nspan);

					try {
						// fprintf(stderr, "%s: commitThread: network\n", _myAddressStr.c_str());
						pqxx::work w(*c->c);

						std::string id = OSUtils::jsonString(config["id"], "");

						// id is the PK, so this returns at most one row: present ⇒ the network
						// exists (and we read its frontend), empty ⇒ it's a new network. (A
						// result rather than one_row(), which would throw on the expected
						// zero-row "new network" case.)
						pqxx::result nwres =
							w.exec("SELECT frontend FROM networks_ctl WHERE id = $1", pqxx::params { id });
						bool isNewNetwork = nwres.empty();
						std::string frontend = "";
						if (! isNewNetwork) {
							frontend = nwres[0][0].as<std::optional<std::string> >().value_or("");
						}

						std::string change_source;
						if (config["change_source"].is_string()) {
							change_source = config["change_source"].get<std::string>();
						}

						if (! isNewNetwork && change_source != "controller" && frontend != change_source) {
							// if it is not a new network and the change source is not the controller and doesn't match
							// the frontend, don't apply the change.
							w.abort();
							_pool->unborrow(c);
							continue;
						}

						// Capture old DB state before INSERT so PubSub notifier sees real prior values.
						// Must happen before w.commit() — _getNetwork reuses this transaction.
						nlohmann::json oldNetwork;
						if (! isNewNetwork && _listenerMode == LISTENER_MODE_PUBSUB
							&& (config["change_source"].is_null() || config["change_source"] == "controller")) {
							oldNetwork = _getNetwork(w, id);
						}

						// For new networks, tag the row with the originating frontend (change_source).
						// For existing networks, reuse the stored frontend so the ON CONFLICT update
						// is self-healing rather than ownership-changing (a controller-originated
						// message must not silently overwrite the per-network owner).
						std::string frontendParam = isNewNetwork ? change_source : frontend;

						pqxx::result res = w.exec(
							"INSERT INTO networks_ctl (id, name, configuration, controller_id, revision, frontend) "
							"VALUES ($1, $2, $3, $4, $5, $6) "
							"ON CONFLICT (id) DO UPDATE SET "
							"name = EXCLUDED.name, configuration = EXCLUDED.configuration, revision = "
							"EXCLUDED.revision, frontend = EXCLUDED.frontend, last_modified = now()",
							pqxx::params { id, OSUtils::jsonString(config["name"], ""), OSUtils::jsonDump(config, -1),
										   _myAddressStr, OSUtils::jsonInt(config["revision"], 0), frontendParam });

						w.commit();

						if (_listenerMode == LISTENER_MODE_PUBSUB) {
							// Publish change to pubsub stream — oldNetwork was captured before INSERT
							if (config["change_source"].is_null() || config["change_source"] == "controller") {
								_changeNotifier->notifyNetworkChange(oldNetwork, config, frontend);
							}
						}

						const uint64_t nwidInt = OSUtils::jsonIntHex(config["id"], 0ULL);
						if (nwidInt) {
							nlohmann::json nwOrig;
							nlohmann::json nwNew(config);

							get(nwidInt, nwOrig);

							_networkChanged(nwOrig, nwNew, qitem.notifyListeners);
						}
						else {
							ZTC_LOG("Can't notify network changed: %llu\n", (unsigned long long)nwidInt);
						}
					}
					catch (std::exception& e) {
						nspan->SetStatus(opentelemetry::trace::StatusCode::kError, e.what());
						ZTC_LOG("ERROR: Error updating network: %s\n", e.what());
						commitFailed = true;
					}
					if (_listenerMode == LISTENER_MODE_REDIS && _redisMemberStatus) {
						try {
							std::string id = OSUtils::jsonString(config["id"], "");
							std::string controllerId = _myAddressStr.c_str();
							std::string key = "networks:{" + controllerId + "}";
							if (_cc->redisConfig->clusterMode) {
								_cluster->sadd(key, id);
							}
							else {
								_redis->sadd(key, id);
							}
						}
						catch (sw::redis::Error& e) {
							nspan->SetStatus(opentelemetry::trace::StatusCode::kError, e.what());
							ZTC_LOG("ERROR: Error adding network to Redis: %s\n", e.what());
						}
					}
				}
				else if (objtype == "_delete_network") {
					auto dspan = tracer->StartSpan("CentralDB::commitThread::_delete_network");
					auto dscope = tracer->WithActiveSpan(dspan);

					// fprintf(stderr, "%s: commitThread: delete network\n", _myAddressStr.c_str());
					try {
						pqxx::work w(*c->c);
						std::string networkId = OSUtils::jsonString(config["id"], "");
						w.exec("DELETE FROM network_memberships_ctl WHERE network_id = $1", pqxx::params { networkId });
						w.exec("DELETE FROM networks_ctl WHERE id = $1", pqxx::params { networkId });

						w.commit();

						uint64_t nwidInt = OSUtils::jsonIntHex(config["id"], 0ULL);
						json oldConfig;
						get(nwidInt, oldConfig);
						json empty;
						_networkChanged(oldConfig, empty, qitem.notifyListeners);
					}
					catch (std::exception& e) {
						dspan->SetStatus(opentelemetry::trace::StatusCode::kError, e.what());
						ZTC_LOG("ERROR: Error deleting network: %s\n", e.what());
						commitFailed = true;
					}
					if (_listenerMode == LISTENER_MODE_REDIS && _redisMemberStatus) {
						try {
							std::string id = OSUtils::jsonString(config["id"], "");
							std::string controllerId = _myAddressStr.c_str();
							std::string key = "networks:{" + controllerId + "}";
							if (_cc->redisConfig->clusterMode) {
								_cluster->srem(key, id);
								_cluster->del("network-nodes-online:{" + controllerId + "}:" + id);
							}
							else {
								_redis->srem(key, id);
								_redis->del("network-nodes-online:{" + controllerId + "}:" + id);
							}
						}
						catch (sw::redis::Error& e) {
							dspan->SetStatus(opentelemetry::trace::StatusCode::kError, e.what());
							ZTC_LOG("ERROR: Error adding network to Redis: %s\n", e.what());
						}
					}
				}
				else if (objtype == "_delete_member") {
					auto mspan = tracer->StartSpan("CentralDB::commitThread::_delete_member");
					auto mscope = tracer->WithActiveSpan(mspan);

					// fprintf(stderr, "%s commitThread: delete member\n", _myAddressStr.c_str());
					try {
						pqxx::work w(*c->c);

						std::string memberId = OSUtils::jsonString(config["id"], "");
						std::string networkId = OSUtils::jsonString(config["nwid"], "");

						pqxx::result res =
							w.exec("DELETE FROM network_memberships_ctl WHERE device_id = $1 AND network_id = $2",
								   pqxx::params { memberId, networkId })
								.no_rows();

						w.commit();

						uint64_t nwidInt = OSUtils::jsonIntHex(config["nwid"], 0ULL);
						uint64_t memberidInt = OSUtils::jsonIntHex(config["id"], 0ULL);

						nlohmann::json networkConfig;
						nlohmann::json oldConfig;

						get(nwidInt, networkConfig, memberidInt, oldConfig);
						json empty;
						_memberChanged(oldConfig, empty, qitem.notifyListeners);
					}
					catch (std::exception& e) {
						mspan->SetStatus(opentelemetry::trace::StatusCode::kError, e.what());
						ZTC_LOG("ERROR: Error deleting member: %s\n", e.what());
						commitFailed = true;
					}
					if (_listenerMode == LISTENER_MODE_REDIS && _redisMemberStatus) {
						try {
							std::string memberId = OSUtils::jsonString(config["id"], "");
							std::string networkId = OSUtils::jsonString(config["nwid"], "");
							std::string controllerId = _myAddressStr.c_str();
							std::string key = "network-nodes-all:{" + controllerId + "}:" + networkId;
							if (_cc->redisConfig->clusterMode) {
								_cluster->srem(key, memberId);
								_cluster->del("member:{" + controllerId + "}:" + networkId + ":" + memberId);
							}
							else {
								_redis->srem(key, memberId);
								_redis->del("member:{" + controllerId + "}:" + networkId + ":" + memberId);
							}
						}
						catch (sw::redis::Error& e) {
							mspan->SetStatus(opentelemetry::trace::StatusCode::kError, e.what());
							ZTC_LOG("ERROR: Error deleting member from Redis: %s\n", e.what());
						}
					}
				}
				else {
					ZTC_LOG("ERROR: unknown objtype\n");
				}
			}
			catch (std::exception& e) {
				span->SetStatus(opentelemetry::trace::StatusCode::kError, e.what());
				ZTC_LOG("ERROR: Error getting objtype: %s\n", e.what());
				commitFailed = true;
			}
			_pool->unborrow(c);
			c.reset();

			// Re-queue on a failed DB write so the (already-acked) change isn't lost.
			// Done after the connection is returned so we never sleep holding one.
			if (commitFailed) {
				_requeueFailedCommit(qitem);
			}
		}
	}

	ZTC_LOG("commitThread finished\n");
}

void CentralDB::onlineNotificationThread()
{
	waitForReady();
	while (_run == 1) {
		{
			auto provider = opentelemetry::trace::Provider::GetTracerProvider();
			auto tracer = provider->GetTracer("CentralDB");
			auto span = tracer->StartSpan("CentralDB::onlineNotificationThread");
			auto scope = tracer->WithActiveSpan(span);

			try {
				std::unordered_map<std::pair<uint64_t, uint64_t>, NodeOnlineRecord, _PairHasher> lastOnline;
				{
					std::lock_guard<std::mutex> l(_lastOnline_l);
					lastOnline.swap(_lastOnline);
				}

				// Build status updates straight from the controller's in-memory view.  get()
				// below is an authoritative membership check -- it returns false for any
				// network/member this controller doesn't hold -- so there's no need to query
				// Postgres to confirm existence.  This also keeps status flowing to the status
				// writer even while the database is unavailable.
				uint64_t writtenCount = 0;
				for (auto i = lastOnline.begin(); i != lastOnline.end(); ++i) {
					uint64_t nwid_i = i->first.first;
					char nwidTmp[64];
					char memTmp[64];
					char ipTmp[64];
					OSUtils::ztsnprintf(nwidTmp, sizeof(nwidTmp), "%.16llx", nwid_i);
					OSUtils::ztsnprintf(memTmp, sizeof(memTmp), "%.10llx", i->first.second);

					std::string networkId(nwidTmp);
					std::string memberId(memTmp);

					nlohmann::json network, member;
					if (! get(nwid_i, network, i->first.second, member)) {
						continue;	// skip non existent networks/members
					}

					int64_t ts = i->second.lastSeen;
					std::string ipAddr = i->second.physicalAddress.toIpString(ipTmp);
					std::string timestamp = std::to_string(ts);
					std::string osArch = i->second.osArch;
					std::vector<std::string> osArchSplit = split(osArch, '/');
					std::string os = "unknown";
					std::string arch = "unknown";
					std::string frontend = OSUtils::jsonString(network["frontend"], "");

					int vMajor = OSUtils::jsonInt(member["vMajor"], 0);
					int vMinor = OSUtils::jsonInt(member["vMinor"], 0);
					int vRev = OSUtils::jsonInt(member["vRev"], 0);
					std::string version;
					if (vMajor <= 0 && vMinor <= 0 && vRev <= 0) {
						vMajor = 0;
						vMinor = 0;
						vRev = 0;
						version = "unknown";
					}
					else {
						version =
							"v" + std::to_string(vMajor) + "." + std::to_string(vMinor) + "." + std::to_string(vRev);
					}
					if (osArchSplit.size() == 2) {
						os = osArchSplit[0];
						arch = osArchSplit[1];
					}

					_statusWriter->updateNodeStatus(networkId, memberId, os, arch, version, i->second.physicalAddress,
													ts, frontend);
					writtenCount++;
				}
				ZTC_LOG("onlineNotificationThread: %llu entries in lastOnline, %llu passed to status writer\n",
						(unsigned long long)lastOnline.size(), (unsigned long long)writtenCount);
				_statusWriter->writePending();
			}
			catch (std::exception& e) {
				ZTC_LOG("error in onlinenotification thread: %s\n", e.what());
			}
		}

		std::this_thread::sleep_for(std::chrono::seconds(10));
	}
}

nlohmann::json CentralDB::_getNetworkMember(pqxx::work& tx, const std::string networkID, const std::string memberID)
{
	nlohmann::json out;

	try {
		pqxx::row row =
			tx.exec(
				  "SELECT nm.device_id, nm.network_id, nm.authorized, nm.active_bridge, nm.ip_assignments, "
				  "nm.no_auto_assign_ips, "
				  "nm.sso_exempt, (EXTRACT(EPOCH FROM nm.authentication_expiry_time AT TIME ZONE 'UTC')*1000)::bigint, "
				  "(EXTRACT(EPOCH FROM nm.creation_time AT TIME ZONE 'UTC')*1000)::bigint, nm.identity, "
				  "(EXTRACT(EPOCH FROM nm.last_authorized_time AT TIME ZONE 'UTC')*1000)::bigint, "
				  "(EXTRACT(EPOCH FROM nm.last_deauthorized_time AT TIME ZONE 'UTC')*1000)::bigint, "
				  "nm.remote_trace_level, nm.remote_trace_target, nm.revision, nm.capabilities, nm.tags, "
				  "nm.frontend, nm.version_major, nm.version_minor, nm.version_revision, nm.version_protocol "
				  "FROM network_memberships_ctl nm "
				  "INNER JOIN networks_ctl n "
				  "  ON nm.network_id = n.id "
				  "WHERE nm.network_id = $1 AND nm.device_id = $2",
				  pqxx::params { networkID, memberID })
				.one_row();

		bool authorized = row[2].as<bool>();
		std::optional<bool> active_bridge =
			row[3].is_null() ? std::optional<bool>() : std::optional<bool>(row[3].as<bool>());
		std::string ip_assignments = row[4].is_null() ? "{}" : row[4].as<std::string>();
		std::optional<bool> no_auto_assign_ips =
			row[5].is_null() ? std::optional<bool>() : std::optional<bool>(row[5].as<bool>());
		std::optional<bool> sso_exempt =
			row[6].is_null() ? std::optional<bool>() : std::optional<bool>(row[6].as<bool>());
		std::optional<uint64_t> authentication_expiry_time =
			row[7].is_null() ? std::optional<uint64_t>() : std::optional<uint64_t>(row[7].as<uint64_t>());
		std::optional<uint64_t> creation_time =
			row[8].is_null() ? std::optional<uint64_t>() : std::optional<uint64_t>(row[8].as<uint64_t>());
		std::optional<std::string> identity =
			row[9].is_null() ? std::optional<std::string>() : std::optional<std::string>(row[9].as<std::string>());
		std::optional<uint64_t> last_authorized_time =
			row[10].is_null() ? std::optional<uint64_t>() : std::optional<uint64_t>(row[10].as<uint64_t>());
		std::optional<uint64_t> last_deauthorized_time =
			row[11].is_null() ? std::optional<uint64_t>() : std::optional<uint64_t>(row[11].as<uint64_t>());
		std::optional<int32_t> remote_trace_level =
			row[12].is_null() ? std::optional<int32_t>() : std::optional<int32_t>(row[12].as<int32_t>());
		std::optional<std::string> remote_trace_target =
			row[13].is_null() ? std::optional<std::string>() : std::optional<std::string>(row[13].as<std::string>());
		std::optional<uint64_t> revision =
			row[14].is_null() ? std::optional<uint64_t>() : std::optional<uint64_t>(row[14].as<uint64_t>());
		std::optional<std::string> capabilities =
			row[15].is_null() ? std::optional<std::string>() : std::optional<std::string>(row[15].as<std::string>());
		std::optional<std::string> tags =
			row[16].is_null() ? std::optional<std::string>() : std::optional<std::string>(row[16].as<std::string>());
		std::string frontend = row[17].is_null() ? "" : row[17].as<std::string>();
		std::optional<int32_t> version_major =
			row[18].is_null() ? std::optional<int32_t>() : std::optional<int32_t>(row[18].as<int32_t>());
		std::optional<int32_t> version_minor =
			row[19].is_null() ? std::optional<int32_t>() : std::optional<int32_t>(row[19].as<int32_t>());
		std::optional<int32_t> version_revision =
			row[20].is_null() ? std::optional<int32_t>() : std::optional<int32_t>(row[20].as<int32_t>());
		std::optional<int32_t> version_protocol =
			row[21].is_null() ? std::optional<int32_t>() : std::optional<int32_t>(row[21].as<int32_t>());

		out["objtype"] = "member";
		out["id"] = memberID;
		out["nwid"] = networkID;
		out["address"] = memberID;
		out["identity"] = identity.value_or("");
		out["authorized"] = authorized;
		out["activeBridge"] = active_bridge.value_or(false);
		out["ipAssignments"] = json::array();
		if (! ip_assignments.empty() && ip_assignments != "{}" && ip_assignments != "[]") {
			std::string tmp = ip_assignments.substr(1, ip_assignments.length() - 2);
			std::vector<std::string> addrs = split(tmp, ',');
			for (auto it = addrs.begin(); it != addrs.end(); ++it) {
				out["ipAssignments"].push_back(*it);
			}
		}
		out["capabilities"] = json::parse(capabilities.value_or("[]"));
		out["creationTime"] = creation_time.value_or(0);
		out["lastAuthorizedTime"] = last_authorized_time.value_or(0);
		out["lastDeauthorizedTime"] = last_deauthorized_time.value_or(0);
		// Not stored in the members table; emit empty defaults so oldMember matches
		// the shape memberFromJson (PubSubWriter.cpp) reads.
		out["lastAuthorizedCredential"] = "";
		out["lastAuthorizedCredentialType"] = "";
		out["noAutoAssignIps"] = no_auto_assign_ips.value_or(false);
		out["remoteTraceLevel"] = remote_trace_level.value_or(0);
		out["remoteTraceTarget"] = remote_trace_target.value_or(nullptr);
		out["revision"] = revision.value_or(0);
		out["ssoExempt"] = sso_exempt.value_or(false);
		out["authenticationExpiryTime"] = authentication_expiry_time.value_or(0);
		out["tags"] = json::parse(tags.value_or("[]"));
		out["frontend"] = frontend;
		out["vMajor"] = version_major.value_or(-1);
		out["vMinor"] = version_minor.value_or(-1);
		out["vRev"] = version_revision.value_or(-1);
		out["vProto"] = version_protocol.value_or(-1);
	}
	catch (std::exception& e) {
		ZTC_LOG("ERROR: Error getting network member %s-%s: %s\n", networkID.c_str(), memberID.c_str(), e.what());
		return nlohmann::json();
	}

	return out;
}

nlohmann::json CentralDB::_getNetwork(pqxx::work& tx, const std::string networkID)
{
	nlohmann::json out;

	try {
		std::optional<std::string> name;
		std::string cfg;
		std::optional<uint64_t> creation_time;
		std::optional<uint64_t> last_modified;
		std::optional<uint64_t> revision;
		std::string frontend;

		pqxx::row row =
			tx.exec("SELECT id, name, configuration , (EXTRACT(EPOCH FROM creation_time AT TIME ZONE "
					"'UTC')*1000)::bigint, "
					"(EXTRACT(EPOCH FROM last_modified AT TIME ZONE 'UTC')*1000)::bigint, revision, frontend "
					"FROM networks_ctl WHERE id = $1",
					pqxx::params { networkID })
				.one_row();

		cfg = row[2].as<std::string>();
		creation_time = row[3].is_null() ? std::optional<uint64_t>() : std::optional<uint64_t>(row[3].as<uint64_t>());
		last_modified = row[4].is_null() ? std::optional<uint64_t>() : std::optional<uint64_t>(row[4].as<uint64_t>());
		revision = row[5].is_null() ? std::optional<uint64_t>() : std::optional<uint64_t>(row[5].as<uint64_t>());
		frontend = row[6].is_null() ? "" : row[6].as<std::string>();

		nlohmann::json cfgtmp = nlohmann::json::parse(cfg);
		if (! cfgtmp.is_object()) {
			ZTC_LOG("ERROR: Network %s configuration is not a JSON object\n", networkID.c_str());
			return nlohmann::json();
		}

		out["objtype"] = "network";
		out["id"] = row[0].as<std::string>();
		out["name"] = row[1].is_null() ? "" : row[1].as<std::string>();
		out["creationTime"] = creation_time.value_or(0);
		out["lastModified"] = last_modified.value_or(0);
		out["revision"] = revision.value_or(0);
		out["capabilities"] = cfgtmp["capabilities"].is_array() ? cfgtmp["capabilities"] : json::array();
		out["enableBroadcast"] = cfgtmp["enableBroadcast"].is_boolean() ? cfgtmp["enableBroadcast"].get<bool>() : true;
		out["mtu"] = cfgtmp["mtu"].is_number() ? cfgtmp["mtu"].get<int32_t>() : 2800;
		out["multicastLimit"] = cfgtmp["multicastLimit"].is_number() ? cfgtmp["multicastLimit"].get<int32_t>() : 64;
		out["private"] = cfgtmp["private"].is_boolean() ? cfgtmp["private"].get<bool>() : true;
		out["remoteTraceLevel"] =
			cfgtmp["remoteTraceLevel"].is_number() ? cfgtmp["remoteTraceLevel"].get<int32_t>() : 0;
		out["remoteTraceTarget"] =
			cfgtmp["remoteTraceTarget"].is_string() ? cfgtmp["remoteTraceTarget"].get<std::string>() : "";
		out["revision"] = revision.value_or(0);
		out["rules"] = cfgtmp["rules"].is_array() ? cfgtmp["rules"] : json::array();
		out["tags"] = cfgtmp["tags"].is_array() ? cfgtmp["tags"] : json::array();
		if (cfgtmp["v4AssignMode"].is_object()) {
			out["v4AssignMode"] = cfgtmp["v4AssignMode"];
		}
		else {
			out["v4AssignMode"] = json::object();
			out["v4AssignMode"]["zt"] = true;
		}
		if (cfgtmp["v6AssignMode"].is_object()) {
			out["v6AssignMode"] = cfgtmp["v6AssignMode"];
		}
		else {
			out["v6AssignMode"] = json::object();
			out["v6AssignMode"]["zt"] = true;
			out["v6AssignMode"]["6plane"] = true;
			out["v6AssignMode"]["rfc4193"] = false;
		}
		out["ssoEnabled"] = cfgtmp["ssoEnabled"].is_boolean() ? cfgtmp["ssoEnabled"].get<bool>() : false;
		nlohmann::json ssocfg = cfgtmp["ssoConfig"].is_object() ? cfgtmp["ssoConfig"] : nlohmann::json::object();
		ssocfg["ssoClientId"] = ssocfg["ssoClientId"].is_string() ? ssocfg["ssoClientId"].get<std::string>() : "";
		ssocfg["ssoLinkedId"] = ssocfg["ssoLinkedId"].is_string() ? ssocfg["ssoLinkedId"].get<std::string>() : "";
		out["ssoConfig"] = ssocfg;
		out["objtype"] = "network";
		out["routes"] = cfgtmp["routes"].is_array() ? cfgtmp["routes"] : json::array();
		if (! cfgtmp["dns"].is_object()) {
			out["dns"] = json::object();
			out["dns"]["domain"] = "";
			out["dns"]["servers"] = json::array();
		}
		else {
			out["dns"] = cfgtmp["dns"];
		}
		out["ipAssignmentPools"] = cfgtmp["ipAssignmentPools"].is_array() ? cfgtmp["ipAssignmentPools"] : json::array();
		out["frontend"] = row[6].as<std::string>();
	}
	catch (std::exception& e) {
		ZTC_LOG("ERROR: Error getting network %s: %s\n", networkID.c_str(), e.what());
		return nlohmann::json();
	}
	return out;
}

#endif	 // ZT_CONTROLLER_USE_LIBPQ
