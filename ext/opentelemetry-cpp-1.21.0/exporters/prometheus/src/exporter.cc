// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <prometheus/exposer.h>
#include <chrono>
#include <exception>
#include <memory>
#include <ostream>
#include <string>

#include "opentelemetry/exporters/prometheus/collector.h"
#include "opentelemetry/exporters/prometheus/exporter.h"
#include "opentelemetry/exporters/prometheus/exporter_options.h"
#include "opentelemetry/sdk/common/global_log_handler.h"
#include "opentelemetry/sdk/metrics/instruments.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE

namespace exporter
{
namespace metrics
{
/**
 * Constructor - binds an exposer and collector to the exporter
 * @param address: an address for an exposer that exposes
 *  an HTTP endpoint for the exporter to connect to
 */
PrometheusExporter::PrometheusExporter(const PrometheusExporterOptions &options) : options_(options)
{
  try
  {
    exposer_ = std::unique_ptr<::prometheus::Exposer>(new ::prometheus::Exposer{options_.url});
  }
  catch (const std::exception &ex)
  {
    exposer_.reset(nullptr);
    OTEL_INTERNAL_LOG_ERROR("[Prometheus Exporter] "
                            << "Can't initialize prometheus exposer with endpoint: " << options_.url
                            << "\nError: " << ex.what());
    Shutdown();  // set MetricReader in shutdown state.
    return;
  }
  collector_ = std::shared_ptr<PrometheusCollector>(
      new PrometheusCollector(this, options_.populate_target_info, options_.without_otel_scope));

  exposer_->RegisterCollectable(collector_);
}

sdk::metrics::AggregationTemporality PrometheusExporter::GetAggregationTemporality(
    sdk::metrics::InstrumentType /* instrument_type */) const noexcept
{
  // Prometheus exporter only support Cumulative
  return sdk::metrics::AggregationTemporality::kCumulative;
}

bool PrometheusExporter::OnForceFlush(std::chrono::microseconds /* timeout */) noexcept
{
  return true;
}

bool PrometheusExporter::OnShutDown(std::chrono::microseconds /* timeout */) noexcept
{
  if (exposer_ != nullptr)
    exposer_->RemoveCollectable(collector_);
  return true;
}

void PrometheusExporter::OnInitialized() noexcept {}

}  // namespace metrics
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
