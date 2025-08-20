// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include "grpcpp/grpcpp.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_exporter.h"

#include "opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_factory.h"
#include "opentelemetry/metrics/provider.h"
#include "opentelemetry/sdk/metrics/aggregation/default_aggregation.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h"
#include "opentelemetry/sdk/metrics/meter.h"
#include "opentelemetry/sdk/metrics/meter_context_factory.h"
#include "opentelemetry/sdk/metrics/meter_provider.h"
#include "opentelemetry/sdk/metrics/meter_provider_factory.h"
#include "opentelemetry/sdk/metrics/provider.h"
#include "opentelemetry/sdk/metrics/view/instrument_selector_factory.h"
#include "opentelemetry/sdk/metrics/view/meter_selector_factory.h"
#include "opentelemetry/sdk/metrics/view/view_factory.h"

#include <memory>
#include <thread>

#ifdef BAZEL_BUILD
#  include "examples/common/metrics_foo_library/foo_library.h"
#else
#  include "metrics_foo_library/foo_library.h"
#endif

namespace metric_sdk    = opentelemetry::sdk::metrics;
namespace common        = opentelemetry::common;
namespace metrics_api   = opentelemetry::metrics;
namespace otlp_exporter = opentelemetry::exporter::otlp;

namespace
{

otlp_exporter::OtlpGrpcMetricExporterOptions exporter_options;

void InitMetrics(std::string &name)
{
  auto exporter = otlp_exporter::OtlpGrpcMetricExporterFactory::Create(exporter_options);

  std::string version{"1.2.0"};
  std::string schema{"https://opentelemetry.io/schemas/1.2.0"};

  // Initialize and set the global MeterProvider
  metric_sdk::PeriodicExportingMetricReaderOptions reader_options;
  reader_options.export_interval_millis = std::chrono::milliseconds(1000);
  reader_options.export_timeout_millis  = std::chrono::milliseconds(500);

  auto reader =
      metric_sdk::PeriodicExportingMetricReaderFactory::Create(std::move(exporter), reader_options);

  auto context = metric_sdk::MeterContextFactory::Create();
  context->AddMetricReader(std::move(reader));

  auto provider = metric_sdk::MeterProviderFactory::Create(std::move(context));

  // histogram view
  std::string histogram_name = name + "_exponential_histogram";
  std::string unit           = "histogram-unit";

  auto histogram_instrument_selector = metric_sdk::InstrumentSelectorFactory::Create(
      metric_sdk::InstrumentType::kHistogram, histogram_name, unit);

  auto histogram_meter_selector = metric_sdk::MeterSelectorFactory::Create(name, version, schema);

  auto histogram_aggregation_config =
      std::unique_ptr<metric_sdk::Base2ExponentialHistogramAggregationConfig>(
          new metric_sdk::Base2ExponentialHistogramAggregationConfig);

  std::shared_ptr<metric_sdk::AggregationConfig> aggregation_config(
      std::move(histogram_aggregation_config));

  auto histogram_view = metric_sdk::ViewFactory::Create(
      name, "des", unit, metric_sdk::AggregationType::kBase2ExponentialHistogram,
      aggregation_config);

  provider->AddView(std::move(histogram_instrument_selector), std::move(histogram_meter_selector),
                    std::move(histogram_view));

  std::shared_ptr<opentelemetry::metrics::MeterProvider> api_provider(std::move(provider));

  metric_sdk::Provider::SetMeterProvider(api_provider);
}

void CleanupMetrics()
{
  std::shared_ptr<metrics_api::MeterProvider> none;
  metric_sdk::Provider::SetMeterProvider(none);
}
}  // namespace

int main(int argc, char *argv[])
{
  std::string example_type;
  if (argc > 1)
  {
    exporter_options.endpoint = argv[1];
    if (argc > 2)
    {
      example_type = argv[2];
      if (argc > 3)
      {
        exporter_options.use_ssl_credentials         = true;
        exporter_options.ssl_credentials_cacert_path = argv[3];
      }
    }
  }
  std::cout << "Using endpoint: " << exporter_options.endpoint << std::endl;
  std::cout << "Using example type: " << example_type << std::endl;
  std::cout << "Using cacert path: " << exporter_options.ssl_credentials_cacert_path << std::endl;
  std::cout << "Using ssl credentials: " << exporter_options.use_ssl_credentials << std::endl;

  // Removing this line will leave the default noop MetricProvider in place.

  std::string name{"otlp_grpc_metric_example"};

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
  else
  {
    std::thread counter_example{&foo_library::counter_example, name};
    std::thread observable_counter_example{&foo_library::observable_counter_example, name};
    std::thread histogram_example{&foo_library::histogram_example, name};
    std::thread histogram_exp_example{&foo_library::histogram_exp_example, name};
#if OPENTELEMETRY_ABI_VERSION_NO >= 2
    std::thread gauge_example{&foo_library::gauge_example, name};
#endif

    counter_example.join();
    observable_counter_example.join();
    histogram_example.join();
    histogram_exp_example.join();
#if OPENTELEMETRY_ABI_VERSION_NO >= 2
    gauge_example.join();
#endif
  }

  CleanupMetrics();
  return 0;
}
