// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

// clang-format off
#include "opentelemetry/exporters/otlp/protobuf_include_prefix.h"
// clang-format on

#include "opentelemetry/proto/collector/logs/v1/logs_service.grpc.pb.h"

// clang-format off
#include "opentelemetry/exporters/otlp/protobuf_include_suffix.h"
// clang-format on

#include "opentelemetry/exporters/otlp/otlp_environment.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_log_record_exporter_options.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/sdk/logs/exporter.h"

#include <atomic>

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace otlp
{

class OtlpGrpcClientReferenceGuard;

class OtlpGrpcClient;

/**
 * The OTLP exporter exports log data in OpenTelemetry Protocol (OTLP) format in gRPC.
 */
class OtlpGrpcLogRecordExporter : public opentelemetry::sdk::logs::LogRecordExporter
{
public:
  /**
   * Create an OtlpGrpcLogRecordExporter with default exporter options.
   */
  OtlpGrpcLogRecordExporter();

  /**
   * Create an OtlpGrpcLogRecordExporter using specified OtlpGrpcClient.
   *
   * @param options options to create exporter
   * @param client the gRPC client to use
   */
  OtlpGrpcLogRecordExporter(const OtlpGrpcLogRecordExporterOptions &options,
                            const std::shared_ptr<OtlpGrpcClient> &client);

  /**
   * Create an OtlpGrpcLogRecordExporter with user specified options.
   * @param options An object containing the user's configuration options.
   */
  OtlpGrpcLogRecordExporter(const OtlpGrpcLogRecordExporterOptions &options);

  ~OtlpGrpcLogRecordExporter() override;

  /**
   * Creates a recordable that stores the data in protobuf.
   * @return a newly initialized Recordable object.
   */
  std::unique_ptr<opentelemetry::sdk::logs::Recordable> MakeRecordable() noexcept override;

  /**
   * Exports a vector of log records to the configured gRPC endpoint. Guaranteed to return after a
   * timeout specified from the options passed to the constructor.
   * @param records A list of log records.
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
   * @param timeout The maximum time to wait for the shutdown method to return.
   */
  bool Shutdown(
      std::chrono::microseconds timeout = (std::chrono::microseconds::max)()) noexcept override;

  /**
   * Get the Client object
   *
   * @return return binded gRPC client
   */
  const std::shared_ptr<OtlpGrpcClient> &GetClient() const noexcept;

private:
  // Configuration options for the exporter
  const OtlpGrpcLogRecordExporterOptions options_;

  std::shared_ptr<OtlpGrpcClient> client_;
  std::shared_ptr<OtlpGrpcClientReferenceGuard> client_reference_guard_;

  // For testing
  friend class OtlpGrpcLogRecordExporterTestPeer;

  // Store service stub internally. Useful for testing.
  std::shared_ptr<proto::collector::logs::v1::LogsService::StubInterface> log_service_stub_;

  /**
   * Create an OtlpGrpcLogRecordExporter using the specified service stub.
   * Only tests can call this constructor directly.
   * @param stub the service stub to be used for exporting
   */
  OtlpGrpcLogRecordExporter(
      std::unique_ptr<proto::collector::logs::v1::LogsService::StubInterface> stub);

  /**
   * Create an OtlpGrpcLogRecordExporter using the specified service stub and gRPC client.
   * Only tests can call this constructor directly.
   * @param stub the service stub to be used for exporting
   * @param client the gRPC client to use
   */
  OtlpGrpcLogRecordExporter(
      std::unique_ptr<proto::collector::logs::v1::LogsService::StubInterface> stub,
      const std::shared_ptr<OtlpGrpcClient> &client);

  std::atomic<bool> is_shutdown_{false};
  bool isShutdown() const noexcept;
};

}  // namespace otlp
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
