// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#ifndef OPENTELEMETRY_STL_VERSION

#  include <chrono>
#  include <thread>

#  include "opentelemetry/exporters/otlp/otlp_http_log_record_exporter.h"

#  include "opentelemetry/exporters/otlp/protobuf_include_prefix.h"

#  include "opentelemetry/proto/collector/logs/v1/logs_service.pb.h"

#  include "opentelemetry/exporters/otlp/protobuf_include_suffix.h"

#  include "opentelemetry/common/key_value_iterable_view.h"
#  include "opentelemetry/ext/http/client/http_client_factory.h"
#  include "opentelemetry/ext/http/server/http_server.h"
#  include "opentelemetry/logs/provider.h"
#  include "opentelemetry/sdk/logs/batch_log_record_processor.h"
#  include "opentelemetry/sdk/logs/exporter.h"
#  include "opentelemetry/sdk/logs/logger_provider.h"
#  include "opentelemetry/sdk/resource/resource.h"
#  include "opentelemetry/test_common/ext/http/client/http_client_test_factory.h"
#  include "opentelemetry/test_common/ext/http/client/nosend/http_client_nosend.h"

#  include <google/protobuf/message_lite.h>
#  include <gtest/gtest.h>
#  include "gmock/gmock.h"

#  include "nlohmann/json.hpp"

#  if defined(_MSC_VER)
#    include "opentelemetry/sdk/common/env_variables.h"
using opentelemetry::sdk::common::setenv;
using opentelemetry::sdk::common::unsetenv;
#  endif

using namespace testing;

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace otlp
{

template <class T, size_t N>
static nostd::span<T, N> MakeSpan(T (&array)[N])
{
  return nostd::span<T, N>(array);
}

OtlpHttpClientOptions MakeOtlpHttpClientOptions(HttpRequestContentType content_type,
                                                bool async_mode)
{
  std::shared_ptr<opentelemetry::sdk::common::ThreadInstrumentation> not_instrumented;
  OtlpHttpLogRecordExporterOptions options;
  options.content_type  = content_type;
  options.console_debug = true;
  options.http_headers.insert(std::make_pair("Custom-Header-Key", "Custom-Header-Value"));
  options.retry_policy_max_attempts       = 0U;
  options.retry_policy_initial_backoff    = std::chrono::duration<float>::zero();
  options.retry_policy_max_backoff        = std::chrono::duration<float>::zero();
  options.retry_policy_backoff_multiplier = 0.0f;
  OtlpHttpClientOptions otlp_http_client_options(
      options.url, false, /* ssl_insecure_skip_verify */
      "",                 /* ssl_ca_cert_path */
      "",                 /* ssl_ca_cert_string */
      "",                 /* ssl_client_key_path */
      "",                 /* ssl_client_key_string */
      "",                 /* ssl_client_cert_path */
      "",                 /* ssl_client_cert_string */
      "",                 /* ssl_min_tls */
      "",                 /* ssl_max_tls */
      "",                 /* ssl_cipher */
      "",                 /* ssl_cipher_suite */
      options.content_type, options.json_bytes_mapping, options.compression, options.use_json_name,
      options.console_debug, options.timeout, options.http_headers,
      options.retry_policy_max_attempts, options.retry_policy_initial_backoff,
      options.retry_policy_max_backoff, options.retry_policy_backoff_multiplier, not_instrumented);
  if (!async_mode)
  {
    otlp_http_client_options.max_concurrent_requests = 0;
  }
  return otlp_http_client_options;
}

namespace http_client = opentelemetry::ext::http::client;

class OtlpHttpLogRecordExporterTestPeer : public ::testing::Test
{
public:
  std::unique_ptr<sdk::logs::LogRecordExporter> GetExporter(
      std::unique_ptr<OtlpHttpClient> http_client)
  {
    return std::unique_ptr<sdk::logs::LogRecordExporter>(
        new OtlpHttpLogRecordExporter(std::move(http_client)));
  }

  // Get the options associated with the given exporter.
  const OtlpHttpLogRecordExporterOptions &GetOptions(
      std::unique_ptr<OtlpHttpLogRecordExporter> &exporter)
  {
    return exporter->options_;
  }
  static std::pair<OtlpHttpClient *, std::shared_ptr<http_client::HttpClient>>
  GetMockOtlpHttpClient(HttpRequestContentType content_type, bool async_mode = false)
  {
    auto http_client = http_client::HttpClientTestFactory::Create();
    return {new OtlpHttpClient(MakeOtlpHttpClientOptions(content_type, async_mode), http_client),
            http_client};
  }

  void ExportJsonIntegrationTest()
  {
    auto mock_otlp_client =
        OtlpHttpLogRecordExporterTestPeer::GetMockOtlpHttpClient(HttpRequestContentType::kJson);
    auto mock_otlp_http_client = mock_otlp_client.first;
    auto client                = mock_otlp_client.second;
    auto exporter = GetExporter(std::unique_ptr<OtlpHttpClient>{mock_otlp_http_client});

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
            std::move(exporter), 5, std::chrono::milliseconds(256), 5)));

    std::string report_trace_id;
    std::string report_span_id;
    uint8_t trace_id_bin[opentelemetry::trace::TraceId::kSize] = {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    char trace_id_hex[2 * opentelemetry::trace::TraceId::kSize] = {0};
    opentelemetry::trace::TraceId trace_id{trace_id_bin};
    uint8_t span_id_bin[opentelemetry::trace::SpanId::kSize]  = {'7', '6', '5', '4',
                                                                 '3', '2', '1', '0'};
    char span_id_hex[2 * opentelemetry::trace::SpanId::kSize] = {0};
    opentelemetry::trace::SpanId span_id{span_id_bin};

    const std::string schema_url{"https://opentelemetry.io/schemas/1.2.0"};
    auto logger = provider->GetLogger("test", "opentelelemtry_library", "", schema_url,
                                      {{"scope_key1", "scope_value"}, {"scope_key2", 2}});

    trace_id.ToLowerBase16(MakeSpan(trace_id_hex));
    report_trace_id.assign(trace_id_hex, sizeof(trace_id_hex));

    span_id.ToLowerBase16(MakeSpan(span_id_hex));
    report_span_id.assign(span_id_hex, sizeof(span_id_hex));

    auto no_send_client = std::static_pointer_cast<http_client::nosend::HttpClient>(client);
    auto mock_session =
        std::static_pointer_cast<http_client::nosend::Session>(no_send_client->session_);
    EXPECT_CALL(*mock_session, SendRequest)
        .WillOnce(
            [&mock_session, report_trace_id, report_span_id](
                const std::shared_ptr<opentelemetry::ext::http::client::EventHandler> &callback) {
              auto check_json =
                  nlohmann::json::parse(mock_session->GetRequest()->body_, nullptr, false);
              auto resource_logs     = *check_json["resourceLogs"].begin();
              auto scope_logs        = *resource_logs["scopeLogs"].begin();
              auto scope             = scope_logs["scope"];
              auto log               = *scope_logs["logRecords"].begin();
              auto received_trace_id = log["traceId"].get<std::string>();
              auto received_span_id  = log["spanId"].get<std::string>();
              EXPECT_EQ(received_trace_id, report_trace_id);
              EXPECT_EQ(received_span_id, report_span_id);
              EXPECT_EQ("Log message", log["body"]["stringValue"].get<std::string>());
              EXPECT_LE(15, log["attributes"].size());
              auto custom_header = mock_session->GetRequest()->headers_.find("Custom-Header-Key");
              ASSERT_TRUE(custom_header != mock_session->GetRequest()->headers_.end());
              if (custom_header != mock_session->GetRequest()->headers_.end())
              {
                EXPECT_EQ("Custom-Header-Value", custom_header->second);
              }

              bool check_scope_attribute = false;
              auto scope_attributes      = scope["attributes"];
              for (auto &attribute : scope_attributes)
              {
                if (!attribute.is_object())
                {
                  continue;
                }
                if ("scope_key1" == attribute["key"])
                {
                  check_scope_attribute = true;
                  EXPECT_EQ("scope_value", attribute["value"]["stringValue"].get<std::string>());
                }
              }
              ASSERT_TRUE(check_scope_attribute);

              http_client::nosend::Response response;
              response.Finish(*callback.get());
            });

    logger->EmitLogRecord(
        opentelemetry::logs::Severity::kInfo, "Log message",
        opentelemetry::common::MakeAttributes(
            {{"service.name", "unit_test_service"},
             {"tenant.id", "test_user"},
             {"bool_value", true},
             {"int32_value", static_cast<int32_t>(1)},
             {"uint32_value", static_cast<uint32_t>(2)},
             {"int64_value", static_cast<int64_t>(0x1100000000LL)},
             {"uint64_value", static_cast<uint64_t>(0x1200000000ULL)},
             {"double_value", static_cast<double>(3.1)},
             {"vec_bool_value", attribute_storage_bool_value},
             {"vec_int32_value", attribute_storage_int32_value},
             {"vec_uint32_value", attribute_storage_uint32_value},
             {"vec_int64_value", attribute_storage_int64_value},
             {"vec_uint64_value", attribute_storage_uint64_value},
             {"vec_double_value", attribute_storage_double_value},
             {"vec_string_value", attribute_storage_string_value}}),
        trace_id, span_id,
        opentelemetry::trace::TraceFlags{opentelemetry::trace::TraceFlags::kIsSampled},
        std::chrono::system_clock::now());

    provider->ForceFlush();
  }

#  ifdef ENABLE_ASYNC_EXPORT
  void ExportJsonIntegrationTestAsync()
  {
    auto mock_otlp_client = OtlpHttpLogRecordExporterTestPeer::GetMockOtlpHttpClient(
        HttpRequestContentType::kJson, true);
    auto mock_otlp_http_client = mock_otlp_client.first;
    auto client                = mock_otlp_client.second;
    auto exporter = GetExporter(std::unique_ptr<OtlpHttpClient>{mock_otlp_http_client});

    bool attribute_storage_bool_value[]                                = {true, false, true};
    int32_t attribute_storage_int32_value[]                            = {1, 2};
    uint32_t attribute_storage_uint32_value[]                          = {3, 4};
    int64_t attribute_storage_int64_value[]                            = {5, 6};
    uint64_t attribute_storage_uint64_value[]                          = {7, 8};
    double attribute_storage_double_value[]                            = {3.2, 3.3};
    opentelemetry::nostd::string_view attribute_storage_string_value[] = {"vector", "string"};

    auto provider = nostd::shared_ptr<sdk::logs::LoggerProvider>(new sdk::logs::LoggerProvider());
    sdk::logs::BatchLogRecordProcessorOptions options;
    options.max_queue_size        = 5;
    options.schedule_delay_millis = std::chrono::milliseconds(256);
    options.max_export_batch_size = 5;

    provider->AddProcessor(std::unique_ptr<sdk::logs::LogRecordProcessor>(
        new sdk::logs::BatchLogRecordProcessor(std::move(exporter), options)));

    std::string report_trace_id;
    std::string report_span_id;
    uint8_t trace_id_bin[opentelemetry::trace::TraceId::kSize] = {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    char trace_id_hex[2 * opentelemetry::trace::TraceId::kSize] = {0};
    opentelemetry::trace::TraceId trace_id{trace_id_bin};
    uint8_t span_id_bin[opentelemetry::trace::SpanId::kSize]  = {'7', '6', '5', '4',
                                                                 '3', '2', '1', '0'};
    char span_id_hex[2 * opentelemetry::trace::SpanId::kSize] = {0};
    opentelemetry::trace::SpanId span_id{span_id_bin};

    const std::string schema_url{"https://opentelemetry.io/schemas/1.2.0"};
    auto logger = provider->GetLogger("test", "opentelelemtry_library", "1.2.0", schema_url,
                                      {{"scope_key1", "scope_value"}, {"scope_key2", 2}});

    trace_id.ToLowerBase16(MakeSpan(trace_id_hex));
    report_trace_id.assign(trace_id_hex, sizeof(trace_id_hex));

    span_id.ToLowerBase16(MakeSpan(span_id_hex));
    report_span_id.assign(span_id_hex, sizeof(span_id_hex));

    auto no_send_client = std::static_pointer_cast<http_client::nosend::HttpClient>(client);
    auto mock_session =
        std::static_pointer_cast<http_client::nosend::Session>(no_send_client->session_);
    EXPECT_CALL(*mock_session, SendRequest)
        .WillOnce(
            [&mock_session, report_trace_id, report_span_id](
                const std::shared_ptr<opentelemetry::ext::http::client::EventHandler> &callback) {
              auto check_json =
                  nlohmann::json::parse(mock_session->GetRequest()->body_, nullptr, false);
              auto resource_logs     = *check_json["resourceLogs"].begin();
              auto scope_logs        = *resource_logs["scopeLogs"].begin();
              auto schema_url        = scope_logs["schemaUrl"].get<std::string>();
              auto scope             = scope_logs["scope"];
              auto scope_name        = scope["name"];
              auto scope_version     = scope["version"];
              auto log               = *scope_logs["logRecords"].begin();
              auto received_trace_id = log["traceId"].get<std::string>();
              auto received_span_id  = log["spanId"].get<std::string>();
              EXPECT_EQ(schema_url, "https://opentelemetry.io/schemas/1.2.0");
              EXPECT_EQ(scope_name, "opentelelemtry_library");
              EXPECT_EQ(scope_version, "1.2.0");
              EXPECT_EQ(received_trace_id, report_trace_id);
              EXPECT_EQ(received_span_id, report_span_id);
              EXPECT_EQ("Log message", log["body"]["stringValue"].get<std::string>());
              EXPECT_LE(15, log["attributes"].size());
              auto custom_header = mock_session->GetRequest()->headers_.find("Custom-Header-Key");
              ASSERT_TRUE(custom_header != mock_session->GetRequest()->headers_.end());
              if (custom_header != mock_session->GetRequest()->headers_.end())
              {
                EXPECT_EQ("Custom-Header-Value", custom_header->second);
              }

              bool check_scope_attribute = false;
              auto scope_attributes      = scope["attributes"];
              for (auto &attribute : scope_attributes)
              {
                if (!attribute.is_object())
                {
                  continue;
                }
                if ("scope_key1" == attribute["key"])
                {
                  check_scope_attribute = true;
                  EXPECT_EQ("scope_value", attribute["value"]["stringValue"].get<std::string>());
                }
              }
              ASSERT_TRUE(check_scope_attribute);

              // let the otlp_http_client to continue
              std::thread async_finish{[callback]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                http_client::nosend::Response response;
                response.Finish(*callback.get());
              }};
              async_finish.detach();
            });

    logger->EmitLogRecord(
        opentelemetry::logs::Severity::kInfo, "Log message",
        opentelemetry::common::MakeAttributes(
            {{"service.name", "unit_test_service"},
             {"tenant.id", "test_user"},
             {"bool_value", true},
             {"int32_value", static_cast<int32_t>(1)},
             {"uint32_value", static_cast<uint32_t>(2)},
             {"int64_value", static_cast<int64_t>(0x1100000000LL)},
             {"uint64_value", static_cast<uint64_t>(0x1200000000ULL)},
             {"double_value", static_cast<double>(3.1)},
             {"vec_bool_value", attribute_storage_bool_value},
             {"vec_int32_value", attribute_storage_int32_value},
             {"vec_uint32_value", attribute_storage_uint32_value},
             {"vec_int64_value", attribute_storage_int64_value},
             {"vec_uint64_value", attribute_storage_uint64_value},
             {"vec_double_value", attribute_storage_double_value},
             {"vec_string_value", attribute_storage_string_value}}),
        trace_id, span_id,
        opentelemetry::trace::TraceFlags{opentelemetry::trace::TraceFlags::kIsSampled},
        std::chrono::system_clock::now());

    provider->ForceFlush();
  }
#  endif

  void ExportBinaryIntegrationTest()
  {
    auto mock_otlp_client =
        OtlpHttpLogRecordExporterTestPeer::GetMockOtlpHttpClient(HttpRequestContentType::kBinary);
    auto mock_otlp_http_client = mock_otlp_client.first;
    auto client                = mock_otlp_client.second;
    auto exporter = GetExporter(std::unique_ptr<OtlpHttpClient>{mock_otlp_http_client});

    bool attribute_storage_bool_value[]                                = {true, false, true};
    int32_t attribute_storage_int32_value[]                            = {1, 2};
    uint32_t attribute_storage_uint32_value[]                          = {3, 4};
    int64_t attribute_storage_int64_value[]                            = {5, 6};
    uint64_t attribute_storage_uint64_value[]                          = {7, 8};
    double attribute_storage_double_value[]                            = {3.2, 3.3};
    opentelemetry::nostd::string_view attribute_storage_string_value[] = {"vector", "string"};

    auto provider = nostd::shared_ptr<sdk::logs::LoggerProvider>(new sdk::logs::LoggerProvider());
    sdk::logs::BatchLogRecordProcessorOptions processor_options;
    processor_options.max_export_batch_size = 5;
    processor_options.max_queue_size        = 5;
    processor_options.schedule_delay_millis = std::chrono::milliseconds(256);
    provider->AddProcessor(std::unique_ptr<sdk::logs::LogRecordProcessor>(
        new sdk::logs::BatchLogRecordProcessor(std::move(exporter), processor_options)));

    std::string report_trace_id;
    std::string report_span_id;
    uint8_t trace_id_bin[opentelemetry::trace::TraceId::kSize] = {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    opentelemetry::trace::TraceId trace_id{trace_id_bin};
    uint8_t span_id_bin[opentelemetry::trace::SpanId::kSize] = {'7', '6', '5', '4',
                                                                '3', '2', '1', '0'};
    opentelemetry::trace::SpanId span_id{span_id_bin};

    const std::string schema_url{"https://opentelemetry.io/schemas/1.2.0"};
    auto logger = provider->GetLogger("test", "opentelelemtry_library", "1.2.0", schema_url,
                                      {{"scope_key1", "scope_value"}, {"scope_key2", 2}});

    report_trace_id.assign(reinterpret_cast<const char *>(trace_id_bin), sizeof(trace_id_bin));
    report_span_id.assign(reinterpret_cast<const char *>(span_id_bin), sizeof(span_id_bin));

    auto no_send_client = std::static_pointer_cast<http_client::nosend::HttpClient>(client);
    auto mock_session =
        std::static_pointer_cast<http_client::nosend::Session>(no_send_client->session_);
    EXPECT_CALL(*mock_session, SendRequest)
        .WillOnce(
            [&mock_session, report_trace_id, report_span_id](
                const std::shared_ptr<opentelemetry::ext::http::client::EventHandler> &callback) {
              opentelemetry::proto::collector::logs::v1::ExportLogsServiceRequest request_body;
              request_body.ParseFromArray(
                  &mock_session->GetRequest()->body_[0],
                  static_cast<int>(mock_session->GetRequest()->body_.size()));
              auto scope_log = request_body.resource_logs(0).scope_logs(0);
              EXPECT_EQ(scope_log.schema_url(), "https://opentelemetry.io/schemas/1.2.0");
              EXPECT_EQ(scope_log.scope().name(), "opentelelemtry_library");
              EXPECT_EQ(scope_log.scope().version(), "1.2.0");
              const auto &received_log = scope_log.log_records(0);
              EXPECT_EQ(received_log.trace_id(), report_trace_id);
              EXPECT_EQ(received_log.span_id(), report_span_id);
              EXPECT_EQ("Log message", received_log.body().string_value());
              EXPECT_LE(15, received_log.attributes_size());
              bool check_service_name = false;
              for (auto &attribute : received_log.attributes())
              {
                if ("service.name" == attribute.key())
                {
                  check_service_name = true;
                  EXPECT_EQ("unit_test_service", attribute.value().string_value());
                }
              }
              ASSERT_TRUE(check_service_name);

              bool check_scope_attribute = false;
              for (auto &attribute : scope_log.scope().attributes())
              {
                if ("scope_key1" == attribute.key())
                {
                  check_scope_attribute = true;
                  EXPECT_EQ("scope_value", attribute.value().string_value());
                }
              }
              ASSERT_TRUE(check_scope_attribute);

              // let the otlp_http_client to continue

              http_client::nosend::Response response;
              response.Finish(*callback.get());
            });

    logger->EmitLogRecord(
        opentelemetry::logs::Severity::kInfo, "Log message",
        opentelemetry::common::MakeAttributes(
            {{"service.name", "unit_test_service"},
             {"tenant.id", "test_user"},
             {"bool_value", true},
             {"int32_value", static_cast<int32_t>(1)},
             {"uint32_value", static_cast<uint32_t>(2)},
             {"int64_value", static_cast<int64_t>(0x1100000000LL)},
             {"uint64_value", static_cast<uint64_t>(0x1200000000ULL)},
             {"double_value", static_cast<double>(3.1)},
             {"vec_bool_value", attribute_storage_bool_value},
             {"vec_int32_value", attribute_storage_int32_value},
             {"vec_uint32_value", attribute_storage_uint32_value},
             {"vec_int64_value", attribute_storage_int64_value},
             {"vec_uint64_value", attribute_storage_uint64_value},
             {"vec_double_value", attribute_storage_double_value},
             {"vec_string_value", attribute_storage_string_value}}),
        trace_id, span_id,
        opentelemetry::trace::TraceFlags{opentelemetry::trace::TraceFlags::kIsSampled},
        std::chrono::system_clock::now());

    provider->ForceFlush();
  }

#  ifdef ENABLE_ASYNC_EXPORT
  void ExportBinaryIntegrationTestAsync()
  {
    auto mock_otlp_client = OtlpHttpLogRecordExporterTestPeer::GetMockOtlpHttpClient(
        HttpRequestContentType::kBinary, true);
    auto mock_otlp_http_client = mock_otlp_client.first;
    auto client                = mock_otlp_client.second;
    auto exporter = GetExporter(std::unique_ptr<OtlpHttpClient>{mock_otlp_http_client});

    bool attribute_storage_bool_value[]                                = {true, false, true};
    int32_t attribute_storage_int32_value[]                            = {1, 2};
    uint32_t attribute_storage_uint32_value[]                          = {3, 4};
    int64_t attribute_storage_int64_value[]                            = {5, 6};
    uint64_t attribute_storage_uint64_value[]                          = {7, 8};
    double attribute_storage_double_value[]                            = {3.2, 3.3};
    opentelemetry::nostd::string_view attribute_storage_string_value[] = {"vector", "string"};

    auto provider = nostd::shared_ptr<sdk::logs::LoggerProvider>(new sdk::logs::LoggerProvider());

    sdk::logs::BatchLogRecordProcessorOptions processor_options;
    processor_options.max_export_batch_size = 5;
    processor_options.max_queue_size        = 5;
    processor_options.schedule_delay_millis = std::chrono::milliseconds(256);
    provider->AddProcessor(std::unique_ptr<sdk::logs::LogRecordProcessor>(
        new sdk::logs::BatchLogRecordProcessor(std::move(exporter), processor_options)));

    std::string report_trace_id;
    std::string report_span_id;
    uint8_t trace_id_bin[opentelemetry::trace::TraceId::kSize] = {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    opentelemetry::trace::TraceId trace_id{trace_id_bin};
    uint8_t span_id_bin[opentelemetry::trace::SpanId::kSize] = {'7', '6', '5', '4',
                                                                '3', '2', '1', '0'};
    opentelemetry::trace::SpanId span_id{span_id_bin};

    const std::string schema_url{"https://opentelemetry.io/schemas/1.2.0"};
    auto logger = provider->GetLogger("test", "opentelelemtry_library", "", schema_url,
                                      {{"scope_key1", "scope_value"}, {"scope_key2", 2}});

    report_trace_id.assign(reinterpret_cast<const char *>(trace_id_bin), sizeof(trace_id_bin));
    report_span_id.assign(reinterpret_cast<const char *>(span_id_bin), sizeof(span_id_bin));

    auto no_send_client = std::static_pointer_cast<http_client::nosend::HttpClient>(client);
    auto mock_session =
        std::static_pointer_cast<http_client::nosend::Session>(no_send_client->session_);
    EXPECT_CALL(*mock_session, SendRequest)
        .WillOnce(
            [&mock_session, report_trace_id, report_span_id, schema_url](
                const std::shared_ptr<opentelemetry::ext::http::client::EventHandler> &callback) {
              opentelemetry::proto::collector::logs::v1::ExportLogsServiceRequest request_body;
              request_body.ParseFromArray(
                  &mock_session->GetRequest()->body_[0],
                  static_cast<int>(mock_session->GetRequest()->body_.size()));
              auto &scope_log   = request_body.resource_logs(0).scope_logs(0);
              auto received_log = scope_log.log_records(0);
              EXPECT_EQ(received_log.trace_id(), report_trace_id);
              EXPECT_EQ(received_log.span_id(), report_span_id);
              EXPECT_EQ("Log message", received_log.body().string_value());
              EXPECT_LE(15, received_log.attributes_size());
              bool check_service_name = false;
              for (auto &attribute : received_log.attributes())
              {
                if ("service.name" == attribute.key())
                {
                  check_service_name = true;
                  EXPECT_EQ("unit_test_service", attribute.value().string_value());
                }
              }
              ASSERT_TRUE(check_service_name);

              auto &scope = scope_log.scope();
              EXPECT_EQ(scope.name(), "opentelelemtry_library");
              EXPECT_EQ(scope_log.schema_url(), schema_url);
              bool check_scope_attribute = false;
              for (auto &attribute : scope.attributes())
              {
                if ("scope_key1" == attribute.key())
                {
                  check_scope_attribute = true;
                  EXPECT_EQ("scope_value", attribute.value().string_value());
                }
              }
              ASSERT_TRUE(check_scope_attribute);

              // let the otlp_http_client to continue

              std::thread async_finish{[callback]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                http_client::nosend::Response response;
                response.Finish(*callback.get());
              }};
              async_finish.detach();
            });

    logger->EmitLogRecord(
        opentelemetry::logs::Severity::kInfo, "Log message",
        opentelemetry::common::MakeAttributes(
            {{"service.name", "unit_test_service"},
             {"tenant.id", "test_user"},
             {"bool_value", true},
             {"int32_value", static_cast<int32_t>(1)},
             {"uint32_value", static_cast<uint32_t>(2)},
             {"int64_value", static_cast<int64_t>(0x1100000000LL)},
             {"uint64_value", static_cast<uint64_t>(0x1200000000ULL)},
             {"double_value", static_cast<double>(3.1)},
             {"vec_bool_value", attribute_storage_bool_value},
             {"vec_int32_value", attribute_storage_int32_value},
             {"vec_uint32_value", attribute_storage_uint32_value},
             {"vec_int64_value", attribute_storage_int64_value},
             {"vec_uint64_value", attribute_storage_uint64_value},
             {"vec_double_value", attribute_storage_double_value},
             {"vec_string_value", attribute_storage_string_value}}),
        trace_id, span_id,
        opentelemetry::trace::TraceFlags{opentelemetry::trace::TraceFlags::kIsSampled},
        std::chrono::system_clock::now());

    provider->ForceFlush();
  }
#  endif
};

TEST(OtlpHttpLogRecordExporterTest, Shutdown)
{
  auto exporter =
      std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter>(new OtlpHttpLogRecordExporter());
  ASSERT_TRUE(exporter->Shutdown());

  nostd::span<std::unique_ptr<opentelemetry::sdk::logs::Recordable>> logs = {};

  auto result = exporter->Export(logs);
  EXPECT_EQ(result, opentelemetry::sdk::common::ExportResult::kFailure);
}

// Create log records, let processor call Export()
TEST_F(OtlpHttpLogRecordExporterTestPeer, ExportJsonIntegrationTestSync)
{
  ExportJsonIntegrationTest();
}

#  ifdef ENABLE_ASYNC_EXPORT
TEST_F(OtlpHttpLogRecordExporterTestPeer, ExportJsonIntegrationTestAsync)
{
  ExportJsonIntegrationTestAsync();
  google::protobuf::ShutdownProtobufLibrary();
}
#  endif

// Create log records, let processor call Export()
TEST_F(OtlpHttpLogRecordExporterTestPeer, ExportBinaryIntegrationTestSync)
{
  ExportBinaryIntegrationTest();
}

#  ifdef ENABLE_ASYNC_EXPORT
TEST_F(OtlpHttpLogRecordExporterTestPeer, ExportBinaryIntegrationTestAsync)
{
  ExportBinaryIntegrationTestAsync();
}
#  endif

// Test exporter configuration options
TEST_F(OtlpHttpLogRecordExporterTestPeer, ConfigTest)
{
  OtlpHttpLogRecordExporterOptions opts;
  opts.url = "http://localhost:45456/v1/logs";
  std::unique_ptr<OtlpHttpLogRecordExporter> exporter(new OtlpHttpLogRecordExporter(opts));
  EXPECT_EQ(GetOptions(exporter).url, "http://localhost:45456/v1/logs");
}

// Test exporter configuration options with use_json_name
TEST_F(OtlpHttpLogRecordExporterTestPeer, ConfigUseJsonNameTest)
{
  OtlpHttpLogRecordExporterOptions opts;
  opts.use_json_name = true;
  std::unique_ptr<OtlpHttpLogRecordExporter> exporter(new OtlpHttpLogRecordExporter(opts));
  EXPECT_EQ(GetOptions(exporter).use_json_name, true);
}

// Test exporter configuration options with json_bytes_mapping=JsonBytesMappingKind::kHex
TEST_F(OtlpHttpLogRecordExporterTestPeer, ConfigJsonBytesMappingTest)
{
  OtlpHttpLogRecordExporterOptions opts;
  opts.json_bytes_mapping = JsonBytesMappingKind::kHex;
  std::unique_ptr<OtlpHttpLogRecordExporter> exporter(new OtlpHttpLogRecordExporter(opts));
  EXPECT_EQ(GetOptions(exporter).json_bytes_mapping, JsonBytesMappingKind::kHex);
}

TEST(OtlpHttpLogRecordExporterTest, ConfigDefaultProtocolTest)
{
  OtlpHttpLogRecordExporterOptions opts;
  EXPECT_EQ(opts.content_type, HttpRequestContentType::kBinary);
}

#  ifndef NO_GETENV
// Test exporter configuration options with use_ssl_credentials
TEST_F(OtlpHttpLogRecordExporterTestPeer, ConfigFromEnv)
{
  const std::string url = "http://localhost:9999/v1/logs";
  setenv("OTEL_EXPORTER_OTLP_ENDPOINT", "http://localhost:9999", 1);
  setenv("OTEL_EXPORTER_OTLP_TIMEOUT", "20s", 1);
  setenv("OTEL_EXPORTER_OTLP_HEADERS", "k1=v1,k2=v2", 1);
  setenv("OTEL_EXPORTER_OTLP_LOGS_HEADERS", "k1=v3,k1=v4", 1);
  setenv("OTEL_EXPORTER_OTLP_PROTOCOL", "http/json", 1);

  std::unique_ptr<OtlpHttpLogRecordExporter> exporter(new OtlpHttpLogRecordExporter());
  EXPECT_EQ(GetOptions(exporter).url, url);
  EXPECT_EQ(
      GetOptions(exporter).timeout.count(),
      std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::seconds{20})
          .count());
  EXPECT_EQ(GetOptions(exporter).http_headers.size(), 3);
  {
    // Test k2
    auto range = GetOptions(exporter).http_headers.equal_range("k2");
    EXPECT_TRUE(range.first != range.second);
    EXPECT_EQ(range.first->second, std::string("v2"));
    ++range.first;
    EXPECT_TRUE(range.first == range.second);
  }
  {
    // k1
    auto range = GetOptions(exporter).http_headers.equal_range("k1");
    EXPECT_TRUE(range.first != range.second);
    EXPECT_EQ(range.first->second, std::string("v3"));
    ++range.first;
    EXPECT_EQ(range.first->second, std::string("v4"));
    ++range.first;
    EXPECT_TRUE(range.first == range.second);
  }
  EXPECT_EQ(GetOptions(exporter).content_type, HttpRequestContentType::kJson);

  unsetenv("OTEL_EXPORTER_OTLP_ENDPOINT");
  unsetenv("OTEL_EXPORTER_OTLP_TIMEOUT");
  unsetenv("OTEL_EXPORTER_OTLP_HEADERS");
  unsetenv("OTEL_EXPORTER_OTLP_LOGS_HEADERS");
  unsetenv("OTEL_EXPORTER_OTLP_PROTOCOL");
}

TEST_F(OtlpHttpLogRecordExporterTestPeer, ConfigFromLogsEnv)
{
  const std::string url = "http://localhost:9999/v1/logs";
  setenv("OTEL_EXPORTER_OTLP_LOGS_ENDPOINT", url.c_str(), 1);
  setenv("OTEL_EXPORTER_OTLP_LOGS_TIMEOUT", "20s", 1);
  setenv("OTEL_EXPORTER_OTLP_HEADERS", "k1=v1,k2=v2", 1);
  setenv("OTEL_EXPORTER_OTLP_LOGS_HEADERS", "k1=v3,k1=v4", 1);
  setenv("OTEL_EXPORTER_OTLP_LOGS_PROTOCOL", "http/json", 1);

  std::unique_ptr<OtlpHttpLogRecordExporter> exporter(new OtlpHttpLogRecordExporter());
  EXPECT_EQ(GetOptions(exporter).url, url);
  EXPECT_EQ(
      GetOptions(exporter).timeout.count(),
      std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::seconds{20})
          .count());
  EXPECT_EQ(GetOptions(exporter).http_headers.size(), 3);
  {
    // Test k2
    auto range = GetOptions(exporter).http_headers.equal_range("k2");
    EXPECT_TRUE(range.first != range.second);
    EXPECT_EQ(range.first->second, std::string("v2"));
    ++range.first;
    EXPECT_TRUE(range.first == range.second);
  }
  {
    // k1
    auto range = GetOptions(exporter).http_headers.equal_range("k1");
    EXPECT_TRUE(range.first != range.second);
    EXPECT_EQ(range.first->second, std::string("v3"));
    ++range.first;
    EXPECT_EQ(range.first->second, std::string("v4"));
    ++range.first;
    EXPECT_TRUE(range.first == range.second);
  }
  EXPECT_EQ(GetOptions(exporter).content_type, HttpRequestContentType::kJson);

  unsetenv("OTEL_EXPORTER_OTLP_LOGS_ENDPOINT");
  unsetenv("OTEL_EXPORTER_OTLP_LOGS_TIMEOUT");
  unsetenv("OTEL_EXPORTER_OTLP_HEADERS");
  unsetenv("OTEL_EXPORTER_OTLP_LOGS_HEADERS");
  unsetenv("OTEL_EXPORTER_OTLP_LOGS_PROTOCOL");
}

TEST_F(OtlpHttpLogRecordExporterTestPeer, DefaultEndpoint)
{
  EXPECT_EQ("http://localhost:4318/v1/logs", GetOtlpDefaultHttpLogsEndpoint());
  EXPECT_EQ("http://localhost:4318/v1/traces", GetOtlpDefaultHttpEndpoint());
  EXPECT_EQ("http://localhost:4317", GetOtlpDefaultGrpcEndpoint());
}

TEST_F(OtlpHttpLogRecordExporterTestPeer, ConfigRetryDefaultValues)
{
  std::unique_ptr<OtlpHttpLogRecordExporter> exporter(new OtlpHttpLogRecordExporter());
  const auto options = GetOptions(exporter);
  ASSERT_EQ(options.retry_policy_max_attempts, 5);
  ASSERT_FLOAT_EQ(options.retry_policy_initial_backoff.count(), 1.0f);
  ASSERT_FLOAT_EQ(options.retry_policy_max_backoff.count(), 5.0f);
  ASSERT_FLOAT_EQ(options.retry_policy_backoff_multiplier, 1.5f);
}

TEST_F(OtlpHttpLogRecordExporterTestPeer, ConfigRetryValuesFromEnv)
{
  setenv("OTEL_CPP_EXPORTER_OTLP_LOGS_RETRY_MAX_ATTEMPTS", "123", 1);
  setenv("OTEL_CPP_EXPORTER_OTLP_LOGS_RETRY_INITIAL_BACKOFF", "4.5", 1);
  setenv("OTEL_CPP_EXPORTER_OTLP_LOGS_RETRY_MAX_BACKOFF", "6.7", 1);
  setenv("OTEL_CPP_EXPORTER_OTLP_LOGS_RETRY_BACKOFF_MULTIPLIER", "8.9", 1);

  std::unique_ptr<OtlpHttpLogRecordExporter> exporter(new OtlpHttpLogRecordExporter());
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

TEST_F(OtlpHttpLogRecordExporterTestPeer, ConfigRetryGenericValuesFromEnv)
{
  setenv("OTEL_CPP_EXPORTER_OTLP_RETRY_MAX_ATTEMPTS", "321", 1);
  setenv("OTEL_CPP_EXPORTER_OTLP_RETRY_INITIAL_BACKOFF", "5.4", 1);
  setenv("OTEL_CPP_EXPORTER_OTLP_RETRY_MAX_BACKOFF", "7.6", 1);
  setenv("OTEL_CPP_EXPORTER_OTLP_RETRY_BACKOFF_MULTIPLIER", "9.8", 1);

  std::unique_ptr<OtlpHttpLogRecordExporter> exporter(new OtlpHttpLogRecordExporter());
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
#  endif  // NO_GETENV

}  // namespace otlp
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
#endif /* OPENTELEMETRY_STL_VERSION */
