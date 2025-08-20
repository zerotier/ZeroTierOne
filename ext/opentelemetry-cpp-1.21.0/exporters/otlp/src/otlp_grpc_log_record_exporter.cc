// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <chrono>
#include <memory>

#include "opentelemetry/common/macros.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_client.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_client_factory.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_log_record_exporter.h"
#include "opentelemetry/exporters/otlp/otlp_log_recordable.h"
#include "opentelemetry/exporters/otlp/otlp_recordable_utils.h"

// clang-format off
#include "opentelemetry/exporters/otlp/protobuf_include_prefix.h"
// clang-format on

#include "opentelemetry/proto/collector/logs/v1/logs_service.grpc.pb.h"
#include "opentelemetry/proto/collector/logs/v1/logs_service.pb.h"

// clang-format off
#include "opentelemetry/exporters/otlp/protobuf_include_suffix.h"
// clang-format on

#include "opentelemetry/sdk/common/global_log_handler.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace otlp
{
// -------------------------------- Constructors --------------------------------

OtlpGrpcLogRecordExporter::OtlpGrpcLogRecordExporter()
    : OtlpGrpcLogRecordExporter(OtlpGrpcLogRecordExporterOptions())
{}

OtlpGrpcLogRecordExporter::OtlpGrpcLogRecordExporter(
    const OtlpGrpcLogRecordExporterOptions &options)
    : options_(options)
{
  client_                 = OtlpGrpcClientFactory::Create(options_);
  client_reference_guard_ = OtlpGrpcClientFactory::CreateReferenceGuard();
  client_->AddReference(*client_reference_guard_, options_);

  log_service_stub_ = client_->MakeLogsServiceStub();
}

OtlpGrpcLogRecordExporter::OtlpGrpcLogRecordExporter(
    std::unique_ptr<proto::collector::logs::v1::LogsService::StubInterface> stub)
    : options_(OtlpGrpcLogRecordExporterOptions()), log_service_stub_(std::move(stub))
{
  client_                 = OtlpGrpcClientFactory::Create(options_);
  client_reference_guard_ = OtlpGrpcClientFactory::CreateReferenceGuard();
  client_->AddReference(*client_reference_guard_, options_);
}

OtlpGrpcLogRecordExporter::OtlpGrpcLogRecordExporter(
    const OtlpGrpcLogRecordExporterOptions &options,
    const std::shared_ptr<OtlpGrpcClient> &client)
    : options_(options),
      client_(client),
      client_reference_guard_(OtlpGrpcClientFactory::CreateReferenceGuard())
{
  client_->AddReference(*client_reference_guard_, options_);

  log_service_stub_ = client_->MakeLogsServiceStub();
}

OtlpGrpcLogRecordExporter::OtlpGrpcLogRecordExporter(
    std::unique_ptr<proto::collector::logs::v1::LogsService::StubInterface> stub,
    const std::shared_ptr<OtlpGrpcClient> &client)
    : options_(OtlpGrpcLogRecordExporterOptions()),
      client_(client),
      client_reference_guard_(OtlpGrpcClientFactory::CreateReferenceGuard()),
      log_service_stub_(std::move(stub))
{
  client_->AddReference(*client_reference_guard_, options_);
}

OtlpGrpcLogRecordExporter::~OtlpGrpcLogRecordExporter()
{
  if (client_)
  {
    client_->RemoveReference(*client_reference_guard_);
  }
}

// ----------------------------- Exporter methods ------------------------------

std::unique_ptr<opentelemetry::sdk::logs::Recordable>
OtlpGrpcLogRecordExporter::MakeRecordable() noexcept
{
  return std::unique_ptr<opentelemetry::sdk::logs::Recordable>(new OtlpLogRecordable());
}

opentelemetry::sdk::common::ExportResult OtlpGrpcLogRecordExporter::Export(
    const nostd::span<std::unique_ptr<opentelemetry::sdk::logs::Recordable>> &logs) noexcept
{
  std::shared_ptr<OtlpGrpcClient> client = client_;
  if (isShutdown() || !client)
  {
    OTEL_INTERNAL_LOG_ERROR("[OTLP gRPC log] Exporting " << logs.size()
                                                         << " log(s) failed, exporter is shutdown");
    return sdk::common::ExportResult::kFailure;
  }
  if (!log_service_stub_)
  {
    OTEL_INTERNAL_LOG_ERROR("[OTLP gRPC] Exporting " << logs.size()
                                                     << " log(s) failed, service stub unavailable");
    return sdk::common::ExportResult::kFailure;
  }

  if (logs.empty())
  {
    return sdk::common::ExportResult::kSuccess;
  }

  google::protobuf::ArenaOptions arena_options;
  // It's easy to allocate datas larger than 1024 when we populate basic resource and attributes
  arena_options.initial_block_size = 1024;
  // When in batch mode, it's easy to export a large number of spans at once, we can alloc a lager
  // block to reduce memory fragments.
  arena_options.max_block_size = 65536;
  std::unique_ptr<google::protobuf::Arena> arena{new google::protobuf::Arena{arena_options}};

  proto::collector::logs::v1::ExportLogsServiceRequest *request =
      google::protobuf::Arena::Create<proto::collector::logs::v1::ExportLogsServiceRequest>(
          arena.get());
  OtlpRecordableUtils::PopulateRequest(logs, request);

  auto context = OtlpGrpcClient::MakeClientContext(options_);
  proto::collector::logs::v1::ExportLogsServiceResponse *response =
      google::protobuf::Arena::Create<proto::collector::logs::v1::ExportLogsServiceResponse>(
          arena.get());

#ifdef ENABLE_ASYNC_EXPORT
  if (options_.max_concurrent_requests > 1)
  {
    return client->DelegateAsyncExport(
        options_, log_service_stub_.get(), std::move(context), std::move(arena),
        std::move(*request),
        // Capture log_service_stub by value to ensure it is not destroyed before the callback is
        // called.
        [log_service_stub = log_service_stub_](
            opentelemetry::sdk::common::ExportResult result,
            std::unique_ptr<google::protobuf::Arena> &&,
            const proto::collector::logs::v1::ExportLogsServiceRequest &request,
            proto::collector::logs::v1::ExportLogsServiceResponse *) {
          if (result != opentelemetry::sdk::common::ExportResult::kSuccess)
          {
            OTEL_INTERNAL_LOG_ERROR("[OTLP LOG GRPC Exporter] ERROR: Export "
                                    << request.resource_logs_size()
                                    << " log(s) error: " << static_cast<int>(result));
          }
          else
          {
            OTEL_INTERNAL_LOG_DEBUG("[OTLP LOG GRPC Exporter] Export "
                                    << request.resource_logs_size() << " log(s) success");
          }
          return true;
        });
  }
  else
  {
#endif
    grpc::Status status =
        OtlpGrpcClient::DelegateExport(log_service_stub_.get(), std::move(context),
                                       std::move(arena), std::move(*request), response);

    if (!status.ok())
    {
      OTEL_INTERNAL_LOG_ERROR(
          "[OTLP LOG GRPC Exporter] Export() failed: " << status.error_message());
      return sdk::common::ExportResult::kFailure;
    }
#ifdef ENABLE_ASYNC_EXPORT
  }
#endif
  return sdk::common::ExportResult::kSuccess;
}

bool OtlpGrpcLogRecordExporter::Shutdown(
    OPENTELEMETRY_MAYBE_UNUSED std::chrono::microseconds timeout) noexcept
{
  is_shutdown_ = true;
  // Maybe already shutdown, we need to keep thread-safety here.
  std::shared_ptr<OtlpGrpcClient> client;
  client.swap(client_);
  if (!client)
  {
    return true;
  }
  return client->Shutdown(*client_reference_guard_, timeout);
}

bool OtlpGrpcLogRecordExporter::ForceFlush(
    OPENTELEMETRY_MAYBE_UNUSED std::chrono::microseconds timeout) noexcept
{
  // Maybe already shutdown, we need to keep thread-safety here.
  std::shared_ptr<OtlpGrpcClient> client = client_;
  if (!client)
  {
    return true;
  }
  return client->ForceFlush(timeout);
}

bool OtlpGrpcLogRecordExporter::isShutdown() const noexcept
{
  return is_shutdown_;
}

const std::shared_ptr<OtlpGrpcClient> &OtlpGrpcLogRecordExporter::GetClient() const noexcept
{
  return client_;
}

}  // namespace otlp
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
