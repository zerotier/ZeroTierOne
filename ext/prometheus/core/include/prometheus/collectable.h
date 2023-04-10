#pragma once

#include <vector>

#include "prometheus/metric_family.h"

namespace prometheus {

  /// @brief Interface implemented by anything that can be used by Prometheus to
  /// collect metrics.
  ///
  /// A Collectable has to be registered for collection. See Registry.
  class Collectable {

    public:

      //Collectable() = default;

      virtual ~Collectable() = default;

      using MetricFamilies = std::vector<MetricFamily>;

      /// \brief Returns a list of metrics and their samples.
      virtual MetricFamilies Collect() const = 0;
  };

}  // namespace prometheus
