
#pragma once

#include <prometheus/family.h>
#include <prometheus/registry.h>
#include <prometheus/counter.h>
#include <prometheus/gauge.h>
#include <prometheus/benchmark.h>

#include <prometheus/registry.h>
#include <prometheus/save_to_file.h>

#include <thread>
#include <iostream>
#include <memory>
#include <functional>

namespace prometheus {
  namespace simpleapi {

    extern Registry&  registry;
    extern SaveToFile saver;


    template <typename CustomWrapper>
    class family_wrapper_t {

      typename CustomWrapper::Family* family_{ nullptr };

    public:

      // make new family: family_t family {"family_name", "Family description"}
      family_wrapper_t(const std::string& name, const std::string& description)
        : family_(&CustomWrapper::Family::Build(registry, name, description)) {}

      // make new metric into existing family: metric_t metric {family.Add({{"tag_name", "tag_value"}})}
      CustomWrapper Add(const typename CustomWrapper::Family::Labels& labels) {
        return CustomWrapper(family_, family_->Add(labels));
      }

    };


    class counter_metric_t {

    public:

      using Metric = Counter<uint64_t>;
      using Family = Metric::Family;

    private:

      Family*  family_ { nullptr };
      Metric*  metric_ { nullptr };

      friend family_wrapper_t<counter_metric_t>;
      counter_metric_t(typename Metric::Family* family, Metric& metric)
        : family_(family), metric_(&metric) {}

    public:

      // fake empty metric
      counter_metric_t() = default;

      // make new counter as simple metric without tags and with hidden family included: metric_t metric {"counter_name", "Counter description"}
      counter_metric_t(const std::string& name, const std::string& description)
        : family_(&Metric::Family::Build(registry, name, description)), metric_(&family_->Add({})) {}

      void operator++ ()                           { metric_->Increment();    }
      void operator++ (int)                        { metric_->Increment();    }
      void operator+= (typename Metric::Value val) { metric_->Increment(val); }

      uint64_t value() const { return metric_->Get(); }

    };

    using counter_family_t = family_wrapper_t<counter_metric_t>;


    class gauge_metric_t {

    public:

      using Metric = Gauge<int64_t>;
      using Family = Metric::Family;

    private:

      Family*  family_ { nullptr };
      Metric*  metric_ { nullptr };

      friend family_wrapper_t<gauge_metric_t>;
      gauge_metric_t(typename Metric::Family* family, Metric& metric)
        : family_(family), metric_(&metric) {}

    public:

      // fake empty metric
      gauge_metric_t() = default;

      // make new gauge as simple metric without tags and with hidden family included: metric {"counter_name", "Counter description"}
      gauge_metric_t(const std::string& name, const std::string& description)
        : family_(&Metric::Family::Build(registry, name, description)), metric_(&family_->Add({})) {}

      void operator++ ()                           { metric_->Increment();    }
      void operator++ (int)                        { metric_->Increment();    }
      void operator+= (typename Metric::Value val) { metric_->Increment(val); }

      void operator-- ()                           { metric_->Decrement();    }
      void operator-- (int)                        { metric_->Decrement();    }
      void operator-= (typename Metric::Value val) { metric_->Decrement(val); }
      void operator=  (typename Metric::Value val) { metric_->Set(val);       }

      int64_t value() const { return metric_->Get(); }

    };

    using gauge_family_t = family_wrapper_t<gauge_metric_t>;


    class benchmark_metric_t {

    public:

      using Metric = Benchmark;
      using Family = Metric::Family;

    private:

      Family*  family_ { nullptr };
      Metric*  metric_ { nullptr };

      friend family_wrapper_t<benchmark_metric_t>;
      benchmark_metric_t(typename Metric::Family* family, Metric& metric)
        : family_(family), metric_(&metric) {}

    public:

      // fake empty metric
      benchmark_metric_t() = default;

      // make new benchmark as simple metric without tags and with hidden family included: metric {"counter_name", "Counter description"}
      benchmark_metric_t(const std::string& name, const std::string& description)
        : family_(&Metric::Family::Build(registry, name, description)), metric_(&family_->Add({})) {}

      void start() { metric_->start(); }
      void stop()  { metric_->stop();  }

      double value() const { return metric_->Get(); }

    };

    using benchmark_family_t = family_wrapper_t<benchmark_metric_t>;

  }
}
