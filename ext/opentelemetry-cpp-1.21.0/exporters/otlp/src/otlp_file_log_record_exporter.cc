// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <chrono>
#include <cstddef>
#include <memory>
#include <new>
#include <ostream>

#include "opentelemetry/exporters/otlp/otlp_file_client.h"
#include "opentelemetry/exporters/otlp/otlp_file_client_options.h"
#include "opentelemetry/exporters/otlp/otlp_file_log_record_exporter.h"
#include "opentelemetry/exporters/otlp/otlp_file_log_record_exporter_options.h"
#include "opentelemetry/exporters/otlp/otlp_file_log_record_exporter_runtime_options.h"
#include "opentelemetry/exporters/otlp/otlp_log_recordable.h"
#include "opentelemetry/exporters/otlp/otlp_recordable_utils.h"
#include "opentelemetry/nostd/span.h"
#include "opentelemetry/sdk/common/exporter_utils.h"
#include "opentelemetry/sdk/common/global_log_handler.h"
#include "opentelemetry/sdk/logs/recordable.h"
#include "opentelemetry/version.h"

// clang-format off
#include "opentelemetry/exporters/otlp/protobuf_include_prefix.h" // IWYU pragma: keep
#include "google/protobuf/arena.h"
#include "opentelemetry/proto/collector/logs/v1/logs_service.pb.h"
#include "opentelemetry/exporters/otlp/protobuf_include_suffix.h" // IWYU pragma: keep
// clang-format on

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace otlp
{

OtlpFileLogRecordExporter::OtlpFileLogRecordExporter()
    : OtlpFileLogRecordExporter(OtlpFileLogRecordExporterOptions())
{}

OtlpFileLogRecordExporter::OtlpFileLogRecordExporter(
    const OtlpFileLogRecordExporterOptions &options)
    : OtlpFileLogRecordExporter(options, OtlpFileLogRecordExporterRuntimeOptions())
{}

OtlpFileLogRecordExporter::OtlpFileLogRecordExporter(
    const OtlpFileLogRecordExporterOptions &options,
    const OtlpFileLogRecordExporterRuntimeOptions &runtime_options)
    : options_(options),
      runtime_options_(runtime_options),
      file_client_(new OtlpFileClient(OtlpFileClientOptions(options),
                                      OtlpFileLogRecordExporterRuntimeOptions(runtime_options)))
{}
// ----------------------------- Exporter methods ------------------------------

std::unique_ptr<opentelemetry::sdk::logs::Recordable>
OtlpFileLogRecordExporter::MakeRecordable() noexcept
{
  return std::unique_ptr<opentelemetry::sdk::logs::Recordable>(new OtlpLogRecordable());
}

opentelemetry::sdk::common::ExportResult OtlpFileLogRecordExporter::Export(
    const nostd::span<std::unique_ptr<opentelemetry::sdk::logs::Recordable>> &logs) noexcept
{
  if (file_client_->IsShutdown())
  {
    std::size_t log_count = logs.size();
    OTEL_INTERNAL_LOG_ERROR("[OTLP LOG FILE Exporter] ERROR: Export "
                            << log_count << " log(s) failed, exporter is shutdown");
    return opentelemetry::sdk::common::ExportResult::kFailure;
  }

  if (logs.empty())
  {
    return opentelemetry::sdk::common::ExportResult::kSuccess;
  }

  google::protobuf::ArenaOptions arena_options;
  // It's easy to allocate datas larger than 1024 when we populate basic resource and attributes
  arena_options.initial_block_size = 1024;
  // When in batch mode, it's easy to export a large number of spans at once, we can alloc a lager
  // block to reduce memory fragments.
  arena_options.max_block_size = 65536;
  google::protobuf::Arena arena{arena_options};

  proto::collector::logs::v1::ExportLogsServiceRequest *service_request =
      google::protobuf::Arena::Create<proto::collector::logs::v1::ExportLogsServiceRequest>(&arena);
  OtlpRecordableUtils::PopulateRequest(logs, service_request);
  std::size_t log_count = logs.size();

  opentelemetry::sdk::common::ExportResult result =
      file_client_->Export(*service_request, log_count);
  if (result != opentelemetry::sdk::common::ExportResult::kSuccess)
  {
    OTEL_INTERNAL_LOG_ERROR("[OTLP LOG FILE Exporter] ERROR: Export "
                            << log_count << " log(s) error: " << static_cast<int>(result));
  }
  else
  {
    OTEL_INTERNAL_LOG_DEBUG("[OTLP LOG FILE Exporter] Export " << log_count << " log(s) success");
  }
  return result;
}

bool OtlpFileLogRecordExporter::ForceFlush(std::chrono::microseconds timeout) noexcept
{
  return file_client_->ForceFlush(timeout);
}

bool OtlpFileLogRecordExporter::Shutdown(std::chrono::microseconds timeout) noexcept
{
  return file_client_->Shutdown(timeout);
}

}  // namespace otlp
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
