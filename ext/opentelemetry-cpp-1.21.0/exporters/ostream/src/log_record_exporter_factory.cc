// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include "opentelemetry/exporters/ostream/log_record_exporter_factory.h"
#include "opentelemetry/exporters/ostream/log_record_exporter.h"
#include "opentelemetry/version.h"

namespace logs_sdk = opentelemetry::sdk::logs;

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace logs
{

std::unique_ptr<logs_sdk::LogRecordExporter> OStreamLogRecordExporterFactory::Create()
{
  return Create(std::cout);
}

std::unique_ptr<logs_sdk::LogRecordExporter> OStreamLogRecordExporterFactory::Create(
    std::ostream &sout)
{
  std::unique_ptr<logs_sdk::LogRecordExporter> exporter(new OStreamLogRecordExporter(sout));
  return exporter;
}

}  // namespace logs
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
