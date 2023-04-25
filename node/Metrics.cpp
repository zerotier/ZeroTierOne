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

#include <prometheus/simpleapi.h>

namespace prometheus {
    namespace simpleapi {
        std::shared_ptr<Registry> registry_ptr = std::make_shared<Registry>();
        Registry&                 registry = *registry_ptr;
        SaveToFile saver;
    }
}

namespace ZeroTier {
    namespace Metrics {
        // Packet Type Counts
        prometheus::simpleapi::counter_family_t packets
        { "zt_packet_incoming", "incoming packet type counts"};
        prometheus::simpleapi::counter_metric_t pkt_error
        { packets.Add({{"packet_type", "error"}}) };
        prometheus::simpleapi::counter_metric_t pkt_ack
        { packets.Add({{"packet_type", "ack"}}) };
        prometheus::simpleapi::counter_metric_t pkt_qos
        { packets.Add({{"packet_type", "qos"}}) };
        prometheus::simpleapi::counter_metric_t pkt_hello
        { packets.Add({{"packet_type", "hello"}}) };
        prometheus::simpleapi::counter_metric_t pkt_ok
        { packets.Add({{"packet_type", "ok"}}) };
        prometheus::simpleapi::counter_metric_t pkt_whois
        { packets.Add({{"packet_type", "whois"}}) };
        prometheus::simpleapi::counter_metric_t pkt_rendezvous
        { packets.Add({{"packet_type", "rendezvous"}}) };
        prometheus::simpleapi::counter_metric_t pkt_frame
        { packets.Add({{"packet_type", "frame"}}) };
        prometheus::simpleapi::counter_metric_t pkt_ext_frame
        { packets.Add({{"packet_type", "ext_frame"}}) };
        prometheus::simpleapi::counter_metric_t pkt_echo
        { packets.Add({{"packet_type", "echo"}}) };
        prometheus::simpleapi::counter_metric_t pkt_multicast_like
        { packets.Add({{"packet_type", "multicast_like"}}) };
        prometheus::simpleapi::counter_metric_t pkt_network_credentials
        { packets.Add({{"packet_type", "network_credentials"}}) };
        prometheus::simpleapi::counter_metric_t pkt_network_config_request
        { packets.Add({{"packet_type", "network_config_request"}}) };
        prometheus::simpleapi::counter_metric_t pkt_network_config
        { packets.Add({{"packet_type", "network_config"}}) };
        prometheus::simpleapi::counter_metric_t pkt_multicast_gather
        { packets.Add({{"packet_type", "multicast_gather"}}) };
        prometheus::simpleapi::counter_metric_t pkt_multicast_frame
        { packets.Add({{"packet_type", "multicast_frame"}}) };
        prometheus::simpleapi::counter_metric_t pkt_push_direct_paths
        { packets.Add({{"packet_type", "push_direct_paths"}}) };
        prometheus::simpleapi::counter_metric_t pkt_user_message
        { packets.Add({{"packet_type", "user_message"}}) };
        prometheus::simpleapi::counter_metric_t pkt_remote_trace
        { packets.Add({{"packet_type", "remote_trace"}}) };
        prometheus::simpleapi::counter_metric_t pkt_path_negotiation_request
        { packets.Add({{"packet_type", "path_negotiation_request"}}) };

        // Packet Error Counts
        prometheus::simpleapi::counter_family_t packet_errors
        { "zt_packet_incoming_error", "incoming packet errors"};
        prometheus::simpleapi::counter_metric_t pkt_error_obj_not_found
        { packet_errors.Add({{"error_type", "obj_not_found"}}) };
        prometheus::simpleapi::counter_metric_t pkt_error_unsupported_op
        { packet_errors.Add({{"error_type", "unsupported_operation"}}) };
        prometheus::simpleapi::counter_metric_t pkt_error_identity_collision
        { packet_errors.Add({{"error_type", "identity_collision"}}) };
        prometheus::simpleapi::counter_metric_t pkt_error_need_membership_cert
        { packet_errors.Add({{"error_type", "need_membership_certificate"}}) };
        prometheus::simpleapi::counter_metric_t pkt_error_network_access_denied
        { packet_errors.Add({{"error_type", "network_access_denied"}}) };
        prometheus::simpleapi::counter_metric_t pkt_error_unwanted_multicast
        { packet_errors.Add({{"error_type", "unwanted_multicast"}}) };
        prometheus::simpleapi::counter_metric_t pkt_error_authentication_required
        { packet_errors.Add({{"error_type", "authentication_required"}}) };

        // Data Sent/Received Metrics
        prometheus::simpleapi::counter_metric_t udp_send
        { "zt_udp_data_sent", "number of bytes ZeroTier has sent via UDP" };
        prometheus::simpleapi::counter_metric_t udp_recv
        { "zt_udp_data_recv", "number of bytes ZeroTier has received via UDP" };
        prometheus::simpleapi::counter_metric_t tcp_send
        { "zt_tcp_data_sent", "number of bytes ZeroTier has sent via TCP" };
        prometheus::simpleapi::counter_metric_t tcp_recv
        { "zt_tcp_data_recv", "number of bytes ZeroTier has received via TCP" };

        // General Controller Metrics
        prometheus::simpleapi::gauge_metric_t   network_count
        {"controller_network_count", "number of networks the controller is serving"};
        prometheus::simpleapi::gauge_metric_t   member_count
        {"controller_member_count", "number of network members the controller is serving"};
        prometheus::simpleapi::counter_metric_t network_changes
        {"controller_network_change_count", "number of times a network configuration is changed"};
        prometheus::simpleapi::counter_metric_t member_changes
        {"controller_member_change_count", "number of times a network member configuration is changed"};
        prometheus::simpleapi::counter_metric_t member_auths
        {"controller_member_auth_count", "number of network member auths"};
        prometheus::simpleapi::counter_metric_t member_deauths
        {"controller_member_deauth_count", "number of network member deauths"};

#ifdef ZT_CONTROLLER_USE_LIBPQ
        // Central Controller Metrics
        prometheus::simpleapi::counter_metric_t pgsql_mem_notification
        { "controller_pgsql_member_notifications_received", "number of member change notifications received via pgsql NOTIFY" };
        prometheus::simpleapi::counter_metric_t pgsql_net_notification
        { "controller_pgsql_network_notifications_received", "number of network change notifications received via pgsql NOTIFY" };
        prometheus::simpleapi::counter_metric_t pgsql_node_checkin
        { "controller_pgsql_node_checkin_count", "number of node check-ins (pgsql)" };
        prometheus::simpleapi::counter_metric_t redis_mem_notification
        { "controller_redis_member_notifications_received", "number of member change notifications received via redis" };
        prometheus::simpleapi::counter_metric_t redis_net_notification
        { "controller_redis_network_notifications_received", "number of network change notifications received via redis" };
        prometheus::simpleapi::counter_metric_t redis_node_checkin
        { "controller_redis_node_checkin_count", "number of node check-ins (redis)" };

        // Central DB Pool Metrics
        prometheus::simpleapi::counter_metric_t conn_counter
        { "controller_pgsql_connections_created", "number of pgsql connections created"};
        prometheus::simpleapi::counter_metric_t max_pool_size
        { "controller_pgsql_max_conn_pool_size", "max connection pool size for postgres"};
        prometheus::simpleapi::counter_metric_t min_pool_size
        { "controller_pgsql_min_conn_pool_size", "minimum connection pool size for postgres" };
        prometheus::simpleapi::gauge_metric_t   pool_avail
        { "controller_pgsql_available_conns", "number of available postgres connections" };
        prometheus::simpleapi::gauge_metric_t   pool_in_use
        { "controller_pgsql_in_use_conns", "number of postgres database connections in use" };
        prometheus::simpleapi::counter_metric_t pool_errors
        { "controller_pgsql_connection_errors", "number of connection errors the connection pool has seen" };
#endif
    }
}
