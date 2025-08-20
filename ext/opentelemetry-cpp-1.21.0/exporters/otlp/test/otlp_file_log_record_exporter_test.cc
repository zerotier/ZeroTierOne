// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <stddef.h>
#include <stdint.h>
#include <chrono>
#include <functional>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <utility>

#include "opentelemetry/common/key_value_iterable_view.h"
#include "opentelemetry/exporters/otlp/otlp_file_client_options.h"
#include "opentelemetry/exporters/otlp/otlp_file_log_record_exporter.h"
#include "opentelemetry/exporters/otlp/otlp_file_log_record_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_file_log_record_exporter_options.h"
#include "opentelemetry/logs/logger.h"
#include "opentelemetry/logs/severity.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/span.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/sdk/common/exporter_utils.h"
#include "opentelemetry/sdk/logs/batch_log_record_processor.h"
#include "opentelemetry/sdk/logs/exporter.h"
#include "opentelemetry/sdk/logs/logger_provider.h"
#include "opentelemetry/sdk/logs/processor.h"
#include "opentelemetry/sdk/logs/recordable.h"
#include "opentelemetry/trace/span_id.h"
#include "opentelemetry/trace/trace_flags.h"
#include "opentelemetry/trace/trace_id.h"
#include "opentelemetry/version.h"

// clang-format off
#include "opentelemetry/exporters/otlp/protobuf_include_prefix.h" // IWYU pragma: keep
#include <google/protobuf/message_lite.h>
#include "opentelemetry/exporters/otlp/protobuf_include_suffix.h" // IWYU pragma: keep
// clang-format on

using namespace testing;

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace otlp
{

class ProtobufGlobalSymbolGuard
{
public:
  ProtobufGlobalSymbolGuard() {}
  ~ProtobufGlobalSymbolGuard() { google::protobuf::ShutdownProtobufLibrary(); }
};

template <class T, size_t N>
static nostd::span<T, N> MakeSpan(T (&array)[N])
{
  return nostd::span<T, N>(array);
}

class OtlpFileLogRecordExporterTestPeer : public ::testing::Test
{
public:
  void ExportJsonIntegrationTest()
  {
    static ProtobufGlobalSymbolGuard global_symbol_guard;

    std::stringstream output;
    OtlpFileLogRecordExporterOptions opts;
    opts.backend_options = std::ref(output);

    auto exporter = OtlpFileLogRecordExporterFactory::Create(opts);

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
    uint8_t span_id_bin[opentelemetry::trace::SpanId::kSize] = {
        // Fix clang-format 10 has different behavior for this line
        '7', '6', '5', '4', '3', '2', '1', '0'};
    char span_id_hex[2 * opentelemetry::trace::SpanId::kSize] = {0};
    opentelemetry::trace::SpanId span_id{span_id_bin};

    const std::string schema_url{"https://opentelemetry.io/schemas/1.2.0"};
    auto logger = provider->GetLogger("test", "opentelelemtry_library", "", schema_url,
                                      {{"scope_key1", "scope_value"}, {"scope_key2", 2}});

    trace_id.ToLowerBase16(MakeSpan(trace_id_hex));
    report_trace_id.assign(trace_id_hex, sizeof(trace_id_hex));

    span_id.ToLowerBase16(MakeSpan(span_id_hex));
    report_span_id.assign(span_id_hex, sizeof(span_id_hex));
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

    output.flush();
    output.sync();
    auto check_json_text = output.str();
    if (!check_json_text.empty())
    {
      auto check_json        = nlohmann::json::parse(check_json_text, nullptr, false);
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
    }
  }
};

TEST(OtlpFileLogRecordExporterTest, Shutdown)
{
  auto exporter =
      std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter>(new OtlpFileLogRecordExporter());
  ASSERT_TRUE(exporter->Shutdown());

  nostd::span<std::unique_ptr<opentelemetry::sdk::logs::Recordable>> logs = {};

  auto result = exporter->Export(logs);
  EXPECT_EQ(result, opentelemetry::sdk::common::ExportResult::kFailure);
}

// Create log records, let processor call Export()
TEST_F(OtlpFileLogRecordExporterTestPeer, ExportJsonIntegrationTestSync)
{
  ExportJsonIntegrationTest();
}

}  // namespace otlp
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
