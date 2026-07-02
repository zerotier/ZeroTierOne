/* (c) ZeroTier, Inc.
 * See LICENSE.txt in nonfree/
 */

#include "PubSubWriter.hpp"

#include "../../osdep/OSUtils.hpp"
#include "CtlUtil.hpp"
#include "OtelCarrier.hpp"
#include "member.pb.h"
#include "member_status.pb.h"
#include "network.pb.h"
#include "opentelemetry/context/propagation/global_propagator.h"
#include "sso.pb.h"

#include <chrono>
#include <google/cloud/options.h>
#include <google/cloud/pubsub/message.h>
#include <google/cloud/pubsub/publisher.h>
#include <google/cloud/pubsub/topic.h>
#include <opentelemetry/trace/provider.h>

namespace pubsub = ::google::cloud::pubsub;

namespace ZeroTier {

pbmessages::NetworkChange*
networkChangeFromJson(std::string controllerID, const nlohmann::json& oldNetwork, const nlohmann::json& newNetwork);
pbmessages::MemberChange*
memberChangeFromJson(std::string controllerID, const nlohmann::json& oldMember, const nlohmann::json& newMember);

namespace {
// Safe const field lookup. Returns a reference to j[key], or to a shared null
// json when key is absent (or j is not an object). Avoids the undefined
// behavior of nlohmann's const operator[] on a missing key, whose only guard
// (JSON_ASSERT) is compiled out in release/NDEBUG builds.
const nlohmann::json& jfield(const nlohmann::json& j, const char* key)
{
	static const nlohmann::json kNull;
	auto it = j.find(key);	 // find() is safe even when j is not an object
	return (it != j.end()) ? *it : kNull;
}
}	// namespace

PubSubWriter::PubSubWriter(std::string project, std::string topic, std::string controller_id)
	: _controller_id(controller_id)
	, _project(project)
	, _topic(topic)
{
	ZTC_LOG("PubSubWriter for controller %s project %s topic %s\n", controller_id.c_str(), project.c_str(),
			topic.c_str());
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	// If PUBSUB_EMULATOR_HOST is set, create the topic if it doesn't exist
	const char* emulatorHost = std::getenv("PUBSUB_EMULATOR_HOST");
	if (emulatorHost != nullptr) {
		create_gcp_pubsub_topic_if_needed(project, topic);
	}

	auto options =
		::google::cloud::Options {}
			.set<pubsub::RetryPolicyOption>(pubsub::LimitedTimeRetryPolicy(std::chrono::seconds(5)).clone())
			.set<pubsub::BackoffPolicyOption>(
				pubsub::ExponentialBackoffPolicy(std::chrono::milliseconds(100), std::chrono::seconds(2), 1.3).clone())
			.set<pubsub::MessageOrderingOption>(true);
	auto publisher = pubsub::MakePublisherConnection(pubsub::Topic(project, topic), std::move(options));
	_publisher = std::make_shared<pubsub::Publisher>(std::move(publisher));
}

PubSubWriter::~PubSubWriter()
{
}

bool PubSubWriter::publishMessage(const std::string& payload,
								  const std::string& frontend,
								  const std::string& orderingKey)
{
	auto provider = opentelemetry::trace::Provider::GetTracerProvider();
	auto tracer = provider->GetTracer("PubSubWriter");
	auto span = tracer->StartSpan("PubSubWriter::publishMessage");
	auto scope = tracer->WithActiveSpan(span);

	std::vector<std::pair<std::string, std::string> > attributes;
	attributes.emplace_back("controller_id", _controller_id);

	std::map<std::string, std::string> attrs_map;
	OtelCarrier<std::map<std::string, std::string> > carrier(attrs_map);
	auto propagator = opentelemetry::context::propagation::GlobalTextMapPropagator::GetGlobalPropagator();
	auto current_ctx = opentelemetry::context::RuntimeContext::GetCurrent();
	propagator->Inject(carrier, current_ctx);

	for (const auto& kv : attrs_map) {
		attributes.emplace_back(kv.first, kv.second);
	}

	if (! frontend.empty()) {
		attributes.emplace_back("frontend", frontend);
	}

	auto msg_tmp = pubsub::MessageBuilder {}.SetData(payload).SetAttributes(attributes);
	if (! orderingKey.empty()) {
		msg_tmp.SetOrderingKey(orderingKey);
	}
	auto msg = std::move(msg_tmp).Build();
	auto message_id = _publisher->Publish(std::move(msg)).get();
	if (! message_id) {
		auto status = std::move(message_id).status();
		ZTC_LOG("Failed to publish message (ordering_key=%s): %s\n", orderingKey.c_str(), status.message().c_str());
		if (! orderingKey.empty()) {
			// Message ordering is enabled (MessageOrderingOption(true)), so a failed publish
			// permanently rejects every subsequent message for this ordering key until
			// ResumePublish() is called. Without this, one transient failure would silently
			// wedge change propagation for this network/member for the life of the process.
			// We resume to unblock future messages; this one is reported as failed to the caller.
			_publisher->ResumePublish(orderingKey);
		}
		return false;
	}

	return true;
}

bool PubSubWriter::publishNetworkChange(const nlohmann::json& oldNetwork,
										const nlohmann::json& newNetwork,
										const std::string& frontend)
{
	pbmessages::NetworkChange* nc = networkChangeFromJson(_controller_id, oldNetwork, newNetwork);

	std::string networkID;
	if (nc->has_new_()) {
		networkID = nc->new_().network_id();
	}
	else if (nc->has_old()) {
		networkID = nc->old().network_id();
	}

	std::string payload;
	if (! nc->SerializeToString(&payload)) {
		ZTC_LOG("Failed to serialize NetworkChange protobuf message\n");
		delete nc;
		return false;
	}
	delete nc;
	return publishMessage(payload, frontend, networkID);
}

bool PubSubWriter::publishMemberChange(const nlohmann::json& oldMember,
									   const nlohmann::json& newMember,
									   const std::string& frontend)
{
	pbmessages::MemberChange* mc = memberChangeFromJson(_controller_id, oldMember, newMember);
	std::string memberID;
	if (mc->has_new_()) {
		memberID = mc->new_().network_id() + "-" + mc->new_().device_id();
	}
	else if (mc->has_old()) {
		memberID = mc->old().network_id() + "-" + mc->old().device_id();
	}

	std::string payload;
	if (! mc->SerializeToString(&payload)) {
		ZTC_LOG("Failed to serialize MemberChange protobuf message\n");
		delete mc;
		return false;
	}

	delete mc;
	return publishMessage(payload, frontend, memberID);
}

bool PubSubWriter::publishStatusChange(std::string frontend,
									   std::string network_id,
									   std::string node_id,
									   std::string os,
									   std::string arch,
									   std::string version,
									   int64_t last_seen)
{
	auto provider = opentelemetry::trace::Provider::GetTracerProvider();
	auto tracer = provider->GetTracer("PubSubWriter");
	auto span = tracer->StartSpan("PubSubWriter::publishStatusChange");
	auto scope = tracer->WithActiveSpan(span);

	pbmessages::MemberStatus_MemberStatusMetadata* metadata = new pbmessages::MemberStatus_MemberStatusMetadata();
	metadata->set_controller_id(_controller_id);
	metadata->set_trace_id("");	  // TODO: generate a trace ID

	pbmessages::MemberStatus ms;
	ms.set_network_id(network_id);
	ms.set_member_id(node_id);
	ms.set_os(os);
	ms.set_arch(arch);
	ms.set_version(version);
	ms.set_timestamp(last_seen);
	ms.set_allocated_metadata(metadata);

	std::string payload;
	if (! ms.SerializeToString(&payload)) {
		ZTC_LOG("Failed to serialize StatusChange protobuf message\n");
		return false;
	}

	return publishMessage(payload, "", "");
}

bool PubSubWriter::publishSSONonceUpdate(const std::string& nonce,
										 uint64_t nonceExpiration,
										 const std::string& networkId,
										 const std::string& deviceId,
										 const std::string& frontend)
{
	auto provider = opentelemetry::trace::Provider::GetTracerProvider();
	auto tracer = provider->GetTracer("PubSubWriter");
	auto span = tracer->StartSpan("PubSubWriter::publishSSONonceUpdate");
	auto scope = tracer->WithActiveSpan(span);

	pbmessages::SSOUpdate msg;
	msg.set_message_type(pbmessages::SSOUpdate::CTL_NONCE_UPDATE);

	pbmessages::SSOUpdate_NonceUpdate* nu = new pbmessages::SSOUpdate_NonceUpdate();
	nu->set_nonce(nonce);
	nu->set_nonce_expiration(nonceExpiration);
	nu->set_network_id(networkId);
	nu->set_device_id(deviceId);
	msg.set_allocated_nonce_update(nu);

	std::string payload;
	if (! msg.SerializeToString(&payload)) {
		ZTC_LOG("Failed to serialize SSOUpdate protobuf message\n");
		return false;
	}

	return publishMessage(payload, frontend, networkId + "-" + deviceId);
}

pbmessages::NetworkChange_Network* networkFromJson(const nlohmann::json& j)
{
	if (! j.is_object()) {
		return nullptr;
	}

	pbmessages::NetworkChange_Network* n = new pbmessages::NetworkChange_Network();
	try {
		n->set_network_id(OSUtils::jsonString(jfield(j, "id"), ""));
		n->set_name(OSUtils::jsonString(jfield(j, "name"), ""));
		n->set_capabilities(OSUtils::jsonDump(j.value("capabilities", "[]"), -1));
		n->set_creation_time(OSUtils::jsonInt(jfield(j, "creationTime"), 0));
		n->set_enable_broadcast(OSUtils::jsonBool(jfield(j, "enableBroadcast"), false));

		for (const auto& p : jfield(j, "ipAssignmentPools")) {
			if (p.is_object()) {
				auto pool = n->add_assignment_pools();
				pool->set_start_ip(OSUtils::jsonString(jfield(p, "ipRangeStart"), ""));
				pool->set_end_ip(OSUtils::jsonString(jfield(p, "ipRangeEnd"), ""));
			}
		}

		n->set_mtu(OSUtils::jsonInt(jfield(j, "mtu"), 2800));
		n->set_multicast_limit(OSUtils::jsonInt(jfield(j, "multicastLimit"), 32));
		n->set_is_private(OSUtils::jsonBool(jfield(j, "private"), true));
		n->set_remote_trace_level(OSUtils::jsonInt(jfield(j, "remoteTraceLevel"), 0));
		n->set_remote_trace_target(OSUtils::jsonString(jfield(j, "remoteTraceTarget"), ""));
		n->set_revision(OSUtils::jsonInt(jfield(j, "revision"), 0));

		for (const auto& p : jfield(j, "routes")) {
			if (p.is_object()) {
				auto r = n->add_routes();
				r->set_target(OSUtils::jsonString(jfield(p, "target"), ""));
				r->set_via(OSUtils::jsonString(jfield(p, "via"), ""));
			}
		}
		std::string rules;
		if (jfield(j, "rules").is_array()) {
			rules = OSUtils::jsonDump(jfield(j, "rules"), -1);
		}
		else {
			rules = "[]";
		}
		n->set_rules(rules);

		std::string tags;
		if (jfield(j, "tags").is_array()) {
			tags = OSUtils::jsonDump(jfield(j, "tags"), -1);
		}
		else {
			tags = "[]";
		}
		n->set_tags(tags);

		pbmessages::NetworkChange_IPV4AssignMode* v4am = new pbmessages::NetworkChange_IPV4AssignMode();
		if (jfield(j, "v4AssignMode").is_object()) {
			nlohmann::json am = jfield(j, "v4AssignMode");
			v4am->set_zt(OSUtils::jsonBool(am["zt"], false));
		}
		n->set_allocated_ipv4_assign_mode(v4am);

		pbmessages::NetworkChange_IPV6AssignMode* v6am = new pbmessages::NetworkChange_IPV6AssignMode();
		if (jfield(j, "v6AssignMode").is_object()) {
			nlohmann::json am = jfield(j, "v6AssignMode");
			v6am->set_zt(OSUtils::jsonBool(am["zt"], false));
			v6am->set_six_plane(OSUtils::jsonBool(am["6plane"], false));
			v6am->set_rfc4193(OSUtils::jsonBool(am["rfc4193"], false));
		}
		n->set_allocated_ipv6_assign_mode(v6am);

		nlohmann::json jdns = jfield(j, "dns");
		if (jdns.is_object()) {
			pbmessages::NetworkChange_DNS* dns = new pbmessages::NetworkChange_DNS();
			dns->set_domain(jdns.value("domain", ""));
			for (const auto& s : jdns["servers"]) {
				if (s.is_string()) {
					auto server = dns->add_nameservers();
					*server = s;
				}
			}
			n->set_allocated_dns(dns);
		}

		n->set_sso_enabled(OSUtils::jsonBool(jfield(j, "ssoEnabled"), false));
		nlohmann::json ssocfg = jfield(j, "ssoConfig");
		if (ssocfg.is_object()) {
			n->set_sso_client_id(OSUtils::jsonString(ssocfg["ssoClientId"], ""));
			n->set_sso_linked_id(OSUtils::jsonString(ssocfg["ssoLinkedId"], ""));
		}

		n->set_rules_source(OSUtils::jsonString(jfield(j, "rulesSource"), ""));
	}
	catch (const std::exception& e) {
		ZTC_LOG("Exception parsing network JSON: %s\n", e.what());
		delete n;
		return nullptr;
	}

	return n;
}

pbmessages::NetworkChange*
networkChangeFromJson(std::string controllerID, const nlohmann::json& oldNetwork, const nlohmann::json& newNetwork)
{
	pbmessages::NetworkChange* nc = new pbmessages::NetworkChange();

	nc->set_allocated_old(networkFromJson(oldNetwork));
	nc->set_allocated_new_(networkFromJson(newNetwork));
	nc->set_change_source(pbmessages::NetworkChange_ChangeSource::NetworkChange_ChangeSource_CONTROLLER);

	pbmessages::NetworkChange_NetworkChangeMetadata* metadata = new pbmessages::NetworkChange_NetworkChangeMetadata();
	metadata->set_controller_id(controllerID);
	metadata->set_trace_id("");	  // TODO: generate a trace ID
	nc->set_allocated_metadata(metadata);

	return nc;
}

pbmessages::MemberChange_Member* memberFromJson(const nlohmann::json& j)
{
	if (! j.is_object()) {
		return nullptr;
	}

	pbmessages::MemberChange_Member* m = new pbmessages::MemberChange_Member();
	try {
		m->set_network_id(OSUtils::jsonString(jfield(j, "nwid"), ""));
		m->set_device_id(OSUtils::jsonString(jfield(j, "id"), ""));
		m->set_identity(OSUtils::jsonString(jfield(j, "identity"), ""));
		m->set_authorized(OSUtils::jsonBool(jfield(j, "authorized"), false));
		if (jfield(j, "ipAssignments").is_array()) {
			for (const auto& addr : jfield(j, "ipAssignments")) {
				if (addr.is_string()) {
					auto a = m->add_ip_assignments();
					std::string address = addr.get<std::string>();
					*a = address;
				}
			}
		}
		m->set_active_bridge(OSUtils::jsonBool(jfield(j, "activeBridge"), false));
		if (jfield(j, "tags").is_array()) {
			nlohmann::json tags = jfield(j, "tags");
			std::string tagsStr = OSUtils::jsonDump(tags, -1);
			m->set_tags(tagsStr);
		}
		else {
			nlohmann::json tags = nlohmann::json::array();
			std::string tagsStr = OSUtils::jsonDump(tags, -1);
			m->set_tags(tagsStr);
		}
		if (jfield(j, "capabilities").is_array()) {
			nlohmann::json caps = jfield(j, "capabilities");
			std::string capsStr = OSUtils::jsonDump(caps, -1);
			m->set_capabilities(capsStr);
		}
		else {
			nlohmann::json caps = nlohmann::json::array();
			std::string capsStr = OSUtils::jsonDump(caps, -1);
			m->set_capabilities(capsStr);
		}
		m->set_creation_time(OSUtils::jsonInt(jfield(j, "creationTime"), 0));
		m->set_no_auto_assign_ips(OSUtils::jsonBool(jfield(j, "noAutoAssignIps"), false));
		m->set_revision(OSUtils::jsonInt(jfield(j, "revision"), 0));
		m->set_last_authorized_time(OSUtils::jsonInt(jfield(j, "lastAuthorizedTime"), 0));
		m->set_last_deauthorized_time(OSUtils::jsonInt(jfield(j, "lastDeauthorizedTime"), 0));
		m->set_last_authorized_credential_type(OSUtils::jsonString(jfield(j, "lastAuthorizedCredentialType"), ""));
		m->set_last_authorized_credential(OSUtils::jsonString(jfield(j, "lastAuthorizedCredential"), ""));
		m->set_version_major(OSUtils::jsonInt(jfield(j, "vMajor"), 0));
		m->set_version_minor(OSUtils::jsonInt(jfield(j, "vMinor"), 0));
		m->set_version_rev(OSUtils::jsonInt(jfield(j, "vRev"), 0));
		m->set_version_protocol(OSUtils::jsonInt(jfield(j, "vProto"), 0));
		m->set_remote_trace_level(OSUtils::jsonInt(jfield(j, "remoteTraceLevel"), 0));
		m->set_remote_trace_target(OSUtils::jsonString(jfield(j, "remoteTraceTarget"), ""));
		m->set_sso_exempt(OSUtils::jsonBool(jfield(j, "ssoExempt"), false));
		m->set_auth_expiry_time(OSUtils::jsonInt(jfield(j, "authenticationExpiryTime"), 0));
	}
	catch (const std::exception& e) {
		ZTC_LOG("Exception parsing member JSON: %s\n", e.what());
		delete m;
		return nullptr;
	}
	return m;
}

pbmessages::MemberChange*
memberChangeFromJson(std::string controllerID, const nlohmann::json& oldMember, const nlohmann::json& newMember)
{
	pbmessages::MemberChange* mc = new pbmessages::MemberChange();
	pbmessages::MemberChange_Member* om = memberFromJson(oldMember);
	if (om != nullptr) {
		mc->set_allocated_old(om);
	}
	pbmessages::MemberChange_Member* nm = memberFromJson(newMember);
	if (nm != nullptr) {
		mc->set_allocated_new_(nm);
	}
	mc->set_change_source(pbmessages::MemberChange_ChangeSource::MemberChange_ChangeSource_CONTROLLER);

	pbmessages::MemberChange_MemberChangeMetadata* metadata = new pbmessages::MemberChange_MemberChangeMetadata();
	metadata->set_controller_id(controllerID);
	metadata->set_trace_id("");	  // TODO: generate a trace ID
	mc->set_allocated_metadata(metadata);

	return mc;
}

}	// namespace ZeroTier