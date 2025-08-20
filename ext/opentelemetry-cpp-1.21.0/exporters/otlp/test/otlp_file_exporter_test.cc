// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <stddef.h>
#include <chrono>
#include <cstdint>
#include <functional>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "opentelemetry/exporters/otlp/otlp_file_client_options.h"
#include "opentelemetry/exporters/otlp/otlp_file_exporter.h"
#include "opentelemetry/exporters/otlp/otlp_file_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_file_exporter_options.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/span.h"
#include "opentelemetry/nostd/variant.h"
#include "opentelemetry/sdk/common/exporter_utils.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/sdk/trace/batch_span_processor.h"
#include "opentelemetry/sdk/trace/batch_span_processor_options.h"
#include "opentelemetry/sdk/trace/exporter.h"
#include "opentelemetry/sdk/trace/processor.h"
#include "opentelemetry/sdk/trace/recordable.h"
#include "opentelemetry/sdk/trace/tracer_provider.h"
#include "opentelemetry/trace/span.h"
#include "opentelemetry/trace/span_context.h"
#include "opentelemetry/trace/span_startoptions.h"
#include "opentelemetry/trace/trace_id.h"
#include "opentelemetry/trace/tracer.h"
#include "opentelemetry/version.h"

// clang-format off
#include "opentelemetry/exporters/otlp/protobuf_include_prefix.h" // IWYU pragma: keep
#include "google/protobuf/message_lite.h"
#include "opentelemetry/exporters/otlp/protobuf_include_suffix.h" // IWYU pragma: keep
// clang-format on

using namespace testing;

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace otlp
{

namespace trace_api = opentelemetry::trace;
namespace resource  = opentelemetry::sdk::resource;

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

class OtlpFileExporterTestPeer : public ::testing::Test
{
public:
  void ExportJsonIntegrationTest()
  {
    static ProtobufGlobalSymbolGuard global_symbol_guard;

    std::stringstream output;
    OtlpFileExporterOptions opts;
    opts.backend_options = std::ref(output);

    auto exporter = OtlpFileExporterFactory::Create(opts);

    resource::ResourceAttributes resource_attributes = {{"service.name", "unit_test_service"},
                                                        {"tenant.id", "test_user"}};
    resource_attributes["bool_value"]                = true;
    resource_attributes["int32_value"]               = static_cast<int32_t>(1);
    resource_attributes["uint32_value"]              = static_cast<uint32_t>(2);
    resource_attributes["int64_value"]               = static_cast<int64_t>(0x1100000000LL);
    resource_attributes["uint64_value"]              = static_cast<uint64_t>(0x1200000000ULL);
    resource_attributes["double_value"]              = static_cast<double>(3.1);
    resource_attributes["vec_bool_value"]            = std::vector<bool>{true, false, true};
    resource_attributes["vec_int32_value"]           = std::vector<int32_t>{1, 2};
    resource_attributes["vec_uint32_value"]          = std::vector<uint32_t>{3, 4};
    resource_attributes["vec_int64_value"]           = std::vector<int64_t>{5, 6};
    resource_attributes["vec_uint64_value"]          = std::vector<uint64_t>{7, 8};
    resource_attributes["vec_double_value"]          = std::vector<double>{3.2, 3.3};
    resource_attributes["vec_string_value"]          = std::vector<std::string>{"vector", "string"};
    auto resource = resource::Resource::Create(resource_attributes, "resource_url");

    auto processor_opts                  = sdk::trace::BatchSpanProcessorOptions();
    processor_opts.max_export_batch_size = 5;
    processor_opts.max_queue_size        = 5;
    processor_opts.schedule_delay_millis = std::chrono::milliseconds(256);

    auto processor = std::unique_ptr<sdk::trace::SpanProcessor>(
        new sdk::trace::BatchSpanProcessor(std::move(exporter), processor_opts));
    auto provider = nostd::shared_ptr<sdk::trace::TracerProvider>(
        new sdk::trace::TracerProvider(std::move(processor), resource));

    std::string report_trace_id;

#if OPENTELEMETRY_ABI_VERSION_NO >= 2
    std::unordered_map<std::string, common::AttributeValue> scope_attributes;
    scope_attributes["scope_key"] = common::AttributeValue("scope_value");
    auto tracer = provider->GetTracer("scope_name", "scope_version", "scope_url", scope_attributes);
#else
    auto tracer = provider->GetTracer("scope_name", "scope_version", "scope_url");
#endif

    auto parent_span = tracer->StartSpan("Test parent span");

    char trace_id_hex[2 * trace_api::TraceId::kSize] = {0};

    trace_api::StartSpanOptions child_span_opts = {};
    child_span_opts.parent                      = parent_span->GetContext();

    auto child_span = tracer->StartSpan("Test child span", child_span_opts);

    nostd::get<trace_api::SpanContext>(child_span_opts.parent)
        .trace_id()
        .ToLowerBase16(MakeSpan(trace_id_hex));
    report_trace_id.assign(trace_id_hex, sizeof(trace_id_hex));

    child_span->End();
    parent_span->End();

    provider->ForceFlush();

    output.flush();
    output.sync();
    auto check_json_text = output.str();
    if (!check_json_text.empty())
    {
      // If the exporting is splited to two standalone resource_span, just checking the first one.
      std::string::size_type eol = check_json_text.find('\n');
      if (eol != std::string::npos)
      {
        check_json_text = check_json_text.substr(0, eol);
      }
      auto check_json = nlohmann::json::parse(check_json_text, nullptr, false);
      if (!check_json.is_discarded())
      {
        auto resource_span = *check_json["resourceSpans"].begin();
        auto scope_span    = *resource_span["scopeSpans"].begin();
        auto scope         = scope_span["scope"];
        auto span          = *scope_span["spans"].begin();

#if OPENTELEMETRY_ABI_VERSION_NO >= 2
        ASSERT_EQ(1, scope["attributes"].size());
        const auto scope_attribute = scope["attributes"].front();
        EXPECT_EQ("scope_key", scope_attribute["key"].get<std::string>());
        EXPECT_EQ("scope_value", scope_attribute["value"]["stringValue"].get<std::string>());
#endif
        EXPECT_EQ("resource_url", resource_span["schemaUrl"].get<std::string>());
        EXPECT_EQ("scope_url", scope_span["schemaUrl"].get<std::string>());
        EXPECT_EQ("scope_name", scope["name"].get<std::string>());
        EXPECT_EQ("scope_version", scope["version"].get<std::string>());
        EXPECT_EQ(report_trace_id, span["traceId"].get<std::string>());
      }
      else
      {
        FAIL() << "Failed to parse json:" << check_json_text;
      }
    }
  }
};

TEST(OtlpFileExporterTest, Shutdown)
{
  auto exporter = std::unique_ptr<opentelemetry::sdk::trace::SpanExporter>(new OtlpFileExporter());
  ASSERT_TRUE(exporter->Shutdown());

  nostd::span<std::unique_ptr<opentelemetry::sdk::trace::Recordable>> spans = {};

  auto result = exporter->Export(spans);
  EXPECT_EQ(result, opentelemetry::sdk::common::ExportResult::kFailure);
}

// Create spans, let processor call Export()
TEST_F(OtlpFileExporterTestPeer, ExportJsonIntegrationTestSync)
{
  ExportJsonIntegrationTest();
}

}  // namespace otlp
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
