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
#include <prometheus/histogram.h>

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
        { "zt_packet", "incoming packet type counts"};

        // Incoming packets
        prometheus::simpleapi::counter_metric_t pkt_nop_in
        { packets.Add({{"packet_type", "nop"}, {"direction", "rx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_error_in
        { packets.Add({{"packet_type", "error"}, {"direction", "rx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_ack_in
        { packets.Add({{"packet_type", "ack"}, {"direction", "rx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_qos_in
        { packets.Add({{"packet_type", "qos"}, {"direction", "rx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_hello_in
        { packets.Add({{"packet_type", "hello"}, {"direction", "rx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_ok_in
        { packets.Add({{"packet_type", "ok"}, {"direction", "rx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_whois_in
        { packets.Add({{"packet_type", "whois"}, {"direction", "rx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_rendezvous_in
        { packets.Add({{"packet_type", "rendezvous"}, {"direction", "rx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_frame_in
        { packets.Add({{"packet_type", "frame"}, {"direction", "rx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_ext_frame_in
        { packets.Add({{"packet_type", "ext_frame"}, {"direction", "rx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_echo_in
        { packets.Add({{"packet_type", "echo"}, {"direction", "rx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_multicast_like_in
        { packets.Add({{"packet_type", "multicast_like"}, {"direction", "rx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_network_credentials_in
        { packets.Add({{"packet_type", "network_credentials"}, {"direction", "rx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_network_config_request_in
        { packets.Add({{"packet_type", "network_config_request"}, {"direction", "rx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_network_config_in
        { packets.Add({{"packet_type", "network_config"}, {"direction", "rx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_multicast_gather_in
        { packets.Add({{"packet_type", "multicast_gather"}, {"direction", "rx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_multicast_frame_in
        { packets.Add({{"packet_type", "multicast_frame"}, {"direction", "rx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_push_direct_paths_in
        { packets.Add({{"packet_type", "push_direct_paths"}, {"direction", "rx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_user_message_in
        { packets.Add({{"packet_type", "user_message"}, {"direction", "rx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_remote_trace_in
        { packets.Add({{"packet_type", "remote_trace"}, {"direction", "rx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_path_negotiation_request_in
        { packets.Add({{"packet_type", "path_negotiation_request"}, {"direction", "rx"}}) };

        // Outgoing packets
        prometheus::simpleapi::counter_metric_t pkt_nop_out
        { packets.Add({{"packet_type", "nop"}, {"direction", "tx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_error_out
        { packets.Add({{"packet_type", "error"}, {"direction", "tx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_ack_out
        { packets.Add({{"packet_type", "ack"}, {"direction", "tx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_qos_out
        { packets.Add({{"packet_type", "qos"}, {"direction", "tx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_hello_out
        { packets.Add({{"packet_type", "hello"}, {"direction", "tx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_ok_out
        { packets.Add({{"packet_type", "ok"}, {"direction", "tx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_whois_out
        { packets.Add({{"packet_type", "whois"}, {"direction", "tx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_rendezvous_out
        { packets.Add({{"packet_type", "rendezvous"}, {"direction", "tx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_frame_out
        { packets.Add({{"packet_type", "frame"}, {"direction", "tx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_ext_frame_out
        { packets.Add({{"packet_type", "ext_frame"}, {"direction", "tx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_echo_out
        { packets.Add({{"packet_type", "echo"}, {"direction", "tx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_multicast_like_out
        { packets.Add({{"packet_type", "multicast_like"}, {"direction", "tx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_network_credentials_out
        { packets.Add({{"packet_type", "network_credentials"}, {"direction", "tx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_network_config_request_out
        { packets.Add({{"packet_type", "network_config_request"}, {"direction", "tx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_network_config_out
        { packets.Add({{"packet_type", "network_config"}, {"direction", "tx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_multicast_gather_out
        { packets.Add({{"packet_type", "multicast_gather"}, {"direction", "tx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_multicast_frame_out
        { packets.Add({{"packet_type", "multicast_frame"}, {"direction", "tx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_push_direct_paths_out
        { packets.Add({{"packet_type", "push_direct_paths"}, {"direction", "tx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_user_message_out
        { packets.Add({{"packet_type", "user_message"}, {"direction", "tx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_remote_trace_out
        { packets.Add({{"packet_type", "remote_trace"}, {"direction", "tx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_path_negotiation_request_out
        { packets.Add({{"packet_type", "path_negotiation_request"}, {"direction", "tx"}}) };


        // Packet Error Counts
        prometheus::simpleapi::counter_family_t packet_errors
        { "zt_packet_error", "incoming packet errors"};

        // Incoming Error Counts
        prometheus::simpleapi::counter_metric_t pkt_error_obj_not_found_in
        { packet_errors.Add({{"error_type", "obj_not_found"}, {"direction", "rx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_error_unsupported_op_in
        { packet_errors.Add({{"error_type", "unsupported_operation"}, {"direction", "rx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_error_identity_collision_in
        { packet_errors.Add({{"error_type", "identity_collision"}, {"direction", "rx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_error_need_membership_cert_in
        { packet_errors.Add({{"error_type", "need_membership_certificate"}, {"direction", "rx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_error_network_access_denied_in
        { packet_errors.Add({{"error_type", "network_access_denied"}, {"direction", "rx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_error_unwanted_multicast_in
        { packet_errors.Add({{"error_type", "unwanted_multicast"}, {"direction", "rx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_error_authentication_required_in
        { packet_errors.Add({{"error_type", "authentication_required"}, {"direction", "rx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_error_internal_server_error_in
        { packet_errors.Add({{"error_type", "internal_server_error"}, {"direction", "rx"}}) };

        // Outgoing Error Counts
        prometheus::simpleapi::counter_metric_t pkt_error_obj_not_found_out
        { packet_errors.Add({{"error_type", "obj_not_found"}, {"direction", "tx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_error_unsupported_op_out
        { packet_errors.Add({{"error_type", "unsupported_operation"}, {"direction", "tx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_error_identity_collision_out
        { packet_errors.Add({{"error_type", "identity_collision"}, {"direction", "tx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_error_need_membership_cert_out
        { packet_errors.Add({{"error_type", "need_membership_certificate"}, {"direction", "tx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_error_network_access_denied_out
        { packet_errors.Add({{"error_type", "network_access_denied"}, {"direction", "tx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_error_unwanted_multicast_out
        { packet_errors.Add({{"error_type", "unwanted_multicast"}, {"direction", "tx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_error_authentication_required_out
        { packet_errors.Add({{"error_type", "authentication_required"}, {"direction", "tx"}}) };
        prometheus::simpleapi::counter_metric_t pkt_error_internal_server_error_out
        { packet_errors.Add({{"error_type", "internal_server_error"}, {"direction", "tx"}}) };

        // Data Sent/Received Metrics
        prometheus::simpleapi::counter_family_t data
        { "zt_data", "number of bytes ZeroTier has transmitted or received" };
        prometheus::simpleapi::counter_metric_t udp_recv
        { data.Add({{"protocol","udp"},{"direction","rx"}}) };
        prometheus::simpleapi::counter_metric_t udp_send
        { data.Add({{"protocol","udp"},{"direction","tx"}}) };
        prometheus::simpleapi::counter_metric_t tcp_send
        { data.Add({{"protocol","tcp"},{"direction", "tx"}}) };
        prometheus::simpleapi::counter_metric_t tcp_recv
        { data.Add({{"protocol","tcp"},{"direction", "rx"}}) };

        // Network Metrics
        prometheus::simpleapi::gauge_metric_t network_num_joined
        { "zt_num_networks", "number of networks this instance is joined to" };
        prometheus::simpleapi::gauge_family_t network_num_multicast_groups
        { "zt_network_multicast_groups_subscribed", "number of multicast groups networks are subscribed to" };
        prometheus::simpleapi::counter_family_t network_packets
        { "zt_network_packets", "number of incoming/outgoing packets per network" };
        
        // PeerMetrics
        prometheus::CustomFamily<prometheus::Histogram<uint64_t>> &peer_latency = 
        prometheus::Builder<prometheus::Histogram<uint64_t>>()
            .Name("zt_peer_latency")
            .Help("peer latency (ms)")
            .Register(prometheus::simpleapi::registry);
    
        prometheus::simpleapi::gauge_family_t peer_path_count
        { "zt_peer_path_count", "number of paths to peer" };
        prometheus::simpleapi::counter_family_t peer_packets
        { "zt_peer_packets", "number of packets to/from a peer" };
        prometheus::simpleapi::counter_family_t peer_packet_errors
        { "zt_peer_packet_errors" , "number of incoming packet errors from a peer" };

        // PeerMetrics
        prometheus::CustomFamily<prometheus::Histogram<uint64_t>> &peer_latency = 
        prometheus::Builder<prometheus::Histogram<uint64_t>>()
            .Name("zt_peer_latency")
            .Help("peer latency (ms)")
            .Register(prometheus::simpleapi::registry);
    
        prometheus::simpleapi::gauge_family_t peer_path_count
        { "zt_peer_path_count", "number of paths to peer" };
        prometheus::simpleapi::counter_family_t peer_incoming_packets
        { "zt_peer_incoming_packets", "number of incoming packets from a peer" };
        prometheus::simpleapi::counter_family_t peer_outgoing_packets
        { "zt_peer_outgoing_packets", "number of outgoing packets to a peer" };
        prometheus::simpleapi::counter_family_t peer_packet_errors
        { "zt_peer_packet_errors" , "number of incoming packet errors from a peer" };

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
