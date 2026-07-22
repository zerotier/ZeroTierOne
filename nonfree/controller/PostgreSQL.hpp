/* (c) ZeroTier, Inc.
 * See LICENSE.txt in nonfree/
 */

#ifdef ZT_CONTROLLER_USE_LIBPQ

#ifndef ZT_CONTROLLER_POSTGRESQL_HPP
#define ZT_CONTROLLER_POSTGRESQL_HPP

#include "ConnectionPool.hpp"
#include "CtlUtil.hpp"
#include "DB.hpp"
#include "NotificationListener.hpp"
#include "opentelemetry/trace/provider.h"

#include <atomic>
#include <memory>
#include <nlohmann/json.hpp>
#include <pqxx/pqxx>

namespace ZeroTier {

extern "C" {
typedef struct pg_conn PGconn;
}

class PostgresConnection : public Connection {
  public:
	virtual ~PostgresConnection()
	{
	}

	// A pqxx::connection whose backend has gone away (server restart, AlloyDB
	// maintenance, network drop) reports is_open() == false and cannot be reused
	// -- pqxx 7 has no reconnect.  Returning false here makes the pool drop it.
	virtual bool alive() const override
	{
		return c && c->is_open();
	}

	std::shared_ptr<pqxx::connection> c;
	int a;
};

// Add client-side liveness/timeout parameters to a libpq connection string unless
// the operator already set them. Without these a connection whose peer silently
// vanishes (AlloyDB failover, network partition) blocks its thread in a socket read
// indefinitely; with them the call fails within a bounded time and the normal
// error/retry paths take over. Handles both key/value conninfo and postgres:// URIs.
inline std::string hardenPostgresConnString(std::string cs)
{
	const bool isUri = (cs.rfind("postgres://", 0) == 0) || (cs.rfind("postgresql://", 0) == 0);
	auto add = [&cs, isUri](const char* key, const char* kvForm, const char* uriForm) {
		if (cs.find(key) != std::string::npos)
			return;
		if (isUri) {
			cs += (cs.find('?') == std::string::npos) ? "?" : "&";
			cs += uriForm;
		}
		else {
			cs += " ";
			cs += kvForm;
		}
	};
	add("connect_timeout", "connect_timeout=10", "connect_timeout=10");
	add("keepalives", "keepalives=1 keepalives_idle=30 keepalives_interval=10 keepalives_count=3",
		"keepalives=1&keepalives_idle=30&keepalives_interval=10&keepalives_count=3");
	// Bounds transmit-side hangs (peer never ACKs) at the TCP layer; milliseconds.
	add("tcp_user_timeout", "tcp_user_timeout=30000", "tcp_user_timeout=30000");
	// Server-side per-statement ceiling; generous enough for the initialize* bulk loads.
	add("statement_timeout", "options='-c statement_timeout=120000'",
		"options=-c%20statement_timeout%3D120000");
	return cs;
}

class PostgresConnFactory : public ConnectionFactory {
  public:
	PostgresConnFactory(std::string& connString) : m_connString(hardenPostgresConnString(connString))
	{
	}

	virtual std::shared_ptr<Connection> create()
	{
		Metrics::conn_counter++;
		auto c = std::shared_ptr<PostgresConnection>(new PostgresConnection());
		c->c = std::make_shared<pqxx::connection>(m_connString);
		return std::static_pointer_cast<Connection>(c);
	}

  private:
	std::string m_connString;
};

template <typename T> class MemberNotificationReceiver : public pqxx::notification_receiver {
  public:
	MemberNotificationReceiver(T* p, pqxx::connection& c, const std::string& channel)
		: pqxx::notification_receiver(c, channel)
		, _psql(p)
	{ ZTC_LOG("initialize MemberNotificationReceiver\n"); }

	virtual ~MemberNotificationReceiver()
	{ ZTC_LOG("MemberNotificationReceiver destroyed\n"); }

	virtual void operator()(const std::string& payload, int backendPid)
	{
		auto provider = opentelemetry::trace::Provider::GetTracerProvider();
		auto tracer = provider->GetTracer("db_member_notification");
		auto span = tracer->StartSpan("db_member_notification::operator()");
		auto scope = tracer->WithActiveSpan(span);
		span->SetAttribute("payload", payload);
		span->SetAttribute("psqlReady", _psql->isReady());

		ZTC_LOG("Member Notification received: %s\n", payload.c_str());
		Metrics::pgsql_mem_notification++;
		nlohmann::json tmp(nlohmann::json::parse(payload));
		nlohmann::json& ov = tmp["old_val"];
		nlohmann::json& nv = tmp["new_val"];
		nlohmann::json oldConfig, newConfig;
		if (ov.is_object())
			oldConfig = ov;
		if (nv.is_object())
			newConfig = nv;

		if (oldConfig.is_object() && newConfig.is_object()) {
			_psql->save(newConfig, _psql->isReady());
			ZTC_LOG("payload sent\n");
		}
		else if (newConfig.is_object() && ! oldConfig.is_object()) {
			// new member
			Metrics::member_count++;
			_psql->save(newConfig, _psql->isReady());
			ZTC_LOG("new member payload sent\n");
		}
		else if (! newConfig.is_object() && oldConfig.is_object()) {
			// member delete
			uint64_t networkId = OSUtils::jsonIntHex(oldConfig["nwid"], 0ULL);
			uint64_t memberId = OSUtils::jsonIntHex(oldConfig["id"], 0ULL);
			if (memberId && networkId) {
				_psql->eraseMember(networkId, memberId);
				ZTC_LOG("member delete payload sent\n");
			}
		}
	}

  private:
	T* _psql;
};

template <typename T> class NetworkNotificationReceiver : public pqxx::notification_receiver {
  public:
	NetworkNotificationReceiver(T* p, pqxx::connection& c, const std::string& channel)
		: pqxx::notification_receiver(c, channel)
		, _psql(p)
	{ ZTC_LOG("initialize NetworkrNotificationReceiver\n"); }

	virtual ~NetworkNotificationReceiver()
	{ ZTC_LOG("NetworkNotificationReceiver destroyed\n"); };

	virtual void operator()(const std::string& payload, int packend_pid)
	{
		auto provider = opentelemetry::trace::Provider::GetTracerProvider();
		auto tracer = provider->GetTracer("db_network_notification");
		auto span = tracer->StartSpan("db_network_notification::operator()");
		auto scope = tracer->WithActiveSpan(span);
		span->SetAttribute("payload", payload);
		span->SetAttribute("psqlReady", _psql->isReady());

		ZTC_LOG("Network Notification received: %s\n", payload.c_str());
		Metrics::pgsql_net_notification++;
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
			_psql->save(newConfig, _psql->isReady());
			ZTC_LOG("payload sent\n");
		}
		else if (newConfig.is_object() && ! oldConfig.is_object()) {
			std::string nwid = newConfig["id"];
			span->SetAttribute("network_id", nwid);
			span->SetAttribute("action", "new_network");
			// new network
			_psql->save(newConfig, _psql->isReady());
			ZTC_LOG("new network payload sent\n");
		}
		else if (! newConfig.is_object() && oldConfig.is_object()) {
			// network delete
			span->SetAttribute("action", "delete_network");
			std::string nwid = oldConfig["id"];
			span->SetAttribute("network_id", nwid);
			uint64_t networkId = Utils::hexStrToU64(nwid.c_str());
			span->SetAttribute("network_id_int", networkId);
			if (networkId) {
				_psql->eraseNetwork(networkId);
				ZTC_LOG("network delete payload sent\n");
			}
		}
	}

  private:
	T* _psql;
};

struct NodeOnlineRecord {
	uint64_t lastSeen;
	InetAddress physicalAddress;
	std::string osArch;
	std::string version;
};

/**
 * internal class for listening to PostgreSQL notification channels.
 */
template <typename T> class _notificationReceiver : public pqxx::notification_receiver {
  public:
	_notificationReceiver(T* p, pqxx::connection& c, const std::string& channel)
		: pqxx::notification_receiver(c, channel)
		, _listener(p)
	{ ZTC_LOG("initialize PostgresMemberNotificationListener::_notificationReceiver\n"); }

	virtual void operator()(const std::string& payload, int backendPid)
	{
		auto provider = opentelemetry::trace::Provider::GetTracerProvider();
		auto tracer = provider->GetTracer("notification_receiver");
		auto span = tracer->StartSpan("notification_receiver::operator()");
		auto scope = tracer->WithActiveSpan(span);
		_listener->onNotification(payload);
	}

  private:
	T* _listener;
};

class PostgresMemberListener : public NotificationListener {
  public:
	PostgresMemberListener(
		DB* db,
		std::shared_ptr<ConnectionPool<PostgresConnection> > pool,
		const std::string& channel,
		uint64_t timeout);
	virtual ~PostgresMemberListener();

	virtual void listen();

	virtual NotificationResult onNotification(const std::string& payload) override;

	// Stop and join the listen thread so no further onNotification callbacks fire.
	// Idempotent; the destructor also calls it as a safety net.
	void stop() override;

  private:
	// Drop the dead connection/receiver and rebind to a freshly borrowed one.
	void reconnect();

	std::atomic<bool> _run { false };
	DB* _db;
	std::shared_ptr<ConnectionPool<PostgresConnection> > _pool;
	std::shared_ptr<PostgresConnection> _conn;
	uint64_t _notification_timeout;
	std::thread _listenerThread;
	std::string _channel;
	_notificationReceiver<PostgresMemberListener>* _receiver = nullptr;
};

class PostgresNetworkListener : public NotificationListener {
  public:
	PostgresNetworkListener(
		DB* db,
		std::shared_ptr<ConnectionPool<PostgresConnection> > pool,
		const std::string& channel,
		uint64_t timeout);
	virtual ~PostgresNetworkListener();

	virtual void listen();

	virtual NotificationResult onNotification(const std::string& payload) override;

	// Stop and join the listen thread so no further onNotification callbacks fire.
	// Idempotent; the destructor also calls it as a safety net.
	void stop() override;

  private:
	// Drop the dead connection/receiver and rebind to a freshly borrowed one.
	void reconnect();

	std::atomic<bool> _run { false };
	DB* _db;
	std::shared_ptr<ConnectionPool<PostgresConnection> > _pool;
	std::shared_ptr<PostgresConnection> _conn;
	uint64_t _notification_timeout;
	std::thread _listenerThread;
	std::string _channel;
	_notificationReceiver<PostgresNetworkListener>* _receiver = nullptr;
};

}	// namespace ZeroTier

#endif	 // ZT_CONTROLLER_POSTGRESQL_HPP

#endif	 // ZT_CONTROLLER_USE_LIBPQ
