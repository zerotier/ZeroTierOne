// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <functional>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

// IWYU pragma: no_include <features.h>

#include "opentelemetry/common/timestamp.h"
#include "opentelemetry/exporters/otlp/otlp_file_client.h"
#include "opentelemetry/exporters/otlp/otlp_file_client_options.h"
#include "opentelemetry/exporters/otlp/otlp_file_client_runtime_options.h"
#include "opentelemetry/exporters/otlp/otlp_recordable.h"
#include "opentelemetry/exporters/otlp/otlp_recordable_utils.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/span.h"
#include "opentelemetry/nostd/unique_ptr.h"
#include "opentelemetry/nostd/variant.h"
#include "opentelemetry/sdk/common/exporter_utils.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/sdk/trace/recordable.h"
#include "opentelemetry/trace/span_context.h"
#include "opentelemetry/trace/span_id.h"
#include "opentelemetry/trace/span_metadata.h"
#include "opentelemetry/trace/trace_flags.h"
#include "opentelemetry/trace/trace_id.h"
#include "opentelemetry/trace/trace_state.h"
#include "opentelemetry/version.h"

// clang-format off
#include "opentelemetry/exporters/otlp/protobuf_include_prefix.h" // IWYU pragma: keep
#include <google/protobuf/message_lite.h>
#include "opentelemetry/proto/collector/trace/v1/trace_service.pb.h"
#include "opentelemetry/proto/trace/v1/trace.pb.h"
#include "opentelemetry/exporters/otlp/protobuf_include_suffix.h" // IWYU pragma: keep
// clang-format on

using namespace testing;

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace otlp
{

namespace resource = opentelemetry::sdk::resource;

class ProtobufGlobalSymbolGuard
{
public:
  ProtobufGlobalSymbolGuard() {}
  ~ProtobufGlobalSymbolGuard() { google::protobuf::ShutdownProtobufLibrary(); }
};

static std::tm GetLocalTime(std::chrono::system_clock::time_point tp)
{
  std::time_t now = std::chrono::system_clock::to_time_t(tp);
#if (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L) || defined(__STDC_LIB_EXT1__)
  std::tm ret;
  localtime_s(&now, &ret);
#elif defined(_MSC_VER) && _MSC_VER >= 1300
  std::tm ret;
  localtime_s(&ret, &now);
#elif defined(_XOPEN_SOURCE) || defined(_BSD_SOURCE) || defined(_SVID_SOURCE) || \
    defined(_POSIX_SOURCE)
  std::tm ret;
  localtime_r(&now, &ret);
#else
  std::tm ret = *localtime(&now);
#endif
  return ret;
}

template <class T, size_t N>
static nostd::span<T, N> MakeSpan(T (&array)[N])
{
  return nostd::span<T, N>(array);
}

static resource::Resource MakeResource()
{
  static ProtobufGlobalSymbolGuard global_symbol_guard;

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

  return resource::Resource::Create(resource_attributes);
}

static opentelemetry::nostd::unique_ptr<
    opentelemetry::sdk::instrumentationscope::InstrumentationScope>
MakeInstrumentationScope()
{
  return opentelemetry::sdk::instrumentationscope::InstrumentationScope::Create(
      "otlp_file_client_test", "1.11.0", "https://opentelemetry.io/schemas/1.11.0");
}

static std::unique_ptr<opentelemetry::sdk::trace::Recordable> MakeRecordable(
    const resource::Resource &resource,
    const opentelemetry::sdk::instrumentationscope::InstrumentationScope &instrumentation_scope)
{
  OtlpRecordable *recordable = new OtlpRecordable();
  recordable->SetResource(resource);
  recordable->SetInstrumentationScope(instrumentation_scope);

  recordable->SetName("otlp_file_client_test_span");
  recordable->SetSpanKind(opentelemetry::trace::SpanKind::kInternal);
  recordable->SetAttribute("test-attribute-key", "test-attribute-value");
  recordable->SetDuration(std::chrono::nanoseconds(1234567890));
  recordable->SetStartTime(
      opentelemetry::common::SystemTimestamp(std::chrono::system_clock::now()));
  recordable->SetStatus(opentelemetry::trace::StatusCode::kOk, "success");

  {
    constexpr uint8_t trace_id_buf[]       = {1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8};
    constexpr uint8_t span_id_buf[]        = {1, 2, 3, 4, 5, 6, 7, 8};
    constexpr uint8_t parent_span_id_buf[] = {8, 7, 6, 5, 4, 3, 2, 1};
    opentelemetry::trace::TraceId trace_id{trace_id_buf};
    opentelemetry::trace::SpanId span_id{span_id_buf};
    opentelemetry::trace::SpanId parent_span_id{parent_span_id_buf};
    const auto trace_state = opentelemetry::trace::TraceState::GetDefault()->Set("key1", "value");
    const opentelemetry::trace::SpanContext span_context{
        trace_id, span_id,
        opentelemetry::trace::TraceFlags{opentelemetry::trace::TraceFlags::kIsSampled}, true,
        trace_state};

    recordable->SetIdentity(span_context, parent_span_id);
  }

  return std::unique_ptr<opentelemetry::sdk::trace::Recordable>(recordable);
}

TEST(OtlpFileClientTest, Shutdown)
{
  opentelemetry::proto::collector::trace::v1::ExportTraceServiceRequest request;
  auto client = std::unique_ptr<opentelemetry::exporter::otlp::OtlpFileClient>(
      new opentelemetry::exporter::otlp::OtlpFileClient(
          opentelemetry::exporter::otlp::OtlpFileClientOptions(),
          opentelemetry::exporter::otlp::OtlpFileClientRuntimeOptions()));
  ASSERT_FALSE(client->IsShutdown());
  ASSERT_TRUE(client->Shutdown());
  ASSERT_TRUE(client->IsShutdown());

  auto result = client->Export(request, 1);
  EXPECT_EQ(result, opentelemetry::sdk::common::ExportResult::kFailure);
}

TEST(OtlpFileClientTest, ExportToOstreamTest)
{
  auto resource              = MakeResource();
  auto instrumentation_scope = MakeInstrumentationScope();

  std::unique_ptr<opentelemetry::sdk::trace::Recordable> recordable[] = {
      MakeRecordable(resource, *instrumentation_scope)};

  opentelemetry::proto::collector::trace::v1::ExportTraceServiceRequest request;
  OtlpRecordableUtils::PopulateRequest(MakeSpan(recordable), &request);

  std::stringstream output_stream;

  opentelemetry::exporter::otlp::OtlpFileClientOptions opts;
  opentelemetry::exporter::otlp::OtlpFileClientRuntimeOptions rt_opts;
  opts.backend_options = std::ref(output_stream);

  auto client = std::unique_ptr<opentelemetry::exporter::otlp::OtlpFileClient>(
      new opentelemetry::exporter::otlp::OtlpFileClient(std::move(opts), std::move(rt_opts)));
  client->Export(request, 1);

  {
    auto check_json        = nlohmann::json::parse(output_stream.str(), nullptr, false);
    auto resource_span     = *check_json["resourceSpans"].begin();
    auto scope_span        = *resource_span["scopeSpans"].begin();
    auto span              = *scope_span["spans"].begin();
    auto received_trace_id = span["traceId"].get<std::string>();
    EXPECT_EQ(received_trace_id, "01020304050607080102030405060708");

    auto received_name = span["name"].get<std::string>();
    EXPECT_EQ(received_name, request.resource_spans(0).scope_spans(0).spans(0).name());

    auto receivec_attributes = span["attributes"];
    int attribute_found      = 0;
    for (auto iter = receivec_attributes.begin(); iter != receivec_attributes.end(); ++iter)
    {
      auto key = (*iter)["key"].get<std::string>();
      if (key == "test-attribute-key")
      {
        auto value = (*iter)["value"]["stringValue"];
        ++attribute_found;
        EXPECT_EQ(value.get<std::string>(), "test-attribute-value");
      }
    }
    EXPECT_EQ(attribute_found, 1);

    auto receivec_resource_attributes = resource_span["resource"]["attributes"];
    for (auto iter = receivec_resource_attributes.begin();
         iter != receivec_resource_attributes.end(); ++iter)
    {
      auto key = (*iter)["key"].get<std::string>();
      if (key == "service.name")
      {
        auto value = (*iter)["value"]["stringValue"];
        ++attribute_found;
        EXPECT_EQ(value.get<std::string>(), "unit_test_service");
      }
      else if (key == "tenant.id")
      {
        auto value = (*iter)["value"]["stringValue"];
        ++attribute_found;
        EXPECT_EQ(value.get<std::string>(), "test_user");
      }
      else if (key == "int32_value")
      {
        auto value = (*iter)["value"]["intValue"];
        ++attribute_found;
        if (value.is_number())
        {
          EXPECT_EQ(value.get<int32_t>(), 1);
        }
        else
        {
          EXPECT_EQ(value.get<std::string>(), "1");
        }
      }
    }
    EXPECT_EQ(attribute_found, 4);
  }
}

TEST(OtlpFileClientTest, ExportToFileSystemRotateIndexTest)
{
  auto resource              = MakeResource();
  auto instrumentation_scope = MakeInstrumentationScope();

  std::unique_ptr<opentelemetry::sdk::trace::Recordable> recordable[] = {
      MakeRecordable(resource, *instrumentation_scope)};

  opentelemetry::proto::collector::trace::v1::ExportTraceServiceRequest request;
  OtlpRecordableUtils::PopulateRequest(MakeSpan(recordable), &request);

  std::stringstream output_stream;

  // Clear old files
  {
    std::fstream clear_file1("otlp_file_client_test_dir/trace-1.jsonl",
                             std::ios::out | std::ios::trunc);
    std::fstream clear_file2("otlp_file_client_test_dir/trace-2.jsonl",
                             std::ios::out | std::ios::trunc);
    std::fstream clear_file3("otlp_file_client_test_dir/trace-3.jsonl",
                             std::ios::out | std::ios::trunc);
    std::fstream clear_file4("otlp_file_client_test_dir/trace-latest.jsonl",
                             std::ios::out | std::ios::trunc);
  }

  opentelemetry::exporter::otlp::OtlpFileClientFileSystemOptions backend_opts;
  backend_opts.file_pattern  = "otlp_file_client_test_dir/trace-%n.jsonl";
  backend_opts.alias_pattern = "otlp_file_client_test_dir/trace-latest.jsonl";
  // Smaller than the size of one record, so it will rotate after each record.
  backend_opts.file_size   = 1500;
  backend_opts.rotate_size = 3;

  opentelemetry::exporter::otlp::OtlpFileClientOptions opts;
  opentelemetry::exporter::otlp::OtlpFileClientRuntimeOptions rt_opts;
  opts.backend_options = backend_opts;

  auto client = std::unique_ptr<opentelemetry::exporter::otlp::OtlpFileClient>(
      new opentelemetry::exporter::otlp::OtlpFileClient(std::move(opts), std::move(rt_opts)));

  // Write 5 records with rotatation index 1,2,3,1,2
  for (int i = 0; i < 4; ++i)
  {
    client->Export(request, 1);
  }
  request.mutable_resource_spans(0)->set_schema_url("https://opentelemetry.io/schemas/1.12.0");
  client->Export(request, 1);
  client->ForceFlush();

  std::unique_ptr<std::ifstream> input_file[5] = {
      std::unique_ptr<std::ifstream>(
          new std::ifstream("otlp_file_client_test_dir/trace-1.jsonl", std::ios::in)),
      std::unique_ptr<std::ifstream>(
          new std::ifstream("otlp_file_client_test_dir/trace-2.jsonl", std::ios::in)),
      std::unique_ptr<std::ifstream>(
          new std::ifstream("otlp_file_client_test_dir/trace-3.jsonl", std::ios::in)),
      std::unique_ptr<std::ifstream>(
          new std::ifstream("otlp_file_client_test_dir/trace-4.jsonl", std::ios::in)),
      std::unique_ptr<std::ifstream>(
          new std::ifstream("otlp_file_client_test_dir/trace-latest.jsonl", std::ios::in))};

  EXPECT_TRUE(input_file[0]->is_open());
  EXPECT_TRUE(input_file[1]->is_open());
  EXPECT_TRUE(input_file[2]->is_open());
  EXPECT_FALSE(input_file[3]->is_open());
  EXPECT_TRUE(input_file[4]->is_open());

  std::string jsonl[4];
  std::getline(*input_file[0], jsonl[0]);
  std::getline(*input_file[1], jsonl[1]);
  std::getline(*input_file[2], jsonl[2]);
  std::getline(*input_file[4], jsonl[3]);

  EXPECT_EQ(jsonl[0], jsonl[2]);
  EXPECT_EQ(jsonl[1], jsonl[3]);

  {
    auto check_json        = nlohmann::json::parse(jsonl[0], nullptr, false);
    auto resource_span     = *check_json["resourceSpans"].begin();
    auto scope_span        = *resource_span["scopeSpans"].begin();
    auto span              = *scope_span["spans"].begin();
    auto received_trace_id = span["traceId"].get<std::string>();
    EXPECT_EQ(received_trace_id, "01020304050607080102030405060708");

    auto received_name = span["name"].get<std::string>();
    EXPECT_EQ(received_name, request.resource_spans(0).scope_spans(0).spans(0).name());

    auto receivec_attributes = span["attributes"];
    int attribute_found      = 0;
    for (auto iter = receivec_attributes.begin(); iter != receivec_attributes.end(); ++iter)
    {
      auto key = (*iter)["key"].get<std::string>();
      if (key == "test-attribute-key")
      {
        auto value = (*iter)["value"]["stringValue"];
        ++attribute_found;
        EXPECT_EQ(value.get<std::string>(), "test-attribute-value");
      }
    }
    EXPECT_EQ(attribute_found, 1);

    auto receivec_resource_attributes = resource_span["resource"]["attributes"];
    for (auto iter = receivec_resource_attributes.begin();
         iter != receivec_resource_attributes.end(); ++iter)
    {
      auto key = (*iter)["key"].get<std::string>();
      if (key == "service.name")
      {
        auto value = (*iter)["value"]["stringValue"];
        ++attribute_found;
        EXPECT_EQ(value.get<std::string>(), "unit_test_service");
      }
      else if (key == "tenant.id")
      {
        auto value = (*iter)["value"]["stringValue"];
        ++attribute_found;
        EXPECT_EQ(value.get<std::string>(), "test_user");
      }
      else if (key == "int32_value")
      {
        auto value = (*iter)["value"]["intValue"];
        ++attribute_found;
        if (value.is_number())
        {
          EXPECT_EQ(value.get<int32_t>(), 1);
        }
        else
        {
          EXPECT_EQ(value.get<std::string>(), "1");
        }
      }
    }
    EXPECT_EQ(attribute_found, 4);
  }
}

TEST(OtlpFileClientTest, ExportToFileSystemRotateByTimeTest)
{
  auto resource              = MakeResource();
  auto instrumentation_scope = MakeInstrumentationScope();

  std::unique_ptr<opentelemetry::sdk::trace::Recordable> recordable[] = {
      MakeRecordable(resource, *instrumentation_scope)};

  opentelemetry::proto::collector::trace::v1::ExportTraceServiceRequest request;
  OtlpRecordableUtils::PopulateRequest(MakeSpan(recordable), &request);

  std::stringstream output_stream;

  opentelemetry::exporter::otlp::OtlpFileClientFileSystemOptions backend_opts;
  backend_opts.file_pattern  = "otlp_file_client_test_dir/trace-%Y-%m-%d-%H-%M-%S.jsonl";
  backend_opts.alias_pattern = "";
  // Smaller than the size of one record, so it will rotate after each record.
  backend_opts.file_size = 1500;

  opentelemetry::exporter::otlp::OtlpFileClientOptions opts;
  opentelemetry::exporter::otlp::OtlpFileClientRuntimeOptions rt_opts;
  opts.backend_options = backend_opts;

  auto client = std::unique_ptr<opentelemetry::exporter::otlp::OtlpFileClient>(
      new opentelemetry::exporter::otlp::OtlpFileClient(std::move(opts), std::move(rt_opts)));

  auto start_time = std::chrono::system_clock::now();
  client->Export(request, 1);
  std::this_thread::sleep_for(std::chrono::seconds{1});
  client->Export(request, 1);
  client->ForceFlush();

  std::unique_ptr<std::ifstream> input_file[2];
  std::size_t found_file_index = 0;
  // Try to load the file in 5s, it should finished.
  for (int i = 0; i < 5; ++i)
  {
    char file_path_buf[256] = {0};
    std::tm local_tm        = GetLocalTime(start_time);
    std::strftime(file_path_buf, sizeof(file_path_buf) - 1,
                  "otlp_file_client_test_dir/trace-%Y-%m-%d-%H-%M-%S.jsonl", &local_tm);
    start_time += std::chrono::seconds{1};

    input_file[found_file_index] =
        std::unique_ptr<std::ifstream>(new std::ifstream(file_path_buf, std::ios::in));
    if (input_file[found_file_index]->is_open())
    {
      ++found_file_index;
    }
    if (found_file_index >= 2)
    {
      break;
    }
  }

  ASSERT_EQ(found_file_index, 2);

  std::string jsonl[2];
  std::getline(*input_file[0], jsonl[0]);
  std::getline(*input_file[1], jsonl[1]);

  EXPECT_EQ(jsonl[0], jsonl[1]);

  {
    auto check_json        = nlohmann::json::parse(jsonl[0], nullptr, false);
    auto resource_span     = *check_json["resourceSpans"].begin();
    auto scope_span        = *resource_span["scopeSpans"].begin();
    auto span              = *scope_span["spans"].begin();
    auto received_trace_id = span["traceId"].get<std::string>();
    EXPECT_EQ(received_trace_id, "01020304050607080102030405060708");

    auto received_name = span["name"].get<std::string>();
    EXPECT_EQ(received_name, request.resource_spans(0).scope_spans(0).spans(0).name());

    auto receivec_attributes = span["attributes"];
    int attribute_found      = 0;
    for (auto iter = receivec_attributes.begin(); iter != receivec_attributes.end(); ++iter)
    {
      auto key = (*iter)["key"].get<std::string>();
      if (key == "test-attribute-key")
      {
        auto value = (*iter)["value"]["stringValue"];
        ++attribute_found;
        EXPECT_EQ(value.get<std::string>(), "test-attribute-value");
      }
    }
    EXPECT_EQ(attribute_found, 1);

    auto receivec_resource_attributes = resource_span["resource"]["attributes"];
    for (auto iter = receivec_resource_attributes.begin();
         iter != receivec_resource_attributes.end(); ++iter)
    {
      auto key = (*iter)["key"].get<std::string>();
      if (key == "service.name")
      {
        auto value = (*iter)["value"]["stringValue"];
        ++attribute_found;
        EXPECT_EQ(value.get<std::string>(), "unit_test_service");
      }
      else if (key == "tenant.id")
      {
        auto value = (*iter)["value"]["stringValue"];
        ++attribute_found;
        EXPECT_EQ(value.get<std::string>(), "test_user");
      }
      else if (key == "int32_value")
      {
        auto value = (*iter)["value"]["intValue"];
        ++attribute_found;
        if (value.is_number())
        {
          EXPECT_EQ(value.get<int32_t>(), 1);
        }
        else
        {
          EXPECT_EQ(value.get<std::string>(), "1");
        }
      }
    }
    EXPECT_EQ(attribute_found, 4);
  }
}

// Test client configuration options
TEST(OtlpFileClientTest, ConfigTest)
{
  {
    opentelemetry::exporter::otlp::OtlpFileClientOptions opts;
    opentelemetry::exporter::otlp::OtlpFileClientRuntimeOptions rt_opts;
    opts.console_debug   = true;
    opts.backend_options = std::ref(std::cout);

    auto client = std::unique_ptr<opentelemetry::exporter::otlp::OtlpFileClient>(
        new opentelemetry::exporter::otlp::OtlpFileClient(std::move(opts), std::move(rt_opts)));

    ASSERT_TRUE(client->GetOptions().console_debug);
    ASSERT_TRUE(opentelemetry::nostd::holds_alternative<std::reference_wrapper<std::ostream>>(
        client->GetOptions().backend_options));
  }

  {
    opentelemetry::exporter::otlp::OtlpFileClientFileSystemOptions backend_opts;
    backend_opts.file_pattern = "test_file_pattern.jsonl";

    opentelemetry::exporter::otlp::OtlpFileClientOptions opts;
    opentelemetry::exporter::otlp::OtlpFileClientRuntimeOptions rt_opts;
    opts.console_debug   = false;
    opts.backend_options = backend_opts;

    auto client = std::unique_ptr<opentelemetry::exporter::otlp::OtlpFileClient>(
        new opentelemetry::exporter::otlp::OtlpFileClient(std::move(opts), std::move(rt_opts)));

    ASSERT_FALSE(client->GetOptions().console_debug);
    ASSERT_TRUE(opentelemetry::nostd::holds_alternative<
                opentelemetry::exporter::otlp::OtlpFileClientFileSystemOptions>(
        client->GetOptions().backend_options));

    EXPECT_EQ(
        opentelemetry::nostd::get<opentelemetry::exporter::otlp::OtlpFileClientFileSystemOptions>(
            client->GetOptions().backend_options)
            .file_pattern,
        "test_file_pattern.jsonl");
  }
}

}  // namespace otlp
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
