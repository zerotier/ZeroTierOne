// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <cstdio>
#include <memory>
#include <string>
#include <thread>
#include <utility>

#include "opentelemetry/common/attribute_value.h"
#include "opentelemetry/exporters/prometheus/exporter_factory.h"
#include "opentelemetry/exporters/prometheus/exporter_options.h"
#include "opentelemetry/metrics/meter_provider.h"
#include "opentelemetry/sdk/metrics/instruments.h"
#include "opentelemetry/sdk/metrics/meter_provider.h"
#include "opentelemetry/sdk/metrics/meter_provider_factory.h"
#include "opentelemetry/sdk/metrics/metric_reader.h"
#include "opentelemetry/sdk/metrics/provider.h"
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

namespace metrics_sdk      = opentelemetry::sdk::metrics;
namespace common           = opentelemetry::common;
namespace metrics_exporter = opentelemetry::exporter::metrics;
namespace metrics_api      = opentelemetry::metrics;

namespace
{

void InitMetrics(const std::string &name, const std::string &addr)
{
  metrics_exporter::PrometheusExporterOptions opts;
  if (!addr.empty())
  {
    opts.url = addr;
  }
  std::puts("PrometheusExporter example program running ...");

  std::string version{"1.2.0"};
  std::string schema{"https://opentelemetry.io/schemas/1.2.0"};

  auto prometheus_exporter = metrics_exporter::PrometheusExporterFactory::Create(opts);

  // Initialize and set the global MeterProvider
  auto u_provider = metrics_sdk::MeterProviderFactory::Create();
  auto *p         = static_cast<metrics_sdk::MeterProvider *>(u_provider.get());

  p->AddMetricReader(std::move(prometheus_exporter));

  // counter view
  std::string counter_name = name + "_counter";
  std::string counter_unit = "unit";

  auto instrument_selector = metrics_sdk::InstrumentSelectorFactory::Create(
      metrics_sdk::InstrumentType::kCounter, counter_name, counter_unit);

  auto meter_selector = metrics_sdk::MeterSelectorFactory::Create(name, version, schema);

  auto sum_view = metrics_sdk::ViewFactory::Create(counter_name, "description", counter_unit,
                                                   metrics_sdk::AggregationType::kSum);

  p->AddView(std::move(instrument_selector), std::move(meter_selector), std::move(sum_view));

  // histogram view
  std::string histogram_name = name + "_histogram";
  std::string histogram_unit = "unit";

  auto histogram_instrument_selector = metrics_sdk::InstrumentSelectorFactory::Create(
      metrics_sdk::InstrumentType::kHistogram, histogram_name, histogram_unit);

  auto histogram_meter_selector = metrics_sdk::MeterSelectorFactory::Create(name, version, schema);

  auto histogram_view = metrics_sdk::ViewFactory::Create(
      histogram_name, "description", histogram_unit, metrics_sdk::AggregationType::kHistogram);

  p->AddView(std::move(histogram_instrument_selector), std::move(histogram_meter_selector),
             std::move(histogram_view));

  std::shared_ptr<opentelemetry::metrics::MeterProvider> provider(std::move(u_provider));
  metrics_sdk::Provider::SetMeterProvider(provider);
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
  std::string addr{"localhost:9464"};
  if (argc == 1)
  {
    std::puts("usage: $prometheus_example <example type> <url>");
  }

  if (argc >= 2)
  {
    example_type = argv[1];
  }
  if (argc > 2)
  {
    addr = argv[2];
  }

  std::string name{"prometheus_metric_example"};
  InitMetrics(name, addr);

  if (example_type == "counter")
  {
    foo_library::counter_example(name);
  }
  else if (example_type == "histogram")
  {
    foo_library::histogram_example(name);
  }
  else
  {
    std::thread counter_example{&foo_library::counter_example, name};
    std::thread histogram_example{&foo_library::histogram_example, name};
    counter_example.join();
    histogram_example.join();
  }

  CleanupMetrics();
  return 0;
}
