#pragma once

#include <stdint.h>

#include "client_metric.h"

namespace prometheus {

  class Metric {

    public:
      enum class Type {
        Counter,
        Gauge,
        Summary,
        Histogram,
        Untyped,
      };

      Type type;

      Metric (Type type_) : type(type_) {}
      virtual ~Metric() = default;

      virtual ClientMetric Collect() const = 0;

  };

}  // namespace prometheus
