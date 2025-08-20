// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>

#include "opentelemetry/sdk/metrics/metric_reader.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace metrics
{

struct PrometheusExporterOptions;

/**
 * Factory class for PrometheusExporter.
 */
class PrometheusExporterFactory
{
public:
  /**
   * Create a PrometheusExporter using the given options.
   */
  static std::unique_ptr<sdk::metrics::MetricReader> Create(
      const PrometheusExporterOptions &options);
};

}  // namespace metrics
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
