/* (c) ZeroTier, Inc.
 * See LICENSE.txt in nonfree/
 */

#ifndef ZT_CTLUTIL_HPP
#define ZT_CTLUTIL_HPP

#include <cstdio>
#include <string>
#include <vector>

namespace ZeroTier {

const char* _timestr();

// Process-wide controller (node) ID used to prefix every controller log line. Set once at
// controller init -- there is one controller/node per process -- so reads are lock-free.
// Available in every controller build (not gated on ZT_CONTROLLER_USE_LIBPQ) since the
// always-compiled files (EmbeddedNetworkController, DB, FileDB) log too.
void setControllerLogId(const std::string& id);
const char* controllerLogId();

// Name the calling thread so /proc wait-channel and stack dumps can identify the
// controller's thread pools. Linux limits names to 15 characters.
void setCurrentThreadName(const char* name);

std::vector<std::string> split(std::string str, char delim);

std::string url_encode(const std::string& value);

std::string random_hex_string(std::size_t length);

#ifdef ZT1_CENTRAL_CONTROLLER
void create_gcp_pubsub_topic_if_needed(std::string project_id, std::string topic_id);

void create_gcp_pubsub_subscription_if_needed(
	std::string project_id,
	std::string subscription_id,
	std::string topic_id,
	std::string controller_id);
#endif

}	// namespace ZeroTier

// Prefix every controller log line with the controller (node) ID. A single atomic fprintf;
// "%s " fmt stays a literal so the compiler's -Wformat still type-checks the args. The format
// argument MUST be a string literal.
#define ZTC_LOG(fmt, ...) fprintf(stderr, "%s " fmt, ::ZeroTier::controllerLogId(), ##__VA_ARGS__)

#endif	 // namespace ZeroTier
