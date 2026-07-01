/* (c) ZeroTier, Inc.
 * See LICENSE.txt in nonfree/
 */

#ifdef ZT_CONTROLLER_USE_LIBPQ
#include "PubSubListener.hpp"

#include "ControllerConfig.hpp"
#include "CtlUtil.hpp"
#include "DB.hpp"
#include "OtelCarrier.hpp"
#include "member.pb.h"
#include "network.pb.h"
#include "sso.pb.h"
#include "opentelemetry/context/propagation/global_propagator.h"
#include "opentelemetry/trace/propagation/http_trace_context.h"
#include "opentelemetry/trace/provider.h"
#include "opentelemetry/trace/tracer.h"
#include "rustybits.h"

#include <google/cloud/opentelemetry_options.h>
#include <google/cloud/pubsub/admin/subscription_admin_client.h>
#include <google/cloud/pubsub/admin/subscription_admin_connection.h>
#include <google/cloud/pubsub/admin/topic_admin_client.h>
#include <google/cloud/pubsub/message.h>
#include <google/cloud/pubsub/options.h>
#include <google/cloud/pubsub/subscriber.h>
#include <google/cloud/pubsub/subscription.h>
#include <google/cloud/pubsub/topic.h>
#include <nlohmann/json.hpp>

namespace pubsub = ::google::cloud::pubsub;
namespace pubsub_admin = ::google::cloud::pubsub_admin;

namespace ZeroTier {

nlohmann::json toJson(const pbmessages::NetworkChange_Network& nc, pbmessages::NetworkChange_ChangeSource source);
nlohmann::json toJson(const pbmessages::MemberChange_Member& mc, pbmessages::MemberChange_ChangeSource source);

namespace {
// Defers a PubSub message's ack/nack until its DB commit completes. Created in the subscriber
// callback holding the moved-in AckHandler, handed off (as a shared_ptr<NotificationCompletion>)
// onto the commit-queue item, and completed by the commit thread after the write -- so an
// un-committed change is redelivered rather than acked-then-lost. Safe to complete exactly once
// from whichever thread finishes first.
class PubSubAckCompletion : public NotificationCompletion {
  public:
	explicit PubSubAckCompletion(pubsub::AckHandler handler) : _handler(std::move(handler))
	{
	}
	void complete(NotificationResult result) override
	{
		if (_done.test_and_set()) {
			return;	  // ack/nack exactly once
		}
		if (result == NotificationResult::TransientFailure) {
			std::move(_handler).nack();	  // retryable -> redeliver
		}
		else {
			std::move(_handler).ack();	 // Ok / PermanentFailure -> ack (drop on permanent)
		}
	}

  private:
	pubsub::AckHandler _handler;
	std::atomic_flag _done = ATOMIC_FLAG_INIT;
};
}	// namespace

PubSubListener::PubSubListener(std::string controller_id, std::string project, std::string topic)
	: _controller_id(controller_id)
	, _project(project)
	, _topic(topic)
	, _subscription_id()
	, _run(false)
	, _adminClient(pubsub_admin::MakeSubscriptionAdminConnection())
	, _subscription(nullptr)
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	_subscription_id = "sub-" + controller_id + "-" + topic;   // + "-" + random_hex_string(8);
	_subscription = new pubsub::Subscription(_project, _subscription_id);
	fprintf(
		stderr, "PubSubListener for controller %s project %s topic %s subscription %s\n", controller_id.c_str(),
		project.c_str(), topic.c_str(), _subscription_id.c_str());

	// If PUBSUB_EMULATOR_HOST is set, create the topic if it doesn't exist
	const char* emulatorHost = std::getenv("PUBSUB_EMULATOR_HOST");
	if (emulatorHost != nullptr) {
		create_gcp_pubsub_topic_if_needed(project, topic);
		create_gcp_pubsub_subscription_if_needed(_project, _subscription_id, _topic, _controller_id);
	}

	_subscriber = std::make_shared<pubsub::Subscriber>(pubsub::MakeSubscriberConnection(
		*_subscription,
		google::cloud::Options {}
			.set<google::cloud::OpenTelemetryTracingOption>(true)
			// Cap the client-side flow-control window. google-cloud-cpp defaults to 1000
			// outstanding messages / 100 MiB per subscriber; with three controller
			// subscribers that is up to ~300 MiB of un-acked messages buffered in-process
			// during a change burst. A smaller window bounds that, and together with
			// ack-after-commit it also bounds how many uncommitted changes can pile up.
			// Flow-control options belong on MakeSubscriberConnection (the SubscriberOptionList
			// consumer), not on the Subscriber constructor.
			.set<pubsub::MaxOutstandingMessagesOption>(200)
			.set<pubsub::MaxOutstandingBytesOption>(16 * 1024 * 1024)));

	_run = true;
	_subscriberThread = std::thread(&PubSubListener::subscribe, this);
}

void PubSubListener::stop()
{
	_run = false;
	{
		std::lock_guard<std::mutex> lock(_sessionMutex);
		if (_hasSession) {
			_session.cancel();
		}
	}
	if (_subscriberThread.joinable()) {
		_subscriberThread.join();
	}
}

PubSubListener::~PubSubListener()
{
	// Safety net: each derived destructor should already have called stop() so the
	// subscriber thread is joined while the derived object is still intact. This is
	// idempotent (thread already joined, _hasSession already cleared).
	stop();

	if (_subscription) {
		delete _subscription;
		_subscription = nullptr;
	}
}

void PubSubListener::subscribe()
{
	while (_run) {
		try {
			ZTC_LOG("PubSubListener::subscribe: starting session for subscription %s\n", _subscription_id.c_str());
			_lastMessageTime.store(std::chrono::steady_clock::now());

			auto session = _subscriber->Subscribe([this](pubsub::Message const& m, pubsub::AckHandler h) {
				_lastMessageTime.store(std::chrono::steady_clock::now());
				// Defer the ack until the change is durably committed: hand ownership of the
				// AckHandler to a completion that the DB's commit thread resolves. If the write is
				// a no-op, never reaches the commit queue, or is handled synchronously (SSO), we
				// complete it ourselves below with onNotification's reported outcome.
				setPendingCompletion(std::make_shared<PubSubAckCompletion>(std::move(h)));
				// Default to transient: if anything throws before onNotification reports an
				// outcome, redeliver rather than silently dropping the change.
				NotificationResult result = NotificationResult::TransientFailure;
				try {
					auto provider = opentelemetry::trace::Provider::GetTracerProvider();
					auto tracer = provider->GetTracer("PubSubListener");

					auto propagator =
						opentelemetry::context::propagation::GlobalTextMapPropagator::GetGlobalPropagator();
					std::map<std::string, std::string> attrs_map;
					for (auto const& kv : m.attributes()) {
						attrs_map.emplace(kv.first, kv.second);
					}

					OtelCarrier<std::map<std::string, std::string> > carrier(attrs_map);

					auto current_ctx = opentelemetry::context::RuntimeContext::GetCurrent();
					auto new_context = propagator->Extract(carrier, current_ctx);
					auto remote_span = opentelemetry::trace::GetSpan(new_context);
					auto remote_scope = tracer->WithActiveSpan(remote_span);

					{
						auto span = tracer->StartSpan("PubSubListener::onMessage");
						auto scope = tracer->WithActiveSpan(span);
						span->SetAttribute("message_id", m.message_id());
						span->SetAttribute("ordering_key", m.ordering_key());

						result = onNotification(m.data());
						switch (result) {
							case NotificationResult::Ok:
								span->SetStatus(opentelemetry::trace::StatusCode::kOk);
								break;
							case NotificationResult::PermanentFailure:
								ZTC_LOG("PubSubListener: permanent failure for message %s (ordering_key=%s); acking to "
										"drop\n",
										m.message_id().c_str(), m.ordering_key().c_str());
								span->SetStatus(opentelemetry::trace::StatusCode::kError,
												"permanent failure; dropping");
								break;
							case NotificationResult::TransientFailure:
								ZTC_LOG("PubSubListener: transient failure for message %s (ordering_key=%s); nacking "
										"for redelivery\n",
										m.message_id().c_str(), m.ordering_key().c_str());
								span->SetStatus(opentelemetry::trace::StatusCode::kError,
												"transient failure; redeliver");
								break;
						}
					}
				}
				catch (const std::exception& e) {
					// An error escaped onNotification's own handling — treat as transient and redeliver.
					ZTC_LOG("PubSubListener callback exception: %s (subscription=%s message_id=%s); nacking\n",
							e.what(), _subscription_id.c_str(), m.message_id().c_str());
					result = NotificationResult::TransientFailure;
				}
				catch (...) {
					// Truly unknown (non-std) error: drop to avoid an infinite poison-redelivery loop.
					fprintf(
						stderr,
						"PubSubListener callback unknown exception (subscription=%s message_id=%s); acking to drop\n",
						_subscription_id.c_str(), m.message_id().c_str());
					result = NotificationResult::PermanentFailure;
				}

				// If onNotification enqueued async DB work it took the pending completion (the
				// commit thread acks/nacks after the write). Otherwise -- a no-op, a
				// parse/validation failure, or a synchronous handler -- complete it now with the
				// outcome onNotification reported.
				if (auto completion = takePendingCompletion()) {
					completion->complete(result);
				}
				return true;
			});

			{
				std::lock_guard<std::mutex> lock(_sessionMutex);
				_session = std::move(session);
				_hasSession = true;
			}

			// Poll the session with a timeout. If no messages have been
			// received for 60 seconds, assume the pull stream is frozen
			// and cancel so we can reconnect.  This avoids the old
			// 10-second blind cancel that raced with in-flight acks.
			while (_run) {
				auto result = _session.wait_for(std::chrono::seconds(15));
				if (result == std::future_status::ready) {
					break;	// session ended naturally
				}
				auto idle = std::chrono::steady_clock::now() - _lastMessageTime.load();
				if (idle > std::chrono::seconds(60)) {
					ZTC_LOG("PubSubListener: no messages for 60s on %s, reconnecting\n", _subscription_id.c_str());
					_session.cancel();
					break;
				}
			}

			{
				std::lock_guard<std::mutex> lock(_sessionMutex);
				_hasSession = false;
			}

			// Retrieve the session status (blocks until cancel or natural end completes)
			try {
				auto status = _session.get();
				if (! status.ok()) {
					ZTC_LOG("Subscription session ended: %s\n", status.message().c_str());
				}
			}
			catch (...) {
				ZTC_LOG("Subscription session ended with exception on %s\n", _subscription_id.c_str());
			}
		}
		catch (google::cloud::Status const& status) {
			ZTC_LOG("Subscription terminated with status: %s\n", status.message().c_str());
		}
	}
}

PubSubNetworkListener::PubSubNetworkListener(std::string controller_id, std::string project, std::string topic, DB* db)
	: PubSubListener(controller_id, project, topic)
	, _db(db)
{
}

PubSubNetworkListener::~PubSubNetworkListener()
{
	stop();	  // join the subscriber thread before this object's members are destroyed
}

NotificationResult PubSubNetworkListener::onNotification(const std::string& payload)
{
	auto provider = opentelemetry::trace::Provider::GetTracerProvider();
	auto tracer = provider->GetTracer("PubSubNetworkListener");
	auto span = tracer->StartSpan("PubSubNetworkListener::onNotification");
	auto scope = tracer->WithActiveSpan(span);

	pbmessages::NetworkChange nc;
	if (! nc.ParseFromString(payload)) {
		ZTC_LOG("Failed to parse NetworkChange protobuf message\n");
		span->SetAttribute("error", "Failed to parse NetworkChange protobuf message");
		span->SetStatus(opentelemetry::trace::StatusCode::kError, "Failed to parse protobuf");
		return NotificationResult::PermanentFailure;
	}

	try {
		nlohmann::json oldConfig, newConfig;

		if (nc.has_old()) {
			ZTC_LOG("has old network config\n");
			oldConfig = toJson(nc.old(), nc.change_source());
		}

		if (nc.has_new_()) {
			ZTC_LOG("has new network config\n");
			newConfig = toJson(nc.new_(), nc.change_source());
		}

		if (! nc.has_old() && ! nc.has_new_()) {
			ZTC_LOG("NetworkChange message has no old or new network config\n");
			span->SetAttribute("error", "NetworkChange message has no old or new network config");
			span->SetStatus(opentelemetry::trace::StatusCode::kError, "No old or new config");
			return NotificationResult::PermanentFailure;
		}

		if (oldConfig.is_object() && newConfig.is_object()) {
			// network modification
			std::string nwid = oldConfig["id"].get<std::string>();
			span->SetAttribute("action", "network_change");
			span->SetAttribute("network_id", nwid);
			_db->save(newConfig, _db->isReady());
		}
		else if (newConfig.is_object() && ! oldConfig.is_object()) {
			// new network
			std::string nwid = newConfig["id"];
			span->SetAttribute("network_id", nwid);
			span->SetAttribute("action", "new_network");
			_db->save(newConfig, _db->isReady());
		}
		else if (! newConfig.is_object() && oldConfig.is_object()) {
			// network deletion
			std::string nwid = oldConfig["id"];
			span->SetAttribute("action", "delete_network");
			span->SetAttribute("network_id", nwid);

			uint64_t networkId = Utils::hexStrToU64(nwid.c_str());
			if (networkId) {
				_db->eraseNetwork(networkId);
			}
		}
	}
	catch (const nlohmann::json::exception& e) {
		// Malformed/unexpected message contents (parse or type error) — unprocessable, drop it.
		ZTC_LOG("PubSubNetworkListener JSON error: %s\n", e.what());
		span->SetAttribute("error", e.what());
		span->SetStatus(opentelemetry::trace::StatusCode::kError, e.what());
		ZTC_LOG("payload: %s\n", payload.c_str());
		return NotificationResult::PermanentFailure;
	}
	catch (const std::exception& e) {
		// Most likely a DB/runtime error from _db->save/eraseNetwork — retryable.
		ZTC_LOG("PubSubNetworkListener Exception in PubSubNetworkListener: %s\n", e.what());
		span->SetAttribute("error", e.what());
		span->SetStatus(opentelemetry::trace::StatusCode::kError, e.what());
		return NotificationResult::TransientFailure;
	}
	catch (...) {
		ZTC_LOG("PubSubNetworkListener Unknown exception in PubSubNetworkListener\n");
		span->SetAttribute("error", "Unknown exception in PubSubNetworkListener");
		span->SetStatus(opentelemetry::trace::StatusCode::kError, "Unknown exception");
		return NotificationResult::PermanentFailure;
	}
	return NotificationResult::Ok;
}

PubSubMemberListener::PubSubMemberListener(std::string controller_id, std::string project, std::string topic, DB* db)
	: PubSubListener(controller_id, project, topic)
	, _db(db)
{
}

PubSubMemberListener::~PubSubMemberListener()
{
	stop();	  // join the subscriber thread before this object's members are destroyed
}

NotificationResult PubSubMemberListener::onNotification(const std::string& payload)
{
	auto provider = opentelemetry::trace::Provider::GetTracerProvider();
	auto tracer = provider->GetTracer("PubSubMemberListener");
	auto span = tracer->StartSpan("PubSubMemberListener::onNotification");
	auto scope = tracer->WithActiveSpan(span);

	pbmessages::MemberChange mc;
	if (! mc.ParseFromString(payload)) {
		ZTC_LOG("Failed to parse MemberChange protobuf message\n");
		span->SetAttribute("error", "Failed to parse MemberChange protobuf message");
		span->SetStatus(opentelemetry::trace::StatusCode::kError, "Failed to parse protobuf");
		return NotificationResult::PermanentFailure;
	}

	try {
		nlohmann::json tmp;
		nlohmann::json oldConfig, newConfig;

		if (mc.has_old()) {
			ZTC_LOG("has old member config\n");
			oldConfig = toJson(mc.old(), mc.change_source());
		}

		if (mc.has_new_()) {
			ZTC_LOG("has new member config\n");
			newConfig = toJson(mc.new_(), mc.change_source());
		}

		if (! mc.has_old() && ! mc.has_new_()) {
			ZTC_LOG("MemberChange message has no old or new member config\n");
			span->SetAttribute("error", "MemberChange message has no old or new member config");
			span->SetStatus(opentelemetry::trace::StatusCode::kError, "No old or new config");
			return NotificationResult::PermanentFailure;
		}

		if (oldConfig.is_object() && newConfig.is_object()) {
			// member modification
			std::string memberID = oldConfig["id"].get<std::string>();
			std::string networkID = oldConfig["nwid"].get<std::string>();
			span->SetAttribute("action", "member_change");
			span->SetAttribute("member_id", memberID);
			span->SetAttribute("network_id", networkID);
			_db->save(newConfig, _db->isReady());
		}
		else if (newConfig.is_object() && ! oldConfig.is_object()) {
			// new member
			std::string memberID = newConfig["id"].get<std::string>();
			std::string networkID = newConfig["nwid"].get<std::string>();
			span->SetAttribute("action", "new_member");
			span->SetAttribute("member_id", memberID);
			span->SetAttribute("network_id", networkID);
			_db->save(newConfig, _db->isReady());
		}
		else if (! newConfig.is_object() && oldConfig.is_object()) {
			// member deletion
			std::string memberID = oldConfig["id"].get<std::string>();
			std::string networkID = oldConfig["nwid"].get<std::string>();
			span->SetAttribute("action", "delete_member");
			span->SetAttribute("member_id", memberID);
			span->SetAttribute("network_id", networkID);

			uint64_t networkId = Utils::hexStrToU64(networkID.c_str());
			uint64_t memberId = Utils::hexStrToU64(memberID.c_str());
			if (networkId && memberId) {
				_db->eraseMember(networkId, memberId);
			}
		}
	}
	catch (const nlohmann::json::exception& e) {
		// Malformed/unexpected message contents (parse or type error) — unprocessable, drop it.
		ZTC_LOG("PubSubMemberListener JSON error: %s\n", e.what());
		span->SetAttribute("error", e.what());
		span->SetStatus(opentelemetry::trace::StatusCode::kError, e.what());
		ZTC_LOG("payload: %s\n", payload.c_str());
		return NotificationResult::PermanentFailure;
	}
	catch (const std::exception& e) {
		// Most likely a DB/runtime error from _db->save/eraseMember — retryable.
		ZTC_LOG("PubSubMemberListener Exception in PubSubMemberListener: %s\n", e.what());
		span->SetAttribute("error", e.what());
		span->SetStatus(opentelemetry::trace::StatusCode::kError, e.what());
		return NotificationResult::TransientFailure;
	}
	return NotificationResult::Ok;
}

nlohmann::json toJson(const pbmessages::NetworkChange_Network& nc, pbmessages::NetworkChange_ChangeSource source)
{
	nlohmann::json out;

	out["objtype"] = "network";
	out["id"] = nc.network_id();
	out["name"] = nc.name();

	try {
		std::string caps = nc.capabilities();
		if (caps.length() == 0) {
			out["capabilities"] = nlohmann::json::array();
		}
		else if (caps == "null") {
			out["capabilities"] = nlohmann::json::array();
		}
		else {
			out["capabilities"] = OSUtils::jsonParse(caps);
		}
	}
	catch (const nlohmann::json::parse_error& e) {
		ZTC_LOG("toJson Network capabilities JSON parse error: %s\n", e.what());
		out["capabilities"] = nlohmann::json::array();
	}

	out["mtu"] = nc.mtu();
	out["multicastLimit"] = nc.multicast_limit();
	out["private"] = nc.is_private();
	out["enableBroadcast"] = nc.enable_broadcast();
	out["creationTime"] = nc.creation_time();
	out["revision"] = nc.revision();
	out["remoteTraceLevel"] = nc.remote_trace_level();
	if (nc.has_remote_trace_target()) {
		out["remoteTraceTarget"] = nc.remote_trace_target();
	}
	else {
		out["remoteTraceTarget"] = "";
	}

	try {
		std::string rules = nc.rules();
		if (rules.length() == 0) {
			out["rules"] = nlohmann::json::array();
		}
		else if (rules == "null") {
			out["rules"] = nlohmann::json::array();
		}
		else {
			out["rules"] = OSUtils::jsonParse(rules);
		}
	}
	catch (const nlohmann::json::parse_error& e) {
		ZTC_LOG("toJson Network rules JSON parse error: %s\n", e.what());
		out["rules"] = nlohmann::json::array();
	}

	out["rulesSource"] = nc.rules_source();

	try {
		std::string tags = nc.tags();
		if (tags.length() == 0) {
			out["tags"] = nlohmann::json::array();
		}
		else if (tags == "[]") {
			out["tags"] = nlohmann::json::array();
		}
		else {
			out["tags"] = OSUtils::jsonParse(tags);
		}
	}
	catch (const nlohmann::json::parse_error& e) {
		ZTC_LOG("toJson Network tags JSON parse error: %s\n", e.what());
		out["tags"] = nlohmann::json::array();
	}

	if (nc.has_ipv4_assign_mode()) {
		nlohmann::json ipv4mode;
		ipv4mode["zt"] = nc.ipv4_assign_mode().zt();
		out["v4AssignMode"] = ipv4mode;
	}
	else {
		nlohmann::json ipv4mode = nlohmann::json::object();
		out["zt"] = false;
		out["v4AssignMode"] = ipv4mode;
	}

	if (nc.has_ipv6_assign_mode()) {
		nlohmann::json ipv6mode;
		ipv6mode["6plane"] = nc.ipv6_assign_mode().six_plane();
		ipv6mode["rfc4193"] = nc.ipv6_assign_mode().rfc4193();
		ipv6mode["zt"] = nc.ipv6_assign_mode().zt();
		out["v6AssignMode"] = ipv6mode;
	}
	else {
		nlohmann::json ipv6mode = nlohmann::json::object();
		ipv6mode["6plane"] = false;
		ipv6mode["rfc4193"] = false;
		ipv6mode["zt"] = false;
		out["v6AssignMode"] = ipv6mode;
	}

	if (nc.assignment_pools_size() > 0) {
		nlohmann::json pools = nlohmann::json::array();
		for (const auto& p : nc.assignment_pools()) {
			nlohmann::json pool;
			pool["ipRangeStart"] = p.start_ip();
			pool["ipRangeEnd"] = p.end_ip();
			pools.push_back(pool);
		}
		out["ipAssignmentPools"] = pools;
	}
	else {
		out["ipAssignmentPools"] = nlohmann::json::array();
	}

	if (nc.routes_size() > 0) {
		nlohmann::json routes = nlohmann::json::array();
		for (const auto& r : nc.routes()) {
			nlohmann::json route;
			std::string target = r.target();
			if (target.length() > 0) {
				route["target"] = r.target();
				if (r.has_via()) {
					route["via"] = r.via();
				}
				else {
					route["via"] = nullptr;
				}
				routes.push_back(route);
			}
		}
		out["routes"] = routes;
	}

	if (nc.has_dns()) {
		nlohmann::json dns;
		if (nc.dns().nameservers_size() > 0) {
			nlohmann::json servers = nlohmann::json::array();
			for (const auto& s : nc.dns().nameservers()) {
				servers.push_back(s);
			}
			dns["servers"] = servers;
		}
		else {
			dns["servers"] = nlohmann::json::array();
		}
		dns["domain"] = nc.dns().domain();

		out["dns"] = dns;
	}

	out["ssoEnabled"] = nc.sso_enabled();
	nlohmann::json sso;
	if (nc.sso_enabled()) {
		sso = nlohmann::json::object();
		if (nc.has_sso_client_id()) {
			sso["ssoClientId"] = nc.sso_client_id();
		}

		if (nc.has_sso_linked_id()) {
			sso["ssoLinkedId"] = nc.sso_linked_id();
		}
	}
	out["ssoConfig"] = sso;
	switch (source) {
		case pbmessages::NetworkChange_ChangeSource_CV1:
			out["change_source"] = "cv1";
			break;
		case pbmessages::NetworkChange_ChangeSource_CV2:
			out["change_source"] = "cv2";
			break;
		case pbmessages::NetworkChange_ChangeSource_CONTROLLER:
			out["change_source"] = "controller";
			break;
		default:
			out["change_source"] = "unknown";
			break;
	}

	return out;
}

nlohmann::json toJson(const pbmessages::MemberChange_Member& mc, pbmessages::MemberChange_ChangeSource source)
{
	nlohmann::json out;
	out["objtype"] = "member";
	out["id"] = mc.device_id();
	out["nwid"] = mc.network_id();
	if (mc.has_remote_trace_target()) {
		out["remoteTraceTarget"] = mc.remote_trace_target();
	}
	else {
		out["remoteTraceTarget"] = "";
	}
	out["authorized"] = mc.authorized();
	out["activeBridge"] = mc.active_bridge();

	auto ipAssignments = mc.ip_assignments();
	if (ipAssignments.size() > 0) {
		nlohmann::json assignments = nlohmann::json::array();
		for (const auto& ip : ipAssignments) {
			assignments.push_back(ip);
		}
		out["ipAssignments"] = assignments;
	}

	out["noAutoAssignIps"] = mc.no_auto_assign_ips();
	out["ssoExempt"] = mc.sso_exempt();
	out["authenticationExpiryTime"] = mc.auth_expiry_time();

	try {
		std::string caps = mc.capabilities();
		if (caps.length() == 0) {
			out["capabilities"] = nlohmann::json::array();
		}
		else if (caps == "null") {
			out["capabilities"] = nlohmann::json::array();
		}
		else {
			out["capabilities"] = OSUtils::jsonParse(caps);
		}
	}
	catch (const nlohmann::json::parse_error& e) {
		ZTC_LOG("MemberChange member capabilities JSON parse error: %s\n", e.what());
		ZTC_LOG("capabilities: %s\n", mc.capabilities().c_str());
		out["capabilities"] = nlohmann::json::array();
	}

	out["creationTime"] = mc.creation_time();
	out["identity"] = mc.identity();
	out["lastAuthorizedTime"] = mc.last_authorized_time();
	out["lastDeauthorizedTime"] = mc.last_deauthorized_time();
	out["remoteTraceLevel"] = mc.remote_trace_level();
	out["revision"] = mc.revision();

	try {
		std::string tags = mc.tags();
		if (tags.length() == 0) {
			out["tags"] = nlohmann::json::array();
		}
		else if (tags == "null") {
			out["tags"] = nlohmann::json::array();
		}
		else {
			out["tags"] = OSUtils::jsonParse(tags);
		}
	}
	catch (const nlohmann::json::parse_error& e) {
		ZTC_LOG("MemberChange member tags JSON parse error: %s\n", e.what());
		ZTC_LOG("tags: %s\n", mc.tags().c_str());
		out["tags"] = nlohmann::json::array();
	}

	out["vMajor"] = mc.version_major();
	out["vMinor"] = mc.version_minor();
	out["vRev"] = mc.version_rev();
	out["vProto"] = mc.version_protocol();
	switch (source) {
		case pbmessages::MemberChange_ChangeSource_CV1:
			out["change_source"] = "cv1";
			break;
		case pbmessages::MemberChange_ChangeSource_CV2:
			out["change_source"] = "cv2";
			break;
		case pbmessages::MemberChange_ChangeSource_CONTROLLER:
			out["change_source"] = "controller";
			break;
		default:
			out["change_source"] = "unknown";
			break;
	}

	return out;
}

PubSubSSOListener::PubSubSSOListener(
	std::string controller_id,
	std::string project,
	std::string topic,
	std::shared_ptr<ConnectionPool<PostgresConnection> > pool)
	: PubSubListener(controller_id, project, topic)
	, _pool(pool)
{
}

PubSubSSOListener::~PubSubSSOListener()
{
	stop();	  // join the subscriber thread before this object's members are destroyed
}

NotificationResult PubSubSSOListener::onNotification(const std::string& payload)
{
	auto provider = opentelemetry::trace::Provider::GetTracerProvider();
	auto tracer = provider->GetTracer("PubSubSSOListener");
	auto span = tracer->StartSpan("PubSubSSOListener::onNotification");
	auto scope = tracer->WithActiveSpan(span);

	pbmessages::SSOUpdate msg;
	if (! msg.ParseFromString(payload)) {
		ZTC_LOG("Failed to parse SSOUpdate protobuf message\n");
		span->SetStatus(opentelemetry::trace::StatusCode::kError, "Failed to parse protobuf");
		return NotificationResult::PermanentFailure;
	}

	if (msg.message_type() != pbmessages::SSOUpdate::ZT1_AUTH_UPDATE) {
		ZTC_LOG("PubSubSSOListener: ignoring non-ZT1_AUTH_UPDATE message type %d\n", msg.message_type());
		return NotificationResult::Ok;	 // valid message we intentionally don't handle — ack
	}

	if (! msg.has_auth_update()) {
		ZTC_LOG("PubSubSSOListener: ZT1_AUTH_UPDATE message missing auth_update\n");
		span->SetStatus(opentelemetry::trace::StatusCode::kError, "Missing auth_update");
		return NotificationResult::PermanentFailure;
	}

	const auto& au = msg.auth_update();
	std::string nonce = au.nonce();
	uint64_t authExpiry = au.authentication_expiry();
	std::string networkId = au.network_id();
	std::string deviceId = au.device_id();

	span->SetAttribute("network_id", networkId);
	span->SetAttribute("device_id", deviceId);
	span->SetAttribute("nonce", nonce);

	try {
		PooledConnection<PostgresConnection> c(_pool);
		pqxx::work w(*c->c);

		// Verify network exists and log its frontend for traceability
		pqxx::result fr = w.exec(
			"SELECT frontend FROM networks_ctl WHERE id = $1",
			pqxx::params { networkId });
		if (fr.empty()) {
			ZTC_LOG("PubSubSSOListener: ignoring auth update for unknown network=%s\n", networkId.c_str());
			w.abort();
			return NotificationResult::Ok;	 // ack — no point redelivering for a nonexistent network
		}
		std::string frontend = fr.at(0)[0].as<std::optional<std::string> >().value_or("");
		span->SetAttribute("frontend", frontend);

		pqxx::result res = w.exec(
			"UPDATE sso_expiry "
			"SET authentication_expiry_time = TO_TIMESTAMP($1::double precision / 1000) "
			"WHERE nonce = $2 AND network_id = $3 AND device_id = $4",
			pqxx::params { authExpiry, nonce, networkId, deviceId });

		w.commit();

		if (res.affected_rows() == 0) {
			fprintf(
				stderr, "PubSubSSOListener: no sso_expiry row matched for nonce=%s network=%s device=%s\n",
				nonce.c_str(), networkId.c_str(), deviceId.c_str());
		}
		// c (PooledConnection) returns the connection to the pool on scope exit.
	}
	catch (std::exception& e) {
		// Pool exhaustion / DB error — retryable. Nack so the auth update is redelivered
		// rather than silently lost (a lost SSO auth-expiry update is security-relevant).
		ZTC_LOG("PubSubSSOListener: error updating sso_expiry: %s\n", e.what());
		span->SetStatus(opentelemetry::trace::StatusCode::kError, e.what());
		return NotificationResult::TransientFailure;
	}

	return NotificationResult::Ok;
}

}	// namespace ZeroTier

#endif	 // ZT_CONTROLLER_USE_LIBPQ