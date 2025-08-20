// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <memory>

#include "opentelemetry/exporters/prometheus/exporter.h"
#include "opentelemetry/exporters/prometheus/exporter_factory.h"
#include "opentelemetry/exporters/prometheus/exporter_options.h"
#include "opentelemetry/sdk/metrics/metric_reader.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace metrics
{

std::unique_ptr<opentelemetry::sdk::metrics::MetricReader> PrometheusExporterFactory::Create(
    const PrometheusExporterOptions &options)
{
  std::unique_ptr<opentelemetry::sdk::metrics::MetricReader> exporter(
      new PrometheusExporter(options));
  return exporter;
}

}  // namespace metrics
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
