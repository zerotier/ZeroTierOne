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
        prometheus::simpleapi::counter_metric_t redis_mem_notification
        { "controller_redis_member_notifications_received", "number of member change notifications received via redis" };
        prometheus::simpleapi::counter_metric_t redis_net_notification
        { "controller_redis_network_notifications_received", "number of network change notifications received via redis" };

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
