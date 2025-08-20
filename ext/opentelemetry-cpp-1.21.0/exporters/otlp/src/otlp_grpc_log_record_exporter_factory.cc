// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

// MUST be first (absl)
#include "opentelemetry/exporters/otlp/otlp_grpc_log_record_exporter.h"

#include "opentelemetry/exporters/otlp/otlp_grpc_log_record_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_log_record_exporter_options.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace otlp
{

std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter>
OtlpGrpcLogRecordExporterFactory::Create()
{
  OtlpGrpcLogRecordExporterOptions options;
  return Create(options);
}

std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter>
OtlpGrpcLogRecordExporterFactory::Create(const OtlpGrpcLogRecordExporterOptions &options)
{
  std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> exporter(
      new OtlpGrpcLogRecordExporter(options));
  return exporter;
}

std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter>
OtlpGrpcLogRecordExporterFactory::Create(const OtlpGrpcLogRecordExporterOptions &options,
                                         const std::shared_ptr<OtlpGrpcClient> &client)
{
  std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> exporter(
      new OtlpGrpcLogRecordExporter(options, client));
  return exporter;
}

}  // namespace otlp
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
