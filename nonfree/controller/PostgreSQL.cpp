/* (c) ZeroTier, Inc.
 * See LICENSE.txt in nonfree/
 */

#ifdef ZT_CONTROLLER_USE_LIBPQ

#include "PostgreSQL.hpp"

#include "CtlUtil.hpp"
#include "opentelemetry/trace/provider.h"

#include <chrono>
#include <cstdio>
#include <nlohmann/json.hpp>
#include <thread>

namespace ZeroTier {

PostgresMemberListener::PostgresMemberListener(DB* db,
											   std::shared_ptr<ConnectionPool<PostgresConnection> > pool,
											   const std::string& channel,
											   uint64_t timeout)
	: NotificationListener()
	, _db(db)
	, _pool(pool)
	, _notification_timeout(timeout)
	, _listenerThread()
	, _channel(channel)
{
	_conn = _pool->borrow();
	try {
		_receiver = new _notificationReceiver<PostgresMemberListener>(this, *_conn->c, _channel);
		_run = true;
		_listenerThread = std::thread(&PostgresMemberListener::listen, this);
	}
	catch (...) {
		// Roll back partial construction so the borrowed connection (and receiver) aren't leaked.
		_run = false;
		delete _receiver;
		_receiver = nullptr;
		if (_conn) {
			_pool->unborrow(_conn);
			_conn.reset();
		}
		throw;
	}
}

PostgresMemberListener::~PostgresMemberListener()
{
	stop();
	delete _receiver;
	if (_conn) {
		_pool->unborrow(_conn);
		_conn.reset();
	}
}

void PostgresMemberListener::stop()
{
	_run = false;
	if (_listenerThread.joinable()) {
		_listenerThread.join();
	}
}

void PostgresMemberListener::reconnect()
{
	// Destroy the receiver before releasing the connection it references, then rebind
	// to a freshly borrowed (live) connection. Called when the backend connection has
	// gone away (server restart, AlloyDB maintenance, network drop).
	delete _receiver;
	_receiver = nullptr;
	if (_conn) {
		_pool->unborrow(_conn);	  // a dead connection is discarded by the pool (alive() == false)
		_conn.reset();
	}
	_conn = _pool->borrow();
	_receiver = new _notificationReceiver<PostgresMemberListener>(this, *_conn->c, _channel);
}

void PostgresMemberListener::listen()
{
	setCurrentThreadName("ctl-pg-member");
	while (_run) {
		// await_notification dispatches into onNotification, which can throw on a
		// malformed payload; libpqxx propagates that out here. An exception escaping
		// this thread would call std::terminate and kill the whole controller, so we
		// must catch everything. A broken connection (or a failed reconnect) also lands
		// in the catch; we drop the dead connection so the next iteration re-borrows a
		// fresh one, backing off briefly to avoid a hot loop while the DB is unreachable.
		try {
			if (! _conn || ! _conn->alive()) {
				ZTC_LOG("PostgresMemberListener: (re)connecting listener on channel %s\n", _channel.c_str());
				reconnect();
			}
			_conn->c->await_notification(_notification_timeout, 0);
		}
		catch (const std::exception& e) {
			ZTC_LOG("ERROR: exception in member notification listener: %s\n", e.what());
			delete _receiver;
			_receiver = nullptr;
			if (_conn) {
				_pool->unborrow(_conn);
				_conn.reset();
			}
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	}
}

NotificationResult PostgresMemberListener::onNotification(const std::string& payload)
{
	auto provider = opentelemetry::trace::Provider::GetTracerProvider();
	auto tracer = provider->GetTracer("PostgresMemberNotificationListener");
	auto span = tracer->StartSpan("PostgresMemberNotificationListener::onNotification");
	auto scope = tracer->WithActiveSpan(span);
	span->SetAttribute("payload", payload);

	Metrics::pgsql_mem_notification++;
	try {
		nlohmann::json tmp(nlohmann::json::parse(payload));
		nlohmann::json& ov = tmp["old_val"];
		nlohmann::json& nv = tmp["new_val"];
		nlohmann::json oldConfig, newConfig;
		if (ov.is_object())
			oldConfig = ov;
		if (nv.is_object())
			newConfig = nv;

		if (oldConfig.is_object() && newConfig.is_object()) {
			_db->save(newConfig, true);
		}
		else if (newConfig.is_object() && ! oldConfig.is_object()) {
			// new member
			Metrics::member_count++;
			_db->save(newConfig, true);
		}
		else if (! newConfig.is_object() && oldConfig.is_object()) {
			// member delete
			uint64_t networkId = OSUtils::jsonIntHex(oldConfig["nwid"], 0ULL);
			uint64_t memberId = OSUtils::jsonIntHex(oldConfig["id"], 0ULL);
			if (memberId && networkId) {
				_db->eraseMember(networkId, memberId);
			}
		}
	}
	catch (const std::exception& e) {
		span->SetStatus(opentelemetry::trace::StatusCode::kError, e.what());
		ZTC_LOG("ERROR: exception handling member notification: %s\n", e.what());
		return NotificationResult::TransientFailure;
	}
	return NotificationResult::Ok;
}

PostgresNetworkListener::PostgresNetworkListener(DB* db,
												 std::shared_ptr<ConnectionPool<PostgresConnection> > pool,
												 const std::string& channel,
												 uint64_t timeout)
	: NotificationListener()
	, _db(db)
	, _pool(pool)
	, _notification_timeout(timeout)
	, _listenerThread()
	, _channel(channel)
{
	_conn = _pool->borrow();
	try {
		_receiver = new _notificationReceiver<PostgresNetworkListener>(this, *_conn->c, _channel);
		_run = true;
		_listenerThread = std::thread(&PostgresNetworkListener::listen, this);
	}
	catch (...) {
		// Roll back partial construction so the borrowed connection (and receiver) aren't leaked.
		_run = false;
		delete _receiver;
		_receiver = nullptr;
		if (_conn) {
			_pool->unborrow(_conn);
			_conn.reset();
		}
		throw;
	}
}

PostgresNetworkListener::~PostgresNetworkListener()
{
	stop();
	delete _receiver;
	if (_conn) {
		_pool->unborrow(_conn);
		_conn.reset();
	}
}

void PostgresNetworkListener::stop()
{
	_run = false;
	if (_listenerThread.joinable()) {
		_listenerThread.join();
	}
}

void PostgresNetworkListener::reconnect()
{
	// See PostgresMemberListener::reconnect.
	delete _receiver;
	_receiver = nullptr;
	if (_conn) {
		_pool->unborrow(_conn);
		_conn.reset();
	}
	_conn = _pool->borrow();
	_receiver = new _notificationReceiver<PostgresNetworkListener>(this, *_conn->c, _channel);
}

void PostgresNetworkListener::listen()
{
	setCurrentThreadName("ctl-pg-network");
	while (_run) {
		// See PostgresMemberListener::listen — an exception escaping this thread would
		// terminate the controller, so everything must be caught here; a broken
		// connection is dropped and re-borrowed on the next iteration.
		try {
			if (! _conn || ! _conn->alive()) {
				ZTC_LOG("PostgresNetworkListener: (re)connecting listener on channel %s\n", _channel.c_str());
				reconnect();
			}
			_conn->c->await_notification(_notification_timeout, 0);
		}
		catch (const std::exception& e) {
			ZTC_LOG("ERROR: exception in network notification listener: %s\n", e.what());
			delete _receiver;
			_receiver = nullptr;
			if (_conn) {
				_pool->unborrow(_conn);
				_conn.reset();
			}
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	}
}

NotificationResult PostgresNetworkListener::onNotification(const std::string& payload)
{
	auto provider = opentelemetry::trace::Provider::GetTracerProvider();
	auto tracer = provider->GetTracer("db_network_notification");
	auto span = tracer->StartSpan("db_network_notification::operator()");
	auto scope = tracer->WithActiveSpan(span);
	span->SetAttribute("payload", payload);

	Metrics::pgsql_net_notification++;
	try {
		nlohmann::json tmp(nlohmann::json::parse(payload));

		nlohmann::json& ov = tmp["old_val"];
		nlohmann::json& nv = tmp["new_val"];
		nlohmann::json oldConfig, newConfig;

		if (ov.is_object())
			oldConfig = ov;
		if (nv.is_object())
			newConfig = nv;

		if (oldConfig.is_object() && newConfig.is_object()) {
			std::string nwid = oldConfig["id"];
			span->SetAttribute("action", "network_change");
			span->SetAttribute("network_id", nwid);
			_db->save(newConfig, true);
		}
		else if (newConfig.is_object() && ! oldConfig.is_object()) {
			std::string nwid = newConfig["id"];
			span->SetAttribute("network_id", nwid);
			span->SetAttribute("action", "new_network");
			// new network
			_db->save(newConfig, true);
		}
		else if (! newConfig.is_object() && oldConfig.is_object()) {
			// network delete
			span->SetAttribute("action", "delete_network");
			std::string nwid = oldConfig["id"];
			span->SetAttribute("network_id", nwid);
			uint64_t networkId = Utils::hexStrToU64(nwid.c_str());
			span->SetAttribute("network_id_int", networkId);
			if (networkId) {
				_db->eraseNetwork(networkId);
			}
		}
	}
	catch (const std::exception& e) {
		span->SetStatus(opentelemetry::trace::StatusCode::kError, e.what());
		ZTC_LOG("ERROR: exception handling network notification: %s\n", e.what());
		return NotificationResult::TransientFailure;
	}
	return NotificationResult::Ok;
}

}	// namespace ZeroTier

#endif
