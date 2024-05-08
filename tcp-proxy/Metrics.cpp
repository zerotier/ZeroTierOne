#include <prometheus/simpleapi.h>


namespace ZeroTier {
    namespace Metrics {
        prometheus::simpleapi::gauge_metric_t tcp_connections
        {"tcp_connections", "TCP connections"};

        prometheus::simpleapi::counter_metric_t udp_open_failed
        {"udp_open_failed", "UDP open failed"};

        prometheus::simpleapi::counter_metric_t tcp_opened
        {"tcp_opened", "TCP opened"};
        prometheus::simpleapi::counter_metric_t tcp_closed
        {"tcp_closed", "TCP closed"};

        prometheus::simpleapi::counter_metric_t tcp_bytes_in
        {"tcp_byes_in", "TCP bytes in"};
        prometheus::simpleapi::counter_metric_t tcp_bytes_out
        {"tcp_byes_out", "TCP bytes out"};

        prometheus::simpleapi::counter_metric_t udp_bytes_in
        {"udp_bytes_in", "UDP bytes in"};
        prometheus::simpleapi::counter_metric_t udp_bytes_out
        {"udp_bytes_out", "UDP bytes out"};
    }
}