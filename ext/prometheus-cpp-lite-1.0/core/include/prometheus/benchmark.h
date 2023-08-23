#pragma once

#include "prometheus/metric.h"
#include "prometheus/family.h"

#include <chrono>

namespace prometheus {

  class Benchmark : public Metric {

    #ifndef NDEBUG
      bool already_started = false;
    #endif

    std::chrono::time_point<std::chrono::high_resolution_clock>           start_;
    std::chrono::time_point<std::chrono::high_resolution_clock>::duration elapsed = std::chrono::time_point<std::chrono::high_resolution_clock>::duration::zero(); // elapsed time

  public:

    using Value  = double;
    using Family = CustomFamily<Benchmark>;

    static const Metric::Type static_type = Metric::Type::Counter;

    Benchmark() : Metric(Metric::Type::Counter) {}

    void start() {

      #ifndef NDEBUG
        if (already_started)
          throw std::runtime_error("try to start already started counter");
        else
          already_started = true;
      #endif

      start_ = std::chrono::high_resolution_clock::now();

    }

    void stop() {

      #ifndef NDEBUG
        if (already_started == false)
          throw std::runtime_error("try to stop already stoped counter");
      #endif

      std::chrono::time_point<std::chrono::high_resolution_clock> stop;
      stop = std::chrono::high_resolution_clock::now();
      elapsed += stop - start_;

      #ifndef NDEBUG
        already_started = false;
      #endif

    }

    double Get() const {
      return std::chrono::duration_cast<std::chrono::duration<double>>(elapsed).count();
    }

    virtual ClientMetric Collect() const {
      ClientMetric metric;
      metric.counter.value = Get();
      return metric;
    }

  };



}  // namespace prometheus
