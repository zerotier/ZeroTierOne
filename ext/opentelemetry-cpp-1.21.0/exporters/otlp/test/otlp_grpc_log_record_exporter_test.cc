// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <unordered_map>

#include "opentelemetry/exporters/otlp/otlp_grpc_client.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_client_factory.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_exporter.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_log_record_exporter.h"

// clang-format off
#include "opentelemetry/exporters/otlp/protobuf_include_prefix.h"
// clang-format on

#include "opentelemetry/proto/collector/logs/v1/logs_service_mock.grpc.pb.h"
#include "opentelemetry/proto/collector/trace/v1/trace_service_mock.grpc.pb.h"

// clang-format off
#include "opentelemetry/exporters/otlp/protobuf_include_suffix.h"
// clang-format on

#include "opentelemetry/logs/provider.h"
#include "opentelemetry/sdk/logs/batch_log_record_processor.h"
#include "opentelemetry/sdk/logs/exporter.h"
#include "opentelemetry/sdk/logs/logger_provider.h"
#include "opentelemetry/sdk/logs/recordable.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/sdk/trace/exporter.h"
#include "opentelemetry/sdk/trace/processor.h"
#include "opentelemetry/sdk/trace/provider.h"
#include "opentelemetry/sdk/trace/simple_processor_factory.h"
#include "opentelemetry/sdk/trace/tracer_provider_factory.h"
#include "opentelemetry/trace/provider.h"

#include <grpcpp/grpcpp.h>
#include <gtest/gtest.h>

#if defined(_MSC_VER)
#  include "opentelemetry/sdk/common/env_variables.h"
using opentelemetry::sdk::common::setenv;
using opentelemetry::sdk::common::unsetenv;
#endif

using namespace testing;

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace otlp
{

namespace
{
class OtlpMockTraceServiceStub : public proto::collector::trace::v1::MockTraceServiceStub
{
public:
#if defined(GRPC_CPP_VERSION_MAJOR) && \
    (GRPC_CPP_VERSION_MAJOR * 1000 + GRPC_CPP_VERSION_MINOR) >= 1039
  using async_interface_base =
      proto::collector::trace::v1::TraceService::StubInterface::async_interface;
#else
  using async_interface_base =
      proto::collector::trace::v1::TraceService::StubInterface::experimental_async_interface;
#endif

  OtlpMockTraceServiceStub() : async_interface_(this) {}

  class async_interface : public async_interface_base
  {
  public:
    async_interface(OtlpMockTraceServiceStub *owner) : stub_(owner) {}

    virtual ~async_interface() override = default;

    void Export(
        ::grpc::ClientContext *context,
        const ::opentelemetry::proto::collector::trace::v1::ExportTraceServiceRequest *request,
        ::opentelemetry::proto::collector::trace::v1::ExportTraceServiceResponse *response,
        std::function<void(::grpc::Status)> callback) override
    {
      stub_->last_async_status_ = stub_->Export(context, *request, response);
      callback(stub_->last_async_status_);
    }

#if defined(GRPC_CPP_VERSION_MAJOR) &&                                      \
        (GRPC_CPP_VERSION_MAJOR * 1000 + GRPC_CPP_VERSION_MINOR) >= 1039 || \
    defined(GRPC_CALLBACK_API_NONEXPERIMENTAL)
    void Export(
        ::grpc::ClientContext * /*context*/,
        const ::opentelemetry::proto::collector::trace::v1::ExportTraceServiceRequest * /*request*/,
        ::opentelemetry::proto::collector::trace::v1::ExportTraceServiceResponse * /*response*/,
        ::grpc::ClientUnaryReactor * /*reactor*/) override
    {}
#else
    void Export(
        ::grpc::ClientContext * /*context*/,
        const ::opentelemetry::proto::collector::trace::v1::ExportTraceServiceRequest * /*request*/,
        ::opentelemetry::proto::collector::trace::v1::ExportTraceServiceResponse * /*response*/,
        ::grpc::experimental::ClientUnaryReactor * /*reactor*/)
    {}
#endif

  private:
    OtlpMockTraceServiceStub *stub_;
  };

  async_interface_base *async() override { return &async_interface_; }
  async_interface_base *experimental_async() { return &async_interface_; }

  ::grpc::Status GetLastAsyncStatus() const noexcept { return last_async_status_; }

private:
  ::grpc::Status last_async_status_;
  async_interface async_interface_;
};

class OtlpMockLogsServiceStub : public proto::collector::logs::v1::MockLogsServiceStub
{
public:
#if defined(GRPC_CPP_VERSION_MAJOR) && \
    (GRPC_CPP_VERSION_MAJOR * 1000 + GRPC_CPP_VERSION_MINOR) >= 1039
  using async_interface_base =
      proto::collector::logs::v1::LogsService::StubInterface::async_interface;
#else
  using async_interface_base =
      proto::collector::logs::v1::LogsService::StubInterface::experimental_async_interface;
#endif

  OtlpMockLogsServiceStub() : async_interface_(this) {}

  class async_interface : public async_interface_base
  {
  public:
    async_interface(OtlpMockLogsServiceStub *owner) : stub_(owner) {}

    virtual ~async_interface() override = default;

    void Export(
        ::grpc::ClientContext *context,
        const ::opentelemetry::proto::collector::logs::v1::ExportLogsServiceRequest *request,
        ::opentelemetry::proto::collector::logs::v1::ExportLogsServiceResponse *response,
        std::function<void(::grpc::Status)> callback) override
    {
      stub_->last_async_status_ = stub_->Export(context, *request, response);
      callback(stub_->last_async_status_);
    }

#if defined(GRPC_CPP_VERSION_MAJOR) &&                                      \
        (GRPC_CPP_VERSION_MAJOR * 1000 + GRPC_CPP_VERSION_MINOR) >= 1039 || \
    defined(GRPC_CALLBACK_API_NONEXPERIMENTAL)
    void Export(
        ::grpc::ClientContext * /*context*/,
        const ::opentelemetry::proto::collector::logs::v1::ExportLogsServiceRequest * /*request*/,
        ::opentelemetry::proto::collector::logs::v1::ExportLogsServiceResponse * /*response*/,
        ::grpc::ClientUnaryReactor * /*reactor*/) override
    {}
#else
    void Export(
        ::grpc::ClientContext * /*context*/,
        const ::opentelemetry::proto::collector::logs::v1::ExportLogsServiceRequest * /*request*/,
        ::opentelemetry::proto::collector::logs::v1::ExportLogsServiceResponse * /*response*/,
        ::grpc::experimental::ClientUnaryReactor * /*reactor*/)
    {}
#endif

  private:
    OtlpMockLogsServiceStub *stub_;
  };

  async_interface_base *async() override { return &async_interface_; }
  async_interface_base *experimental_async() { return &async_interface_; }

  ::grpc::Status GetLastAsyncStatus() const noexcept { return last_async_status_; }

private:
  ::grpc::Status last_async_status_;
  async_interface async_interface_;
};
}  // namespace

class OtlpGrpcLogRecordExporterTestPeer : public ::testing::Test
{
public:
  std::unique_ptr<sdk::logs::LogRecordExporter> GetExporter(
      std::unique_ptr<proto::collector::logs::v1::LogsService::StubInterface> &stub_interface)
  {
    return std::unique_ptr<sdk::logs::LogRecordExporter>(
        new OtlpGrpcLogRecordExporter(std::move(stub_interface)));
  }

  std::unique_ptr<sdk::trace::SpanExporter> GetExporter(
      std::unique_ptr<proto::collector::trace::v1::TraceService::StubInterface> &stub_interface)
  {
    return std::unique_ptr<sdk::trace::SpanExporter>(
        new OtlpGrpcExporter(std::move(stub_interface)));
  }

  std::unique_ptr<sdk::logs::LogRecordExporter> GetExporter(
      std::unique_ptr<proto::collector::logs::v1::LogsService::StubInterface> &stub_interface,
      const std::shared_ptr<OtlpGrpcClient> &client)
  {
    return std::unique_ptr<sdk::logs::LogRecordExporter>(
        new OtlpGrpcLogRecordExporter(std::move(stub_interface), std::move(client)));
  }

  std::unique_ptr<sdk::trace::SpanExporter> GetExporter(
      std::unique_ptr<proto::collector::trace::v1::TraceService::StubInterface> &stub_interface,
      const std::shared_ptr<OtlpGrpcClient> &client)
  {
    return std::unique_ptr<sdk::trace::SpanExporter>(
        new OtlpGrpcExporter(std::move(stub_interface), std::move(client)));
  }

  // Get the options associated with the given exporter.
  const OtlpGrpcLogRecordExporterOptions &GetOptions(
      std::unique_ptr<OtlpGrpcLogRecordExporter> &exporter)
  {
    return exporter->options_;
  }
};

TEST_F(OtlpGrpcLogRecordExporterTestPeer, ShutdownTest)
{
  auto mock_stub = new OtlpMockLogsServiceStub();
  std::unique_ptr<proto::collector::logs::v1::LogsService::StubInterface> stub_interface(mock_stub);
  auto exporter = GetExporter(stub_interface);

  auto recordable_1 = exporter->MakeRecordable();
  auto recordable_2 = exporter->MakeRecordable();

  // exporter shuold not be shutdown by default
  nostd::span<std::unique_ptr<sdk::logs::Recordable>> batch_1(&recordable_1, 1);
  EXPECT_CALL(*mock_stub, Export(_, _, _)).Times(Exactly(1)).WillOnce(Return(grpc::Status::OK));

  auto result = exporter->Export(batch_1);
  exporter->ForceFlush();
  EXPECT_EQ(sdk::common::ExportResult::kSuccess, result);

  exporter->Shutdown();

  nostd::span<std::unique_ptr<sdk::logs::Recordable>> batch_2(&recordable_2, 1);
  result = exporter->Export(batch_2);
  EXPECT_EQ(sdk::common::ExportResult::kFailure, result);
}

// Call Export() directly
TEST_F(OtlpGrpcLogRecordExporterTestPeer, ExportUnitTest)
{
  auto mock_stub = new OtlpMockLogsServiceStub();
  std::unique_ptr<proto::collector::logs::v1::LogsService::StubInterface> stub_interface(mock_stub);
  auto exporter = GetExporter(stub_interface);

  auto recordable_1 = exporter->MakeRecordable();
  auto recordable_2 = exporter->MakeRecordable();

  // Test successful RPC
  nostd::span<std::unique_ptr<sdk::logs::Recordable>> batch_1(&recordable_1, 1);
  EXPECT_CALL(*mock_stub, Export(_, _, _)).Times(Exactly(1)).WillOnce(Return(grpc::Status::OK));
  auto result = exporter->Export(batch_1);
  exporter->ForceFlush();
  EXPECT_EQ(sdk::common::ExportResult::kSuccess, result);

  // Test failed RPC
  nostd::span<std::unique_ptr<sdk::logs::Recordable>> batch_2(&recordable_2, 1);
  EXPECT_CALL(*mock_stub, Export(_, _, _))
      .Times(Exactly(1))
      .WillOnce(Return(grpc::Status::CANCELLED));
  result = exporter->Export(batch_2);
  exporter->ForceFlush();
#if defined(ENABLE_ASYNC_EXPORT)
  EXPECT_EQ(sdk::common::ExportResult::kSuccess, result);
  EXPECT_FALSE(mock_stub->GetLastAsyncStatus().ok());
#else
  EXPECT_EQ(sdk::common::ExportResult::kFailure, result);
#endif
}

// Create spans, let processor call Export()
TEST_F(OtlpGrpcLogRecordExporterTestPeer, ExportIntegrationTest)
{
  auto mock_stub = new OtlpMockLogsServiceStub();
  std::unique_ptr<proto::collector::logs::v1::LogsService::StubInterface> stub_interface(mock_stub);

  auto exporter = GetExporter(stub_interface);

  bool attribute_storage_bool_value[]                                = {true, false, true};
  int32_t attribute_storage_int32_value[]                            = {1, 2};
  uint32_t attribute_storage_uint32_value[]                          = {3, 4};
  int64_t attribute_storage_int64_value[]                            = {5, 6};
  uint64_t attribute_storage_uint64_value[]                          = {7, 8};
  double attribute_storage_double_value[]                            = {3.2, 3.3};
  opentelemetry::nostd::string_view attribute_storage_string_value[] = {"vector", "string"};

  auto provider = nostd::shared_ptr<sdk::logs::LoggerProvider>(new sdk::logs::LoggerProvider());
  provider->AddProcessor(
      std::unique_ptr<sdk::logs::LogRecordProcessor>(new sdk::logs::BatchLogRecordProcessor(
          std::move(exporter), 5, std::chrono::milliseconds(256), 1)));

  EXPECT_CALL(*mock_stub, Export(_, _, _))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(grpc::Status::OK));

  uint8_t trace_id_bin[opentelemetry::trace::TraceId::kSize] = {
      '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
  opentelemetry::trace::TraceId trace_id{trace_id_bin};
  uint8_t span_id_bin[opentelemetry::trace::SpanId::kSize] = {'7', '6', '5', '4',
                                                              '3', '2', '1', '0'};
  opentelemetry::trace::SpanId span_id{span_id_bin};

  auto trace_mock_stub = new OtlpMockTraceServiceStub();
  std::unique_ptr<proto::collector::trace::v1::TraceService::StubInterface> trace_stub_interface(
      trace_mock_stub);

  auto trace_provider = opentelemetry::nostd::shared_ptr<opentelemetry::sdk::trace::TracerProvider>(
      opentelemetry::sdk::trace::TracerProviderFactory::Create(
          opentelemetry::sdk::trace::SimpleSpanProcessorFactory::Create(
              GetExporter(trace_stub_interface))));

  // Trace and Logs should both receive datas when links static gRPC on ELF ABI.
  EXPECT_CALL(*trace_mock_stub, Export(_, _, _))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(grpc::Status::OK));

  {
    const std::string schema_url{"https://opentelemetry.io/schemas/1.11.0"};

    auto tracer = trace_provider->GetTracer("opentelelemtry_library", "", schema_url);
    opentelemetry::sdk::trace::Provider::SetTracerProvider(std::move(trace_provider));
    auto trace_span = tracer->StartSpan("test_log");
    opentelemetry::trace::Scope trace_scope{trace_span};

    auto logger = provider->GetLogger("test", "opentelelemtry_library", "", schema_url,
                                      {{"scope_key1", "scope_value"}, {"scope_key2", 2}});
    std::unordered_map<std::string, opentelemetry::common::AttributeValue> attributes;
    attributes["service.name"]     = "unit_test_service";
    attributes["tenant.id"]        = "test_user";
    attributes["bool_value"]       = true;
    attributes["int32_value"]      = static_cast<int32_t>(1);
    attributes["uint32_value"]     = static_cast<uint32_t>(2);
    attributes["int64_value"]      = static_cast<int64_t>(0x1100000000LL);
    attributes["uint64_value"]     = static_cast<uint64_t>(0x1200000000ULL);
    attributes["double_value"]     = static_cast<double>(3.1);
    attributes["vec_bool_value"]   = attribute_storage_bool_value;
    attributes["vec_int32_value"]  = attribute_storage_int32_value;
    attributes["vec_uint32_value"] = attribute_storage_uint32_value;
    attributes["vec_int64_value"]  = attribute_storage_int64_value;
    attributes["vec_uint64_value"] = attribute_storage_uint64_value;
    attributes["vec_double_value"] = attribute_storage_double_value;
    attributes["vec_string_value"] = attribute_storage_string_value;
    logger->EmitLogRecord(opentelemetry::logs::Severity::kInfo, "Log message", attributes,
                          trace_span->GetContext(), std::chrono::system_clock::now());
  }

  opentelemetry::sdk::trace::Provider::SetTracerProvider(
      opentelemetry::nostd::shared_ptr<opentelemetry::trace::TracerProvider>(
          new opentelemetry::trace::NoopTracerProvider()));
  trace_provider = opentelemetry::nostd::shared_ptr<opentelemetry::sdk::trace::TracerProvider>();
}

// Create spans, let processor call Export() and share client object between trace and logs
TEST_F(OtlpGrpcLogRecordExporterTestPeer, ShareClientTest)
{
  std::shared_ptr<OtlpGrpcClient> shared_client =
      OtlpGrpcClientFactory::Create(OtlpGrpcLogRecordExporterOptions());

  auto mock_stub = new OtlpMockLogsServiceStub();
  std::unique_ptr<proto::collector::logs::v1::LogsService::StubInterface> stub_interface(mock_stub);

  auto exporter = GetExporter(stub_interface, shared_client);

  bool attribute_storage_bool_value[]                                = {true, false, true};
  int32_t attribute_storage_int32_value[]                            = {1, 2};
  uint32_t attribute_storage_uint32_value[]                          = {3, 4};
  int64_t attribute_storage_int64_value[]                            = {5, 6};
  uint64_t attribute_storage_uint64_value[]                          = {7, 8};
  double attribute_storage_double_value[]                            = {3.2, 3.3};
  opentelemetry::nostd::string_view attribute_storage_string_value[] = {"vector", "string"};

  auto provider = nostd::shared_ptr<sdk::logs::LoggerProvider>(new sdk::logs::LoggerProvider());
  provider->AddProcessor(
      std::unique_ptr<sdk::logs::LogRecordProcessor>(new sdk::logs::BatchLogRecordProcessor(
          std::move(exporter), 5, std::chrono::milliseconds(256), 1)));

  EXPECT_CALL(*mock_stub, Export(_, _, _))
      .Times(Exactly(1))
      .WillRepeatedly(Return(grpc::Status::OK));

  uint8_t trace_id_bin[opentelemetry::trace::TraceId::kSize] = {
      '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
  opentelemetry::trace::TraceId trace_id{trace_id_bin};
  uint8_t span_id_bin[opentelemetry::trace::SpanId::kSize] = {'7', '6', '5', '4',
                                                              '3', '2', '1', '0'};
  opentelemetry::trace::SpanId span_id{span_id_bin};

  auto trace_mock_stub = new OtlpMockTraceServiceStub();
  std::unique_ptr<proto::collector::trace::v1::TraceService::StubInterface> trace_stub_interface(
      trace_mock_stub);

  auto trace_provider = opentelemetry::nostd::shared_ptr<opentelemetry::sdk::trace::TracerProvider>(
      opentelemetry::sdk::trace::TracerProviderFactory::Create(
          opentelemetry::sdk::trace::SimpleSpanProcessorFactory::Create(
              GetExporter(trace_stub_interface, shared_client))));

  // Trace and Logs should both receive datas when links static gRPC on ELF ABI.
  EXPECT_CALL(*trace_mock_stub, Export(_, _, _))
      .Times(AtLeast(2))
      .WillRepeatedly(Return(grpc::Status::OK));

  {
    const std::string schema_url{"https://opentelemetry.io/schemas/1.11.0"};

    auto tracer              = trace_provider->GetTracer("opentelelemtry_library", "", schema_url);
    auto copy_trace_provider = trace_provider;
    opentelemetry::sdk::trace::Provider::SetTracerProvider(std::move(copy_trace_provider));
    auto trace_span = tracer->StartSpan("test_log");
    opentelemetry::trace::Scope trace_scope{trace_span};

    auto logger = provider->GetLogger("test", "opentelelemtry_library", "", schema_url,
                                      {{"scope_key1", "scope_value"}, {"scope_key2", 2}});
    std::unordered_map<std::string, opentelemetry::common::AttributeValue> attributes;
    attributes["service.name"]     = "unit_test_service";
    attributes["tenant.id"]        = "test_user";
    attributes["bool_value"]       = true;
    attributes["int32_value"]      = static_cast<int32_t>(1);
    attributes["uint32_value"]     = static_cast<uint32_t>(2);
    attributes["int64_value"]      = static_cast<int64_t>(0x1100000000LL);
    attributes["uint64_value"]     = static_cast<uint64_t>(0x1200000000ULL);
    attributes["double_value"]     = static_cast<double>(3.1);
    attributes["vec_bool_value"]   = attribute_storage_bool_value;
    attributes["vec_int32_value"]  = attribute_storage_int32_value;
    attributes["vec_uint32_value"] = attribute_storage_uint32_value;
    attributes["vec_int64_value"]  = attribute_storage_int64_value;
    attributes["vec_uint64_value"] = attribute_storage_uint64_value;
    attributes["vec_double_value"] = attribute_storage_double_value;
    attributes["vec_string_value"] = attribute_storage_string_value;
    logger->EmitLogRecord(opentelemetry::logs::Severity::kInfo, "Log message", attributes,
                          trace_span->GetContext(), std::chrono::system_clock::now());
  }

  // Shudown logs, but tracer still works
  provider->Shutdown();
  EXPECT_FALSE(shared_client->IsShutdown());

  {
    const std::string schema_url{"https://opentelemetry.io/schemas/1.11.0"};

    auto tracer     = trace_provider->GetTracer("opentelelemtry_library", "", schema_url);
    auto trace_span = tracer->StartSpan("test_log");
    opentelemetry::trace::Scope trace_scope{trace_span};

    auto logger = provider->GetLogger("test", "opentelelemtry_library", "", schema_url,
                                      {{"scope_key1", "scope_value"}, {"scope_key2", 2}});
    std::unordered_map<std::string, opentelemetry::common::AttributeValue> attributes;
    attributes["service.name"] = "unit_test_service";
    attributes["tenant.id"]    = "test_user";
    logger->EmitLogRecord(opentelemetry::logs::Severity::kInfo, "Log message", attributes,
                          trace_span->GetContext(), std::chrono::system_clock::now());
  }

  // All references are released, client should also be shutdown
  trace_provider->Shutdown();
  EXPECT_TRUE(shared_client->IsShutdown());

  opentelemetry::sdk::trace::Provider::SetTracerProvider(
      opentelemetry::nostd::shared_ptr<opentelemetry::trace::TracerProvider>(
          new opentelemetry::trace::NoopTracerProvider()));
  trace_provider = opentelemetry::nostd::shared_ptr<opentelemetry::sdk::trace::TracerProvider>();
}

#ifndef NO_GETENV
TEST_F(OtlpGrpcLogRecordExporterTestPeer, ConfigRetryDefaultValues)
{
  std::unique_ptr<OtlpGrpcLogRecordExporter> exporter(new OtlpGrpcLogRecordExporter());
  const auto options = GetOptions(exporter);
  ASSERT_EQ(options.retry_policy_max_attempts, 5);
  ASSERT_FLOAT_EQ(options.retry_policy_initial_backoff.count(), 1.0f);
  ASSERT_FLOAT_EQ(options.retry_policy_max_backoff.count(), 5.0f);
  ASSERT_FLOAT_EQ(options.retry_policy_backoff_multiplier, 1.5f);
}

TEST_F(OtlpGrpcLogRecordExporterTestPeer, ConfigRetryValuesFromEnv)
{
  setenv("OTEL_CPP_EXPORTER_OTLP_LOGS_RETRY_MAX_ATTEMPTS", "123", 1);
  setenv("OTEL_CPP_EXPORTER_OTLP_LOGS_RETRY_INITIAL_BACKOFF", "4.5", 1);
  setenv("OTEL_CPP_EXPORTER_OTLP_LOGS_RETRY_MAX_BACKOFF", "6.7", 1);
  setenv("OTEL_CPP_EXPORTER_OTLP_LOGS_RETRY_BACKOFF_MULTIPLIER", "8.9", 1);

  std::unique_ptr<OtlpGrpcLogRecordExporter> exporter(new OtlpGrpcLogRecordExporter());
  const auto options = GetOptions(exporter);
  ASSERT_EQ(options.retry_policy_max_attempts, 123);
  ASSERT_FLOAT_EQ(options.retry_policy_initial_backoff.count(), 4.5f);
  ASSERT_FLOAT_EQ(options.retry_policy_max_backoff.count(), 6.7f);
  ASSERT_FLOAT_EQ(options.retry_policy_backoff_multiplier, 8.9f);

  unsetenv("OTEL_CPP_EXPORTER_OTLP_LOGS_RETRY_MAX_ATTEMPTS");
  unsetenv("OTEL_CPP_EXPORTER_OTLP_LOGS_RETRY_INITIAL_BACKOFF");
  unsetenv("OTEL_CPP_EXPORTER_OTLP_LOGS_RETRY_MAX_BACKOFF");
  unsetenv("OTEL_CPP_EXPORTER_OTLP_LOGS_RETRY_BACKOFF_MULTIPLIER");
}

TEST_F(OtlpGrpcLogRecordExporterTestPeer, ConfigRetryGenericValuesFromEnv)
{
  setenv("OTEL_CPP_EXPORTER_OTLP_RETRY_MAX_ATTEMPTS", "321", 1);
  setenv("OTEL_CPP_EXPORTER_OTLP_RETRY_INITIAL_BACKOFF", "5.4", 1);
  setenv("OTEL_CPP_EXPORTER_OTLP_RETRY_MAX_BACKOFF", "7.6", 1);
  setenv("OTEL_CPP_EXPORTER_OTLP_RETRY_BACKOFF_MULTIPLIER", "9.8", 1);

  std::unique_ptr<OtlpGrpcLogRecordExporter> exporter(new OtlpGrpcLogRecordExporter());
  const auto options = GetOptions(exporter);
  ASSERT_EQ(options.retry_policy_max_attempts, 321);
  ASSERT_FLOAT_EQ(options.retry_policy_initial_backoff.count(), 5.4f);
  ASSERT_FLOAT_EQ(options.retry_policy_max_backoff.count(), 7.6f);
  ASSERT_FLOAT_EQ(options.retry_policy_backoff_multiplier, 9.8f);

  unsetenv("OTEL_CPP_EXPORTER_OTLP_RETRY_MAX_ATTEMPTS");
  unsetenv("OTEL_CPP_EXPORTER_OTLP_RETRY_INITIAL_BACKOFF");
  unsetenv("OTEL_CPP_EXPORTER_OTLP_RETRY_MAX_BACKOFF");
  unsetenv("OTEL_CPP_EXPORTER_OTLP_RETRY_BACKOFF_MULTIPLIER");
}
#endif  // NO_GETENV

}  // namespace otlp
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
