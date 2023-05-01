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

        // incoming packets
        extern prometheus::simpleapi::counter_metric_t pkt_nop_in;
        extern prometheus::simpleapi::counter_metric_t pkt_error_in;
        extern prometheus::simpleapi::counter_metric_t pkt_ack_in;
        extern prometheus::simpleapi::counter_metric_t pkt_qos_in;
        extern prometheus::simpleapi::counter_metric_t pkt_hello_in;
        extern prometheus::simpleapi::counter_metric_t pkt_ok_in;
        extern prometheus::simpleapi::counter_metric_t pkt_whois_in;
        extern prometheus::simpleapi::counter_metric_t pkt_rendezvous_in;
        extern prometheus::simpleapi::counter_metric_t pkt_frame_in;
        extern prometheus::simpleapi::counter_metric_t pkt_ext_frame_in;
        extern prometheus::simpleapi::counter_metric_t pkt_echo_in;
        extern prometheus::simpleapi::counter_metric_t pkt_multicast_like_in;
        extern prometheus::simpleapi::counter_metric_t pkt_network_credentials_in;
        extern prometheus::simpleapi::counter_metric_t pkt_network_config_request_in;
        extern prometheus::simpleapi::counter_metric_t pkt_network_config_in;
        extern prometheus::simpleapi::counter_metric_t pkt_multicast_gather_in;
        extern prometheus::simpleapi::counter_metric_t pkt_multicast_frame_in;
        extern prometheus::simpleapi::counter_metric_t pkt_push_direct_paths_in;
        extern prometheus::simpleapi::counter_metric_t pkt_user_message_in;
        extern prometheus::simpleapi::counter_metric_t pkt_remote_trace_in;
        extern prometheus::simpleapi::counter_metric_t pkt_path_negotiation_request_in;

        // outgoing packets
        extern prometheus::simpleapi::counter_metric_t pkt_nop_out;
        extern prometheus::simpleapi::counter_metric_t pkt_error_out;
        extern prometheus::simpleapi::counter_metric_t pkt_ack_out;
        extern prometheus::simpleapi::counter_metric_t pkt_qos_out;
        extern prometheus::simpleapi::counter_metric_t pkt_hello_out;
        extern prometheus::simpleapi::counter_metric_t pkt_ok_out;
        extern prometheus::simpleapi::counter_metric_t pkt_whois_out;
        extern prometheus::simpleapi::counter_metric_t pkt_rendezvous_out;
        extern prometheus::simpleapi::counter_metric_t pkt_frame_out;
        extern prometheus::simpleapi::counter_metric_t pkt_ext_frame_out;
        extern prometheus::simpleapi::counter_metric_t pkt_echo_out;
        extern prometheus::simpleapi::counter_metric_t pkt_multicast_like_out;
        extern prometheus::simpleapi::counter_metric_t pkt_network_credentials_out;
        extern prometheus::simpleapi::counter_metric_t pkt_network_config_request_out;
        extern prometheus::simpleapi::counter_metric_t pkt_network_config_out;
        extern prometheus::simpleapi::counter_metric_t pkt_multicast_gather_out;
        extern prometheus::simpleapi::counter_metric_t pkt_multicast_frame_out;
        extern prometheus::simpleapi::counter_metric_t pkt_push_direct_paths_out;
        extern prometheus::simpleapi::counter_metric_t pkt_user_message_out;
        extern prometheus::simpleapi::counter_metric_t pkt_remote_trace_out;
        extern prometheus::simpleapi::counter_metric_t pkt_path_negotiation_request_out;

        // Packet Error Counts
        extern prometheus::simpleapi::counter_family_t packet_errors;

        // incoming errors
        extern prometheus::simpleapi::counter_metric_t pkt_error_obj_not_found_in;
        extern prometheus::simpleapi::counter_metric_t pkt_error_unsupported_op_in;
        extern prometheus::simpleapi::counter_metric_t pkt_error_identity_collision_in;
        extern prometheus::simpleapi::counter_metric_t pkt_error_need_membership_cert_in;
        extern prometheus::simpleapi::counter_metric_t pkt_error_network_access_denied_in;
        extern prometheus::simpleapi::counter_metric_t pkt_error_unwanted_multicast_in;
        extern prometheus::simpleapi::counter_metric_t pkt_error_authentication_required_in;
        extern prometheus::simpleapi::counter_metric_t pkt_error_internal_server_error_in;

        // outgoing errors
        extern prometheus::simpleapi::counter_metric_t pkt_error_obj_not_found_out;
        extern prometheus::simpleapi::counter_metric_t pkt_error_unsupported_op_out;
        extern prometheus::simpleapi::counter_metric_t pkt_error_identity_collision_out;
        extern prometheus::simpleapi::counter_metric_t pkt_error_need_membership_cert_out;
        extern prometheus::simpleapi::counter_metric_t pkt_error_network_access_denied_out;
        extern prometheus::simpleapi::counter_metric_t pkt_error_unwanted_multicast_out;
        extern prometheus::simpleapi::counter_metric_t pkt_error_authentication_required_out;
        extern prometheus::simpleapi::counter_metric_t pkt_error_internal_server_error_out;


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
