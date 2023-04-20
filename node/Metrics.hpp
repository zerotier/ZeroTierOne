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
        // General Controller Metrics
        static prometheus::simpleapi::gauge_metric_t   network_count;
        extern prometheus::simpleapi::gauge_metric_t   member_count;
        extern prometheus::simpleapi::counter_metric_t network_changes;
        extern prometheus::simpleapi::counter_metric_t member_changes;
        extern prometheus::simpleapi::counter_metric_t member_auths;
        extern prometheus::simpleapi::counter_metric_t member_deauths;

#ifdef ZT_CONTROLLER_USE_LIBPQ
        // Central Controller Metrics
        extern prometheus::simpleapi::counter_metric_t pgsql_mem_notification;
        extern prometheus::simpleapi::counter_metric_t pgsql_net_notification;
        extern prometheus::simpleapi::counter_metric_t redis_mem_notification;
        extern prometheus::simpleapi::counter_metric_t redis_net_notification;

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
