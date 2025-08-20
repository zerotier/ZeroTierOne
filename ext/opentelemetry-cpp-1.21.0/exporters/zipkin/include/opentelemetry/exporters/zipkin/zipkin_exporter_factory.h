// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>

#include "opentelemetry/exporters/zipkin/zipkin_exporter_options.h"
#include "opentelemetry/sdk/trace/exporter.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace zipkin
{

/**
 * Factory class for ZipkinExporter.
 */
class ZipkinExporterFactory
{
public:
  /**
   * Create a ZipkinExporter using all default options.
   */
  static std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> Create();

  /**
   * Create a ZipkinExporter using the given options.
   */
  static std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> Create(
      const ZipkinExporterOptions &options);
};

}  // namespace zipkin
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
