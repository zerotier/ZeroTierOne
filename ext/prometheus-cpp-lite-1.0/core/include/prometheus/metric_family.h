#pragma once

#include <string>
#include <vector>

#include "metric.h"
#include "prometheus/client_metric.h"

namespace prometheus {

  struct MetricFamily {
    Metric::Type              type;
    std::string               name;
    std::string               help;
    std::vector<ClientMetric> metric;
  };

}  // namespace prometheus
