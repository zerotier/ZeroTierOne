// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include "opentelemetry/exporters/ostream/metric_exporter_factory.h"
#include "opentelemetry/exporters/ostream/metric_exporter.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace metrics
{

std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
OStreamMetricExporterFactory::Create()
{
  return Create(std::cout);
}

std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
OStreamMetricExporterFactory::Create(std::ostream &sout)
{
  return Create(sout, sdk::metrics::AggregationTemporality::kCumulative);
}

std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
OStreamMetricExporterFactory::Create(std::ostream &sout,
                                     sdk::metrics::AggregationTemporality aggregation_temporality)
{
  std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> exporter(
      new OStreamMetricExporter(sout, aggregation_temporality));
  return exporter;
}

}  // namespace metrics
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
