/*
 * Copyright (c)2013-2023 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
#ifndef METRICS_H_
#define METRICS_H_

#include <prometheus/simpleapi.h>

namespace prometheus {
    namespace simpleapi {
        extern std::shared_ptr<Registry> registry_ptr;
    }
}

namespace ZeroTier {
    namespace Metrics {
        // Packet Type Counts
        extern prometheus::simpleapi::counter_family_t packets;
        extern prometheus::simpleapi::counter_metric_t pkt_error;
        extern prometheus::simpleapi::counter_metric_t pkt_ack;
        extern prometheus::simpleapi::counter_metric_t pkt_qos;
        extern prometheus::simpleapi::counter_metric_t pkt_hello;
        extern prometheus::simpleapi::counter_metric_t pkt_ok;
        extern prometheus::simpleapi::counter_metric_t pkt_whois;
        extern prometheus::simpleapi::counter_metric_t pkt_rendezvous;
        extern prometheus::simpleapi::counter_metric_t pkt_frame;
        extern prometheus::simpleapi::counter_metric_t pkt_ext_frame;
        extern prometheus::simpleapi::counter_metric_t pkt_echo;
        extern prometheus::simpleapi::counter_metric_t pkt_multicast_like;
        extern prometheus::simpleapi::counter_metric_t pkt_network_credentials;
        extern prometheus::simpleapi::counter_metric_t pkt_network_config_request;
        extern prometheus::simpleapi::counter_metric_t pkt_network_config;
        extern prometheus::simpleapi::counter_metric_t pkt_multicast_gather;
        extern prometheus::simpleapi::counter_metric_t pkt_multicast_frame;
        extern prometheus::simpleapi::counter_metric_t pkt_push_direct_paths;
        extern prometheus::simpleapi::counter_metric_t pkt_user_message;
        extern prometheus::simpleapi::counter_metric_t pkt_remote_trace;
        extern prometheus::simpleapi::counter_metric_t pkt_path_negotiation_request;

        // Packet Error Counts
        extern prometheus::simpleapi::counter_family_t packet_errors;
        extern prometheus::simpleapi::counter_metric_t pkt_error_obj_not_found;
        extern prometheus::simpleapi::counter_metric_t pkt_error_unsupported_op;
        extern prometheus::simpleapi::counter_metric_t pkt_error_identity_collision;
        extern prometheus::simpleapi::counter_metric_t pkt_error_need_membership_cert;
        extern prometheus::simpleapi::counter_metric_t pkt_error_network_access_denied;
        extern prometheus::simpleapi::counter_metric_t pkt_error_unwanted_multicast;
        extern prometheus::simpleapi::counter_metric_t pkt_error_authentication_required;


        // Data Sent/Received Metrics
        extern prometheus::simpleapi::counter_metric_t udp_send;
        extern prometheus::simpleapi::counter_metric_t udp_recv;
        extern prometheus::simpleapi::counter_metric_t tcp_send;
        extern prometheus::simpleapi::counter_metric_t tcp_recv;

        // General Controller Metrics
        extern prometheus::simpleapi::gauge_metric_t   network_count;
        extern prometheus::simpleapi::gauge_metric_t   member_count;
        extern prometheus::simpleapi::counter_metric_t network_changes;
        extern prometheus::simpleapi::counter_metric_t member_changes;
        extern prometheus::simpleapi::counter_metric_t member_auths;
        extern prometheus::simpleapi::counter_metric_t member_deauths;

#ifdef ZT_CONTROLLER_USE_LIBPQ
        // Central Controller Metrics
        extern prometheus::simpleapi::counter_metric_t pgsql_mem_notification;
        extern prometheus::simpleapi::counter_metric_t pgsql_net_notification;
        extern prometheus::simpleapi::counter_metric_t pgsql_node_checkin;
        extern prometheus::simpleapi::counter_metric_t redis_mem_notification;
        extern prometheus::simpleapi::counter_metric_t redis_net_notification;
        extern prometheus::simpleapi::counter_metric_t redis_node_checkin;

        // Central DB Pool Metrics
        extern prometheus::simpleapi::counter_metric_t conn_counter;
        extern prometheus::simpleapi::counter_metric_t max_pool_size;
        extern prometheus::simpleapi::counter_metric_t min_pool_size;
        extern prometheus::simpleapi::gauge_metric_t   pool_avail;
        extern prometheus::simpleapi::gauge_metric_t   pool_in_use;
        extern prometheus::simpleapi::counter_metric_t pool_errors;
#endif
    } // namespace Metrics
}// namespace ZeroTier

#endif // METRICS_H_
