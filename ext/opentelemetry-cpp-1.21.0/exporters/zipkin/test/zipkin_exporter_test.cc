// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#ifndef OPENTELEMETRY_STL_VERSION

#  include "opentelemetry/exporters/zipkin/zipkin_exporter.h"
#  include "opentelemetry/ext/http/client/curl/http_client_curl.h"
#  include "opentelemetry/ext/http/server/http_server.h"
#  include "opentelemetry/sdk/trace/batch_span_processor.h"
#  include "opentelemetry/sdk/trace/batch_span_processor_options.h"
#  include "opentelemetry/sdk/trace/tracer_provider.h"
#  include "opentelemetry/trace/provider.h"

#  include <gtest/gtest.h>
#  include "gmock/gmock.h"

#  include "nlohmann/json.hpp"

#  include <string>
#  include <utility>

#  if defined(_MSC_VER)
#    include "opentelemetry/sdk/common/env_variables.h"
using opentelemetry::sdk::common::setenv;
using opentelemetry::sdk::common::unsetenv;
#  endif
namespace sdk_common = opentelemetry::sdk::common;
using namespace testing;

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace zipkin
{

namespace trace_api = opentelemetry::trace;
namespace resource  = opentelemetry::sdk::resource;

template <class T, size_t N>
static nostd::span<T, N> MakeSpan(T (&array)[N])
{
  return nostd::span<T, N>(array);
}

class ZipkinExporterTestPeer : public ::testing::Test
{
public:
  std::unique_ptr<sdk::trace::SpanExporter> GetExporter(
      std::shared_ptr<opentelemetry::ext::http::client::HttpClientSync> http_client)
  {
    return std::unique_ptr<sdk::trace::SpanExporter>(new ZipkinExporter(std::move(http_client)));
  }

  // Get the options associated with the given exporter.
  const ZipkinExporterOptions &GetOptions(std::unique_ptr<ZipkinExporter> &exporter)
  {
    return exporter->options_;
  }
};

class MockHttpClient : public opentelemetry::ext::http::client::HttpClientSync
{
public:
  MOCK_METHOD(ext::http::client::Result,
              Post,
              (const nostd::string_view &,
               const ext::http::client::HttpSslOptions &,
               const ext::http::client::Body &,
               const ext::http::client::Headers &,
               const ext::http::client::Compression &),
              (noexcept, override));

  MOCK_METHOD(ext::http::client::Result,
              Get,
              (const nostd::string_view &,
               const ext::http::client::HttpSslOptions &,
               const ext::http::client::Headers &,
               const ext::http::client::Compression &),
              (noexcept, override));
};

class IsValidMessageMatcher
{
public:
  IsValidMessageMatcher(const std::string &trace_id) : trace_id_(trace_id) {}
  template <typename T>
  bool MatchAndExplain(const T &p, MatchResultListener * /* listener */) const
  {
    auto body                 = std::string(p.begin(), p.end());
    nlohmann::json check_json = nlohmann::json::parse(body);
    auto trace_id_kv          = check_json.at(0).find("traceId");
    auto received_trace_id    = trace_id_kv.value().get<std::string>();
    return trace_id_ == received_trace_id;
  }

  void DescribeTo(std::ostream *os) const { *os << "received trace_id matches"; }

  void DescribeNegationTo(std::ostream *os) const { *os << "received trace_id does not matche"; }

private:
  std::string trace_id_;
};

PolymorphicMatcher<IsValidMessageMatcher> IsValidMessage(const std::string &trace_id)
{
  return MakePolymorphicMatcher(IsValidMessageMatcher(trace_id));
}

// Create spans, let processor call Export()
TEST_F(ZipkinExporterTestPeer, ExportJsonIntegrationTest)
{
  auto mock_http_client = new MockHttpClient;
  // Leave a comment line here or different version of clang-format has a different result here
  auto exporter = GetExporter(
      std::shared_ptr<opentelemetry::ext::http::client::HttpClientSync>{mock_http_client});

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
  auto resource = resource::Resource::Create(resource_attributes);

  auto processor_opts                  = sdk::trace::BatchSpanProcessorOptions();
  processor_opts.max_export_batch_size = 5;
  processor_opts.max_queue_size        = 5;
  processor_opts.schedule_delay_millis = std::chrono::milliseconds(256);
  auto processor                       = std::unique_ptr<sdk::trace::SpanProcessor>(
      new sdk::trace::BatchSpanProcessor(std::move(exporter), processor_opts));
  auto provider = nostd::shared_ptr<trace::TracerProvider>(
      new sdk::trace::TracerProvider(std::move(processor), resource));

  std::string report_trace_id;
  char trace_id_hex[2 * trace_api::TraceId::kSize] = {0};
  auto tracer                                      = provider->GetTracer("test");
  auto parent_span                                 = tracer->StartSpan("Test parent span");

  trace_api::StartSpanOptions child_span_opts = {};
  child_span_opts.parent                      = parent_span->GetContext();
  auto child_span = tracer->StartSpan("Test child span", child_span_opts);

  nostd::get<trace_api::SpanContext>(child_span_opts.parent)
      .trace_id()
      .ToLowerBase16(MakeSpan(trace_id_hex));
  report_trace_id.assign(trace_id_hex, sizeof(trace_id_hex));

  auto expected_url = nostd::string_view{"http://localhost:9411/api/v2/spans"};

  EXPECT_CALL(*mock_http_client, Post(expected_url, _, IsValidMessage(report_trace_id), _, _))

      .Times(Exactly(1))
      .WillOnce(Return(ByMove(ext::http::client::Result{
          std::unique_ptr<ext::http::client::Response>{new ext::http::client::curl::Response()},
          ext::http::client::SessionState::Response})));

  child_span->End();
  parent_span->End();
}

// Create spans, let processor call Export()
TEST_F(ZipkinExporterTestPeer, ShutdownTest)
{
  auto mock_http_client = new MockHttpClient;
  // Leave a comment line here or different version of clang-format has a different result here
  auto exporter = GetExporter(
      std::shared_ptr<opentelemetry::ext::http::client::HttpClientSync>{mock_http_client});
  auto recordable_1 = exporter->MakeRecordable();
  recordable_1->SetName("Test span 1");
  auto recordable_2 = exporter->MakeRecordable();
  recordable_2->SetName("Test span 2");

  // exporter should not be shutdown by default
  nostd::span<std::unique_ptr<sdk::trace::Recordable>> batch_1(&recordable_1, 1);

  EXPECT_CALL(*mock_http_client, Post(_, _, _, _, _))

      .Times(Exactly(1))
      .WillOnce(Return(ByMove(ext::http::client::Result{
          std::unique_ptr<ext::http::client::Response>{new ext::http::client::curl::Response()},
          ext::http::client::SessionState::Response})));
  auto result = exporter->Export(batch_1);
  EXPECT_EQ(sdk_common::ExportResult::kSuccess, result);

  exporter->Shutdown();

  nostd::span<std::unique_ptr<sdk::trace::Recordable>> batch_2(&recordable_2, 1);
  result = exporter->Export(batch_2);
  EXPECT_EQ(sdk_common::ExportResult::kFailure, result);
}

// Test exporter configuration options
TEST_F(ZipkinExporterTestPeer, ConfigTest)
{
  ZipkinExporterOptions opts;
  opts.endpoint = "http://localhost:45455/v1/traces";
  std::unique_ptr<ZipkinExporter> exporter(new ZipkinExporter(opts));
  EXPECT_EQ(GetOptions(exporter).endpoint, "http://localhost:45455/v1/traces");
}

#  ifndef NO_GETENV
// Test exporter configuration options from env
TEST_F(ZipkinExporterTestPeer, ConfigFromEnv)
{
  const std::string endpoint = "http://localhost:9999/v1/traces";
  setenv("OTEL_EXPORTER_ZIPKIN_ENDPOINT", endpoint.c_str(), 1);

  std::unique_ptr<ZipkinExporter> exporter(new ZipkinExporter());
  EXPECT_EQ(GetOptions(exporter).endpoint, endpoint);

  unsetenv("OTEL_EXPORTER_ZIPKIN_ENDPOINT");
}

#  endif  // NO_GETENV

}  // namespace zipkin
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
#endif /* OPENTELEMETRY_STL_VERSION */
