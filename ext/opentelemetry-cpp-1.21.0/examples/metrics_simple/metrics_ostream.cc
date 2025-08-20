// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "opentelemetry/common/attribute_value.h"
#include "opentelemetry/exporters/ostream/metric_exporter_factory.h"
#include "opentelemetry/metrics/meter_provider.h"
#include "opentelemetry/sdk/metrics/aggregation/aggregation_config.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_options.h"
#include "opentelemetry/sdk/metrics/instruments.h"
#include "opentelemetry/sdk/metrics/meter_context.h"
#include "opentelemetry/sdk/metrics/meter_context_factory.h"
#include "opentelemetry/sdk/metrics/meter_provider.h"
#include "opentelemetry/sdk/metrics/meter_provider_factory.h"
#include "opentelemetry/sdk/metrics/metric_reader.h"
#include "opentelemetry/sdk/metrics/provider.h"
#include "opentelemetry/sdk/metrics/push_metric_exporter.h"
#include "opentelemetry/sdk/metrics/view/instrument_selector.h"
#include "opentelemetry/sdk/metrics/view/instrument_selector_factory.h"
#include "opentelemetry/sdk/metrics/view/meter_selector.h"
#include "opentelemetry/sdk/metrics/view/meter_selector_factory.h"
#include "opentelemetry/sdk/metrics/view/view.h"
#include "opentelemetry/sdk/metrics/view/view_factory.h"

#ifdef BAZEL_BUILD
#  include "examples/common/metrics_foo_library/foo_library.h"
#else
#  include "metrics_foo_library/foo_library.h"
#endif

namespace metrics_sdk     = opentelemetry::sdk::metrics;
namespace common          = opentelemetry::common;
namespace exportermetrics = opentelemetry::exporter::metrics;
namespace metrics_api     = opentelemetry::metrics;

namespace
{

void InitMetrics(const std::string &name)
{
  auto exporter = exportermetrics::OStreamMetricExporterFactory::Create();

  std::string version{"1.2.0"};
  std::string schema{"https://opentelemetry.io/schemas/1.2.0"};

  // Initialize and set the global MeterProvider
  metrics_sdk::PeriodicExportingMetricReaderOptions options;
  options.export_interval_millis = std::chrono::milliseconds(1000);
  options.export_timeout_millis  = std::chrono::milliseconds(500);

  auto reader =
      metrics_sdk::PeriodicExportingMetricReaderFactory::Create(std::move(exporter), options);

  auto context = metrics_sdk::MeterContextFactory::Create();
  context->AddMetricReader(std::move(reader));
  auto provider = opentelemetry::sdk::metrics::MeterProviderFactory::Create(std::move(context));

  // counter view
  std::string counter_name = name + "_counter";
  std::string unit         = "counter-unit";

  auto instrument_selector = metrics_sdk::InstrumentSelectorFactory::Create(
      metrics_sdk::InstrumentType::kCounter, counter_name, unit);

  auto meter_selector = metrics_sdk::MeterSelectorFactory::Create(name, version, schema);

  auto sum_view = metrics_sdk::ViewFactory::Create(name, "description", unit,
                                                   metrics_sdk::AggregationType::kSum);

  provider->AddView(std::move(instrument_selector), std::move(meter_selector), std::move(sum_view));

  // observable counter view
  std::string observable_counter_name = name + "_observable_counter";

  auto observable_instrument_selector = metrics_sdk::InstrumentSelectorFactory::Create(
      metrics_sdk::InstrumentType::kObservableCounter, observable_counter_name, unit);

  auto observable_meter_selector = metrics_sdk::MeterSelectorFactory::Create(name, version, schema);

  auto observable_sum_view = metrics_sdk::ViewFactory::Create(name, "test_description", unit,
                                                              metrics_sdk::AggregationType::kSum);

  provider->AddView(std::move(observable_instrument_selector), std::move(observable_meter_selector),
                    std::move(observable_sum_view));

  // histogram view
  std::string histogram_name = name + "_histogram";
  unit                       = "histogram-unit";

  auto histogram_instrument_selector = metrics_sdk::InstrumentSelectorFactory::Create(
      metrics_sdk::InstrumentType::kHistogram, histogram_name, unit);

  auto histogram_meter_selector = metrics_sdk::MeterSelectorFactory::Create(name, version, schema);

  auto histogram_aggregation_config = std::unique_ptr<metrics_sdk::HistogramAggregationConfig>(
      new metrics_sdk::HistogramAggregationConfig);

  histogram_aggregation_config->boundaries_ = std::vector<double>{
      0.0, 50.0, 100.0, 250.0, 500.0, 750.0, 1000.0, 2500.0, 5000.0, 10000.0, 20000.0};

  std::shared_ptr<metrics_sdk::AggregationConfig> aggregation_config(
      std::move(histogram_aggregation_config));

  auto histogram_view = metrics_sdk::ViewFactory::Create(
      name, "description", unit, metrics_sdk::AggregationType::kHistogram, aggregation_config);

  provider->AddView(std::move(histogram_instrument_selector), std::move(histogram_meter_selector),
                    std::move(histogram_view));

  // hisogram view with base2 exponential aggregation
  std::string histogram_base2_name         = name + "_exponential_histogram";
  unit                                     = "histogram-unit";
  auto histogram_base2_instrument_selector = metrics_sdk::InstrumentSelectorFactory::Create(
      metrics_sdk::InstrumentType::kHistogram, histogram_base2_name, unit);
  auto histogram_base2_meter_selector =
      metrics_sdk::MeterSelectorFactory::Create(name, version, schema);
  auto histogram_base2_aggregation_config =
      std::unique_ptr<metrics_sdk::Base2ExponentialHistogramAggregationConfig>(
          new metrics_sdk::Base2ExponentialHistogramAggregationConfig);
  histogram_base2_aggregation_config->max_scale_      = 3;
  histogram_base2_aggregation_config->record_min_max_ = true;
  histogram_base2_aggregation_config->max_buckets_    = 100;

  std::shared_ptr<metrics_sdk::AggregationConfig> base2_aggregation_config(
      std::move(histogram_base2_aggregation_config));

  auto histogram_base2_view = metrics_sdk::ViewFactory::Create(
      name, "description", unit, metrics_sdk::AggregationType::kBase2ExponentialHistogram,
      base2_aggregation_config);

  provider->AddView(std::move(histogram_base2_instrument_selector),
                    std::move(histogram_base2_meter_selector), std::move(histogram_base2_view));

  std::shared_ptr<opentelemetry::metrics::MeterProvider> api_provider(std::move(provider));

  metrics_sdk::Provider::SetMeterProvider(api_provider);
}

void CleanupMetrics()
{
  std::shared_ptr<metrics_api::MeterProvider> none;
  metrics_sdk::Provider::SetMeterProvider(none);
}
}  // namespace

int main(int argc, char **argv)
{
  std::string example_type;
  if (argc >= 2)
  {
    example_type = argv[1];
  }

  std::string name{"ostream_metric_example"};
  InitMetrics(name);

  if (example_type == "counter")
  {
    foo_library::counter_example(name);
  }
  else if (example_type == "observable_counter")
  {
    foo_library::observable_counter_example(name);
  }
  else if (example_type == "histogram")
  {
    foo_library::histogram_example(name);
  }
  else if (example_type == "exponential_histogram")
  {
    foo_library::histogram_exp_example(name);
  }
#if OPENTELEMETRY_ABI_VERSION_NO >= 2
  else if (example_type == "gauge")
  {
    foo_library::gauge_example(name);
  }
#endif
  else if (example_type == "semconv_counter")
  {
    foo_library::semconv_counter_example();
  }
  else if (example_type == "semconv_observable_counter")
  {
    foo_library::semconv_observable_counter_example();
  }
  else if (example_type == "semconv_histogram")
  {
    foo_library::semconv_histogram_example();
  }
  else
  {
    std::thread counter_example{&foo_library::counter_example, name};
    std::thread observable_counter_example{&foo_library::observable_counter_example, name};
    std::thread histogram_example{&foo_library::histogram_example, name};
    std::thread histogram_exp_example{&foo_library::histogram_exp_example, name};
#if OPENTELEMETRY_ABI_VERSION_NO >= 2
    std::thread gauge_example{&foo_library::gauge_example, name};
#endif
    std::thread semconv_counter_example{&foo_library::semconv_counter_example};
    std::thread semconv_observable_counter_example{
        &foo_library::semconv_observable_counter_example};
    std::thread semconv_histogram_example{&foo_library::semconv_histogram_example};

    counter_example.join();
    observable_counter_example.join();
    histogram_example.join();
    histogram_exp_example.join();
#if OPENTELEMETRY_ABI_VERSION_NO >= 2
    gauge_example.join();
#endif
    semconv_counter_example.join();
    semconv_observable_counter_example.join();
    semconv_histogram_example.join();
  }

  CleanupMetrics();
  return 0;
}
