// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include "opentelemetry/exporters/ostream/span_exporter_factory.h"
#include "opentelemetry/exporters/ostream/span_exporter.h"
#include "opentelemetry/version.h"

namespace trace_sdk = opentelemetry::sdk::trace;

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace trace
{

std::unique_ptr<trace_sdk::SpanExporter> OStreamSpanExporterFactory::Create()
{
  return Create(std::cout);
}

std::unique_ptr<trace_sdk::SpanExporter> OStreamSpanExporterFactory::Create(std::ostream &sout)
{
  std::unique_ptr<trace_sdk::SpanExporter> exporter(new OStreamSpanExporter(sout));
  return exporter;
}

}  // namespace trace
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
