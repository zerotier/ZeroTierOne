#ifndef _TCP_PROXY_METRICS_H_
#define _TCP_PROXY_METRICS_H_

#include "../node/Metrics.hpp"

namespace ZeroTier {
    namespace Metrics {
        extern prometheus::simpleapi::counter_metric_t udp_open_failed;

        extern prometheus::simpleapi::counter_metric_t tcp_opened;
        extern prometheus::simpleapi::counter_metric_t tcp_closed;

        extern prometheus::simpleapi::counter_metric_t tcp_bytes_in;
        extern prometheus::simpleapi::counter_metric_t tcp_bytes_out;

        extern prometheus::simpleapi::counter_metric_t udp_bytes_in;
        extern prometheus::simpleapi::counter_metric_t udp_bytes_out;
    }
}

#endif // _TCP_PROXY_METRICS_H_