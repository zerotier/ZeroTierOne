// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <utility>

#include "opentelemetry/common/attribute_value.h"
#include "opentelemetry/exporters/otlp/otlp_http.h"
#include "opentelemetry/exporters/otlp/otlp_http_metric_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_http_metric_exporter_options.h"
#include "opentelemetry/metrics/meter_provider.h"
#include "opentelemetry/sdk/common/global_log_handler.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_options.h"
#include "opentelemetry/sdk/metrics/meter_context.h"
#include "opentelemetry/sdk/metrics/meter_context_factory.h"
#include "opentelemetry/sdk/metrics/meter_provider.h"
#include "opentelemetry/sdk/metrics/meter_provider_factory.h"
#include "opentelemetry/sdk/metrics/metric_reader.h"
#include "opentelemetry/sdk/metrics/provider.h"
#include "opentelemetry/sdk/metrics/push_metric_exporter.h"

#ifdef BAZEL_BUILD
#  include "examples/common/metrics_foo_library/foo_library.h"
#else
#  include "metrics_foo_library/foo_library.h"
#endif

namespace metrics_sdk   = opentelemetry::sdk::metrics;
namespace common        = opentelemetry::common;
namespace metrics_api   = opentelemetry::metrics;
namespace otlp_exporter = opentelemetry::exporter::otlp;

namespace internal_log = opentelemetry::sdk::common::internal_log;

namespace
{

otlp_exporter::OtlpHttpMetricExporterOptions exporter_options;

void InitMetrics()
{
  auto exporter = otlp_exporter::OtlpHttpMetricExporterFactory::Create(exporter_options);

  std::string version{"1.2.0"};
  std::string schema{"https://opentelemetry.io/schemas/1.2.0"};

  // Initialize and set the global MeterProvider
  metrics_sdk::PeriodicExportingMetricReaderOptions reader_options;
  reader_options.export_interval_millis = std::chrono::milliseconds(1000);
  reader_options.export_timeout_millis  = std::chrono::milliseconds(500);

  auto reader = metrics_sdk::PeriodicExportingMetricReaderFactory::Create(std::move(exporter),
                                                                          reader_options);

  auto context = metrics_sdk::MeterContextFactory::Create();
  context->AddMetricReader(std::move(reader));

  auto u_provider = metrics_sdk::MeterProviderFactory::Create(std::move(context));
  std::shared_ptr<opentelemetry::metrics::MeterProvider> provider(std::move(u_provider));

  metrics_sdk::Provider::SetMeterProvider(provider);
}

void CleanupMetrics()
{
  std::shared_ptr<metrics_api::MeterProvider> none;
  metrics_sdk::Provider::SetMeterProvider(none);
}
}  // namespace

/*
  Usage:
  - example_otlp_http_metric
  - example_otlp_http_metric <URL>
  - example_otlp_http_metric <URL> <EXAMPLE>
  - example_otlp_http_metric <URL> <EXAMPLE> <DEBUG>
  - example_otlp_http_metric <URL> <EXAMPLE> <DEBUG> <BIN>
  <EXAMPLE> = counter|observable_counter|histogram|all
  <DEBUG> = yes|no, to turn console debug on or off
  <BIN> = bin, to export in binary format
*/
int main(int argc, char *argv[])
{
  std::string example_type;
  if (argc > 1)
  {
    exporter_options.url = argv[1];
  }

  if (argc > 2)
  {
    example_type = argv[2];
  }

  if (argc > 3)
  {
    std::string debug              = argv[3];
    exporter_options.console_debug = debug != "" && debug != "0" && debug != "no";
  }

  if (argc > 4)
  {
    std::string binary_mode = argv[4];
    if (binary_mode.size() >= 3 && binary_mode.substr(0, 3) == "bin")
    {
      exporter_options.content_type = otlp_exporter::HttpRequestContentType::kBinary;
    }
  }

  if (exporter_options.console_debug)
  {
    internal_log::GlobalLogHandler::SetLogLevel(internal_log::LogLevel::Debug);
  }

  // Removing this line will leave the default noop MetricProvider in place.
  InitMetrics();
  std::string name{"otlp_http_metric_example"};

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
#if OPENTELEMETRY_ABI_VERSION_NO >= 2
    std::thread gauge_example{&foo_library::gauge_example, name};
#endif

    counter_example.join();
    observable_counter_example.join();
    histogram_example.join();
#if OPENTELEMETRY_ABI_VERSION_NO >= 2
    gauge_example.join();
#endif
  }

  CleanupMetrics();
  return 0;
}
