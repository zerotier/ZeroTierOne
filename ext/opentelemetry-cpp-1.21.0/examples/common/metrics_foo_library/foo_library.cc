// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>
#include <stdlib.h>
#include <chrono>
#include <cmath>
#include <map>
#include <thread>
#include <utility>
#include <vector>

#include "foo_library.h"
#include "opentelemetry/common/key_value_iterable_view.h"
#include "opentelemetry/context/context.h"
#include "opentelemetry/metrics/async_instruments.h"
#include "opentelemetry/metrics/meter.h"
#include "opentelemetry/metrics/meter_provider.h"
#include "opentelemetry/metrics/observer_result.h"
#include "opentelemetry/metrics/provider.h"
#include "opentelemetry/metrics/sync_instruments.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/variant.h"
#include "opentelemetry/semconv/http_metrics.h"
#include "opentelemetry/semconv/incubating/container_metrics.h"
#include "opentelemetry/semconv/incubating/system_metrics.h"

namespace metrics_api = opentelemetry::metrics;

namespace
{

static opentelemetry::nostd::shared_ptr<metrics_api::ObservableInstrument>
    double_observable_counter;

std::map<std::string, std::string> get_random_attr()
{
  const std::vector<std::pair<std::string, std::string>> labels = {{"key1", "value1"},
                                                                   {"key2", "value2"},
                                                                   {"key3", "value3"},
                                                                   {"key4", "value4"},
                                                                   {"key5", "value5"}};
  return std::map<std::string, std::string>{labels[rand() % (labels.size() - 1)],
                                            labels[rand() % (labels.size() - 1)]};
}

class MeasurementFetcher
{
public:
  static void Fetcher(opentelemetry::metrics::ObserverResult observer_result, void * /* state */)
  {
    if (opentelemetry::nostd::holds_alternative<
            opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<double>>>(
            observer_result))
    {
      double random_incr = (rand() % 5) + 1.1;
      value_ += random_incr;
      std::map<std::string, std::string> labels = get_random_attr();
      opentelemetry::nostd::get<
          opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<double>>>(
          observer_result)
          ->Observe(value_, labels);
    }
  }
  static double value_;
};
double MeasurementFetcher::value_ = 0.0;
}  // namespace

void foo_library::counter_example(const std::string &name)
{
  std::string counter_name = name + "_counter";
  auto provider            = metrics_api::Provider::GetMeterProvider();
  opentelemetry::nostd::shared_ptr<metrics_api::Meter> meter = provider->GetMeter(name, "1.2.0");
  auto double_counter = meter->CreateDoubleCounter(counter_name);

  for (uint32_t i = 0; i < 20; ++i)
  {
    double val = (rand() % 700) + 1.1;
    double_counter->Add(val);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }
}

void foo_library::observable_counter_example(const std::string &name)
{
  std::string counter_name = name + "_observable_counter";
  auto provider            = metrics_api::Provider::GetMeterProvider();
  opentelemetry::nostd::shared_ptr<metrics_api::Meter> meter = provider->GetMeter(name, "1.2.0");
  double_observable_counter = meter->CreateDoubleObservableCounter(counter_name);
  double_observable_counter->AddCallback(MeasurementFetcher::Fetcher, nullptr);
  for (uint32_t i = 0; i < 20; ++i)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }
}

void foo_library::histogram_example(const std::string &name)
{
  std::string histogram_name = name + "_histogram";
  auto provider              = metrics_api::Provider::GetMeterProvider();
  opentelemetry::nostd::shared_ptr<metrics_api::Meter> meter = provider->GetMeter(name, "1.2.0");
  auto histogram_counter = meter->CreateDoubleHistogram(histogram_name, "des", "histogram-unit");
  auto context           = opentelemetry::context::Context{};
  for (uint32_t i = 0; i < 20; ++i)
  {
    double val                                = (rand() % 700) + 1.1;
    std::map<std::string, std::string> labels = get_random_attr();
    auto labelkv = opentelemetry::common::KeyValueIterableView<decltype(labels)>{labels};
    histogram_counter->Record(val, labelkv, context);
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
  }
}

void foo_library::histogram_exp_example(const std::string &name)
{
  std::string histogram_name = name + "_exponential_histogram";
  auto provider              = metrics_api::Provider::GetMeterProvider();
  auto meter                 = provider->GetMeter(name, "1.2.0");
  auto histogram_counter = meter->CreateDoubleHistogram(histogram_name, "des", "histogram-unit");
  auto context           = opentelemetry::context::Context{};
  for (uint32_t i = 0; i < 20; ++i)
  {
    double val                                = (rand() % 700) + 1.1;
    std::map<std::string, std::string> labels = get_random_attr();
    auto labelkv = opentelemetry::common::KeyValueIterableView<decltype(labels)>{labels};
    histogram_counter->Record(val, labelkv, context);
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
  }
}

#if OPENTELEMETRY_ABI_VERSION_NO >= 2
void foo_library::gauge_example(const std::string &name)
{
  std::string gauge_name = name + "_gauge";
  auto provider          = metrics_api::Provider::GetMeterProvider();
  opentelemetry::nostd::shared_ptr<metrics_api::Meter> meter = provider->GetMeter(name, "1.2.0");
  auto gauge   = meter->CreateInt64Gauge(gauge_name, "des", "unit");
  auto context = opentelemetry::context::Context{};
  for (uint32_t i = 0; i < 20; ++i)
  {
    int64_t val                               = (rand() % 100) + 100;
    std::map<std::string, std::string> labels = get_random_attr();
    auto labelkv = opentelemetry::common::KeyValueIterableView<decltype(labels)>{labels};
    gauge->Record(val, labelkv, context);
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
  }
}
#endif

void foo_library::semconv_counter_example()
{
  auto provider = metrics_api::Provider::GetMeterProvider();
  opentelemetry::nostd::shared_ptr<metrics_api::Meter> meter = provider->GetMeter("demo", "1.2.0");
  auto double_counter =
      opentelemetry::semconv::container::CreateSyncDoubleMetricContainerDiskIo(meter.get());

  for (uint32_t i = 0; i < 20; ++i)
  {
    double val = (rand() % 700) + 1.1;
    double_counter->Add(val);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }
}

void foo_library::semconv_observable_counter_example()
{
  auto provider = metrics_api::Provider::GetMeterProvider();
  opentelemetry::nostd::shared_ptr<metrics_api::Meter> meter = provider->GetMeter("demo", "1.2.0");
  double_observable_counter =
      opentelemetry::semconv::system::CreateAsyncDoubleMetricSystemCpuTime(meter.get());
  double_observable_counter->AddCallback(MeasurementFetcher::Fetcher, nullptr);
  for (uint32_t i = 0; i < 20; ++i)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }
}

void foo_library::semconv_histogram_example()
{
  auto provider = metrics_api::Provider::GetMeterProvider();
  opentelemetry::nostd::shared_ptr<metrics_api::Meter> meter = provider->GetMeter("demo", "1.2.0");
  auto histogram_counter =
      opentelemetry::semconv::http::CreateSyncInt64MetricHttpClientRequestDuration(meter.get());
  auto context = opentelemetry::context::Context{};
  for (uint32_t i = 0; i < 20; ++i)
  {
    double val                                = (rand() % 700) + 1.1;
    uint64_t int_val                          = std::llround(val);
    std::map<std::string, std::string> labels = get_random_attr();
    auto labelkv = opentelemetry::common::KeyValueIterableView<decltype(labels)>{labels};
    histogram_counter->Record(int_val, labelkv, context);
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
  }
}
