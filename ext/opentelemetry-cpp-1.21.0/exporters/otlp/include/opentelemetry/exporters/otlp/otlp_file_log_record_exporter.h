// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chrono>
#include <memory>

#include "opentelemetry/exporters/otlp/otlp_file_client.h"
#include "opentelemetry/exporters/otlp/otlp_file_log_record_exporter_options.h"
#include "opentelemetry/exporters/otlp/otlp_file_log_record_exporter_runtime_options.h"
#include "opentelemetry/nostd/span.h"
#include "opentelemetry/sdk/common/exporter_utils.h"
#include "opentelemetry/sdk/logs/exporter.h"
#include "opentelemetry/sdk/logs/recordable.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace otlp
{

/**
 * The OTLP exporter exports log data in OpenTelemetry Protocol (OTLP) format.
 */
class OtlpFileLogRecordExporter final : public opentelemetry::sdk::logs::LogRecordExporter
{
public:
  /**
   * Create an OtlpFileLogRecordExporter with default exporter options.
   */
  OtlpFileLogRecordExporter();

  /**
   * Create an OtlpFileLogRecordExporter with user specified options.
   * @param options An object containing the user's configuration options.
   */
  OtlpFileLogRecordExporter(const OtlpFileLogRecordExporterOptions &options);

  /**
   * Create an OtlpFileLogRecordExporter with user specified options.
   * @param options An object containing the user's configuration options.
   * @param runtime_options An object containing the user's runtime options.
   */
  OtlpFileLogRecordExporter(const OtlpFileLogRecordExporterOptions &options,
                            const OtlpFileLogRecordExporterRuntimeOptions &runtime_options);

  /**
   * Creates a recordable that stores the data in a JSON object
   */
  std::unique_ptr<opentelemetry::sdk::logs::Recordable> MakeRecordable() noexcept override;

  /**
   * Exports a vector of log records to the Elasticsearch instance. Guaranteed to return after a
   * timeout specified from the options passed from the constructor.
   * @param records A list of log records to send to Elasticsearch.
   */
  opentelemetry::sdk::common::ExportResult Export(
      const nostd::span<std::unique_ptr<opentelemetry::sdk::logs::Recordable>> &records) noexcept
      override;

  /**
   * Force flush the exporter.
   * @param timeout an option timeout, default to max.
   * @return return true when all data are exported, and false when timeout
   */
  bool ForceFlush(
      std::chrono::microseconds timeout = (std::chrono::microseconds::max)()) noexcept override;

  /**
   * Shutdown this exporter.
   * @param timeout The maximum time to wait for the shutdown method to return
   */
  bool Shutdown(
      std::chrono::microseconds timeout = (std::chrono::microseconds::max)()) noexcept override;

private:
  // Configuration options for the exporter
  const OtlpFileLogRecordExporterOptions options_;
  // Runtime options for the exporter
  const OtlpFileLogRecordExporterRuntimeOptions runtime_options_;

  // Object that stores the file context.
  std::unique_ptr<OtlpFileClient> file_client_;
};
}  // namespace otlp
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
