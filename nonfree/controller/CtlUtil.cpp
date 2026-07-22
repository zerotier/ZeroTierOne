/* (c) ZeroTier, Inc.
 * See LICENSE.txt in nonfree/
 */

#include "CtlUtil.hpp"

#include <pthread.h>

namespace ZeroTier {

// Defined unconditionally (outside ZT_CONTROLLER_USE_LIBPQ): the always-compiled controller
// files log via ZTC_LOG in every build. Set once at init before any worker threads start, so
// the read in controllerLogId() races with nothing.
static std::string s_controllerLogId;

void setControllerLogId(const std::string& id)
{ s_controllerLogId = id; }

const char* controllerLogId()
{ return s_controllerLogId.empty() ? "----------" : s_controllerLogId.c_str(); }

void setCurrentThreadName(const char* name)
{
	// Linux truncates at 15 chars + NUL; keep names within that.
#if defined(__APPLE__)
	pthread_setname_np(name);
#elif defined(__linux__)
	pthread_setname_np(pthread_self(), name);
#else
	(void)name;
#endif
}

}	// namespace ZeroTier

#ifdef ZT_CONTROLLER_USE_LIBPQ

#include <iomanip>
#include <sstream>

#ifdef ZT1_CENTRAL_CONTROLLER
#include <google/cloud/bigtable/admin/bigtable_table_admin_client.h>
#include <google/cloud/bigtable/admin/bigtable_table_admin_connection.h>
#include <google/cloud/bigtable/table.h>
#include <google/cloud/pubsub/admin/subscription_admin_client.h>
#include <google/cloud/pubsub/admin/subscription_admin_connection.h>
#include <google/cloud/pubsub/admin/topic_admin_client.h>
#include <google/cloud/pubsub/message.h>
#include <google/cloud/pubsub/subscriber.h>
#include <google/cloud/pubsub/subscription.h>
#include <google/cloud/pubsub/topic.h>

namespace pubsub = ::google::cloud::pubsub;
namespace pubsub_admin = ::google::cloud::pubsub_admin;
namespace bigtable_admin = ::google::cloud::bigtable_admin;
#endif

namespace ZeroTier {

const char* _timestr()
{
	time_t t = time(0);
	char* ts = ctime(&t);
	char* p = ts;
	if (! p)
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

std::vector<std::string> split(std::string str, char delim)
{
	std::istringstream iss(str);
	std::vector<std::string> tokens;
	std::string item;
	while (std::getline(iss, item, delim)) {
		tokens.push_back(item);
	}
	return tokens;
}

std::string url_encode(const std::string& value)
{
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
		escaped << '%' << std::setw(2) << int((unsigned char)c);
		escaped << std::nouppercase;
	}

	return escaped.str();
}

std::string random_hex_string(std::size_t length)
{
	static const char hex_chars[] = "0123456789abcdef";
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(0, 15);

	std::string result;
	result.reserve(length);
	for (std::size_t i = 0; i < length; ++i) {
		result += hex_chars[dis(gen)];
	}
	return result;
}

#ifdef ZT1_CENTRAL_CONTROLLER
void create_gcp_pubsub_topic_if_needed(std::string project_id, std::string topic_id)
{
	// This is a no-op if the topic already exists.
	auto topicAdminClient = pubsub_admin::TopicAdminClient(pubsub_admin::MakeTopicAdminConnection());
	auto topicName = pubsub::Topic(project_id, topic_id).FullName();
	auto topicResult = topicAdminClient.GetTopic(topicName);
	if (! topicResult.ok()) {
		// Only create if not found
		if (topicResult.status().code() == google::cloud::StatusCode::kNotFound) {
			auto createResult = topicAdminClient.CreateTopic(topicName);
			if (! createResult.ok()) {
				ZTC_LOG("Failed to create topic: %s\n", createResult.status().message().c_str());
				throw std::runtime_error("Failed to create topic");
			}
			ZTC_LOG("Created topic: %s\n", topicName.c_str());
		}
		else {
			ZTC_LOG("Failed to get topic: %s\n", topicResult.status().message().c_str());
			throw std::runtime_error("Failed to get topic");
		}
	}
}

void create_gcp_pubsub_subscription_if_needed(
	std::string project_id,
	std::string subscription_id,
	std::string topic_id,
	std::string controller_id)
{
	// This is a no-op if the subscription already exists.
	auto subscriptionAdminClient =
		pubsub_admin::SubscriptionAdminClient(pubsub_admin::MakeSubscriptionAdminConnection());
	auto topicName = pubsub::Topic(project_id, topic_id).FullName();
	auto subscriptionName = pubsub::Subscription(project_id, subscription_id).FullName();

	auto sub = subscriptionAdminClient.GetSubscription(subscriptionName);
	if (! sub.ok()) {
		if (sub.status().code() == google::cloud::StatusCode::kNotFound) {
			ZTC_LOG("Creating subscription %s for topic %s\n", subscriptionName.c_str(), topicName.c_str());
			google::pubsub::v1::Subscription request;
			request.set_name(subscriptionName);
			request.set_topic(pubsub::Topic(project_id, topic_id).FullName());
			request.set_filter("(attributes.controller_id=\"" + controller_id + "\")");
			request.set_enable_message_ordering(true);
			auto createResult = subscriptionAdminClient.CreateSubscription(request);
			if (! createResult.ok()) {
				ZTC_LOG("Failed to create subscription: %s\n", createResult.status().message().c_str());
				throw std::runtime_error("Failed to create subscription");
			}
			ZTC_LOG("Created subscription: %s\n", subscriptionName.c_str());
		}
		else {
			ZTC_LOG("Failed to get subscription: %s\n", sub.status().message().c_str());
			throw std::runtime_error("Failed to get subscription");
		}
	}
}

#endif

}	// namespace ZeroTier
#endif
