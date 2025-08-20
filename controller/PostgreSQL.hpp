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

#ifdef ZT_CONTROLLER_USE_LIBPQ

#ifndef ZT_CONTROLLER_POSTGRESQL_HPP
#define ZT_CONTROLLER_POSTGRESQL_HPP

#include "ConnectionPool.hpp"
#include "DB.hpp"
#include "opentelemetry/trace/provider.h"

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

	std::shared_ptr<pqxx::connection> c;
	int a;
};

class PostgresConnFactory : public ConnectionFactory {
  public:
	PostgresConnFactory(std::string& connString) : m_connString(connString)
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
	MemberNotificationReceiver(T* p, pqxx::connection& c, const std::string& channel) : pqxx::notification_receiver(c, channel), _psql(p)
	{
		fprintf(stderr, "initialize MemberNotificationReceiver\n");
	}

	virtual ~MemberNotificationReceiver()
	{
		fprintf(stderr, "MemberNotificationReceiver destroyed\n");
	}

	virtual void operator()(const std::string& payload, int backendPid)
	{
		auto provider = opentelemetry::trace::Provider::GetTracerProvider();
		auto tracer = provider->GetTracer("db_member_notification");
		auto span = tracer->StartSpan("db_member_notification::operator()");
		auto scope = tracer->WithActiveSpan(span);
		span->SetAttribute("payload", payload);
		span->SetAttribute("psqlReady", _psql->isReady());

		fprintf(stderr, "Member Notification received: %s\n", payload.c_str());
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
			fprintf(stderr, "payload sent\n");
		}
		else if (newConfig.is_object() && ! oldConfig.is_object()) {
			// new member
			Metrics::member_count++;
			_psql->save(newConfig, _psql->isReady());
			fprintf(stderr, "new member payload sent\n");
		}
		else if (! newConfig.is_object() && oldConfig.is_object()) {
			// member delete
			uint64_t networkId = OSUtils::jsonIntHex(oldConfig["nwid"], 0ULL);
			uint64_t memberId = OSUtils::jsonIntHex(oldConfig["id"], 0ULL);
			if (memberId && networkId) {
				_psql->eraseMember(networkId, memberId);
				fprintf(stderr, "member delete payload sent\n");
			}
		}
	}

  private:
	T* _psql;
};

template <typename T> class NetworkNotificationReceiver : public pqxx::notification_receiver {
  public:
	NetworkNotificationReceiver(T* p, pqxx::connection& c, const std::string& channel) : pqxx::notification_receiver(c, channel), _psql(p)
	{
		fprintf(stderr, "initialize NetworkrNotificationReceiver\n");
	}

	virtual ~NetworkNotificationReceiver()
	{
		fprintf(stderr, "NetworkNotificationReceiver destroyed\n");
	};

	virtual void operator()(const std::string& payload, int packend_pid)
	{
		auto provider = opentelemetry::trace::Provider::GetTracerProvider();
		auto tracer = provider->GetTracer("db_network_notification");
		auto span = tracer->StartSpan("db_network_notification::operator()");
		auto scope = tracer->WithActiveSpan(span);
		span->SetAttribute("payload", payload);
		span->SetAttribute("psqlReady", _psql->isReady());

		fprintf(stderr, "Network Notification received: %s\n", payload.c_str());
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
			fprintf(stderr, "payload sent\n");
		}
		else if (newConfig.is_object() && ! oldConfig.is_object()) {
			std::string nwid = newConfig["id"];
			span->SetAttribute("network_id", nwid);
			span->SetAttribute("action", "new_network");
			// new network
			_psql->save(newConfig, _psql->isReady());
			fprintf(stderr, "new network payload sent\n");
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
				fprintf(stderr, "network delete payload sent\n");
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
};

}	// namespace ZeroTier

#endif	 // ZT_CONTROLLER_POSTGRESQL_HPP

#endif	 // ZT_CONTROLLER_USE_LIBPQ