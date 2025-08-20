// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <utility>
#include <vector>
#include "gmock/gmock.h"

#include "opentelemetry/common/timestamp.h"
#include "opentelemetry/exporters/otlp/otlp_environment.h"
#include "opentelemetry/exporters/otlp/otlp_http.h"
#include "opentelemetry/exporters/otlp/otlp_http_client.h"
#include "opentelemetry/exporters/otlp/otlp_http_metric_exporter.h"
#include "opentelemetry/exporters/otlp/otlp_http_metric_exporter_options.h"
#include "opentelemetry/exporters/otlp/otlp_metric_utils.h"
#include "opentelemetry/exporters/otlp/otlp_preferred_temporality.h"
#include "opentelemetry/ext/http/client/http_client.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/nostd/unique_ptr.h"
#include "opentelemetry/sdk/common/exporter_utils.h"
#include "opentelemetry/sdk/common/thread_instrumentation.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/metrics/data/metric_data.h"
#include "opentelemetry/sdk/metrics/data/point_data.h"
#include "opentelemetry/sdk/metrics/export/metric_producer.h"
#include "opentelemetry/sdk/metrics/instruments.h"
#include "opentelemetry/sdk/metrics/push_metric_exporter.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/test_common/ext/http/client/http_client_test_factory.h"
#include "opentelemetry/test_common/ext/http/client/nosend/http_client_nosend.h"
#include "opentelemetry/version.h"

// clang-format off
#include "opentelemetry/exporters/otlp/protobuf_include_prefix.h" // IWYU pragma: keep
// IWYU pragma: no_include "net/proto2/public/repeated_field.h"
#include <google/protobuf/message_lite.h>
#include "opentelemetry/proto/collector/metrics/v1/metrics_service.pb.h"
#include "opentelemetry/proto/common/v1/common.pb.h"
#include "opentelemetry/proto/metrics/v1/metrics.pb.h"
#include "opentelemetry/exporters/otlp/protobuf_include_suffix.h" // IWYU pragma: keep
// clang-format on

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

template <class IntegerType>
static IntegerType JsonToInteger(nlohmann::json value)
{
  if (value.is_string())
  {
    return static_cast<IntegerType>(strtol(value.get<std::string>().c_str(), nullptr, 10));
  }

  return value.get<IntegerType>();
}

OtlpHttpClientOptions MakeOtlpHttpClientOptions(HttpRequestContentType content_type,
                                                bool async_mode)
{
  std::shared_ptr<opentelemetry::sdk::common::ThreadInstrumentation> not_instrumented;
  OtlpHttpMetricExporterOptions options;
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

class OtlpHttpMetricExporterTestPeer : public ::testing::Test
{
public:
  std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> GetExporter(
      std::unique_ptr<OtlpHttpClient> http_client)
  {
    return std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>(
        new OtlpHttpMetricExporter(std::move(http_client)));
  }

  // Get the options associated with the given exporter.
  const OtlpHttpMetricExporterOptions &GetOptions(std::unique_ptr<OtlpHttpMetricExporter> &exporter)
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

  void ExportJsonIntegrationTestExportSumPointData(
#ifdef ENABLE_ASYNC_EXPORT
      bool async_mode
#endif
  )
  {
    auto mock_otlp_client =
        OtlpHttpMetricExporterTestPeer::GetMockOtlpHttpClient(HttpRequestContentType::kJson
#ifdef ENABLE_ASYNC_EXPORT
                                                              ,
                                                              async_mode
#endif
        );
    auto mock_otlp_http_client = mock_otlp_client.first;
    auto client                = mock_otlp_client.second;
    auto exporter = GetExporter(std::unique_ptr<OtlpHttpClient>{mock_otlp_http_client});

    opentelemetry::sdk::metrics::SumPointData sum_point_data{};
    sum_point_data.value_ = 10.0;
    opentelemetry::sdk::metrics::SumPointData sum_point_data2{};
    sum_point_data2.value_ = 20.0;
    opentelemetry::sdk::metrics::ResourceMetrics data;
    auto resource = opentelemetry::sdk::resource::Resource::Create(
        opentelemetry::sdk::resource::ResourceAttributes{});
    data.resource_ = &resource;
    auto scope     = opentelemetry::sdk::instrumentationscope::InstrumentationScope::Create(
        "library_name", "1.5.0");
    opentelemetry::sdk::metrics::MetricData metric_data{
        opentelemetry::sdk::metrics::InstrumentDescriptor{
            "metrics_library_name", "metrics_description", "metrics_unit",
            opentelemetry::sdk::metrics::InstrumentType::kCounter,
            opentelemetry::sdk::metrics::InstrumentValueType::kDouble},
        opentelemetry::sdk::metrics::AggregationTemporality::kDelta,
        opentelemetry::common::SystemTimestamp{}, opentelemetry::common::SystemTimestamp{},
        std::vector<opentelemetry::sdk::metrics::PointDataAttributes>{
            {opentelemetry::sdk::metrics::PointAttributes{{"a1", "b1"}}, sum_point_data},
            {opentelemetry::sdk::metrics::PointAttributes{{"a2", "b2"}}, sum_point_data2}}};
    data.scope_metric_data_ = std::vector<opentelemetry::sdk::metrics::ScopeMetrics>{
        {scope.get(), std::vector<opentelemetry::sdk::metrics::MetricData>{metric_data}}};

    auto no_send_client = std::static_pointer_cast<http_client::nosend::HttpClient>(client);
    auto mock_session =
        std::static_pointer_cast<http_client::nosend::Session>(no_send_client->session_);
    EXPECT_CALL(*mock_session, SendRequest)
        .WillOnce(
            [&mock_session](
                const std::shared_ptr<opentelemetry::ext::http::client::EventHandler> &callback) {
              auto check_json =
                  nlohmann::json::parse(mock_session->GetRequest()->body_, nullptr, false);

              auto resource_metrics = *check_json["resourceMetrics"].begin();
              auto scope_metrics    = *resource_metrics["scopeMetrics"].begin();
              auto scope            = scope_metrics["scope"];
              EXPECT_EQ("library_name", scope["name"].get<std::string>());
              EXPECT_EQ("1.5.0", scope["version"].get<std::string>());

              auto metric = *scope_metrics["metrics"].begin();
              EXPECT_EQ("metrics_library_name", metric["name"].get<std::string>());
              EXPECT_EQ("metrics_description", metric["description"].get<std::string>());
              EXPECT_EQ("metrics_unit", metric["unit"].get<std::string>());

              auto data_points = metric["sum"]["dataPoints"];
              EXPECT_EQ(10.0, data_points[0]["asDouble"].get<double>());
              EXPECT_EQ(20.0, data_points[1]["asDouble"].get<double>());

              auto custom_header = mock_session->GetRequest()->headers_.find("Custom-Header-Key");
              ASSERT_TRUE(custom_header != mock_session->GetRequest()->headers_.end());
              if (custom_header != mock_session->GetRequest()->headers_.end())
              {
                EXPECT_EQ("Custom-Header-Value", custom_header->second);
              }

              http_client::nosend::Response response;
              response.Finish(*callback.get());
            });

    auto result = exporter->Export(data);
    EXPECT_EQ(result, opentelemetry::sdk::common::ExportResult::kSuccess);

    exporter->ForceFlush();
  }

  void ExportBinaryIntegrationTestExportSumPointData(
#ifdef ENABLE_ASYNC_EXPORT
      bool async_mode
#endif
  )
  {
    auto mock_otlp_client =
        OtlpHttpMetricExporterTestPeer::GetMockOtlpHttpClient(HttpRequestContentType::kBinary
#ifdef ENABLE_ASYNC_EXPORT
                                                              ,
                                                              async_mode
#endif
        );
    auto mock_otlp_http_client = mock_otlp_client.first;
    auto client                = mock_otlp_client.second;
    auto exporter = GetExporter(std::unique_ptr<OtlpHttpClient>{mock_otlp_http_client});

    opentelemetry::sdk::metrics::SumPointData sum_point_data{};
    sum_point_data.value_ = 10.0;
    opentelemetry::sdk::metrics::SumPointData sum_point_data2{};
    sum_point_data2.value_ = 20.0;
    opentelemetry::sdk::metrics::ResourceMetrics data;
    auto resource = opentelemetry::sdk::resource::Resource::Create(
        opentelemetry::sdk::resource::ResourceAttributes{});
    data.resource_ = &resource;
    auto scope     = opentelemetry::sdk::instrumentationscope::InstrumentationScope::Create(
        "library_name", "1.5.0");
    opentelemetry::sdk::metrics::MetricData metric_data{
        opentelemetry::sdk::metrics::InstrumentDescriptor{
            "metrics_library_name", "metrics_description", "metrics_unit",
            opentelemetry::sdk::metrics::InstrumentType::kCounter,
            opentelemetry::sdk::metrics::InstrumentValueType::kDouble},
        opentelemetry::sdk::metrics::AggregationTemporality::kDelta,
        opentelemetry::common::SystemTimestamp{}, opentelemetry::common::SystemTimestamp{},
        std::vector<opentelemetry::sdk::metrics::PointDataAttributes>{
            {opentelemetry::sdk::metrics::PointAttributes{{"a1", "b1"}}, sum_point_data},
            {opentelemetry::sdk::metrics::PointAttributes{{"a2", "b2"}}, sum_point_data2}}};
    data.scope_metric_data_ = std::vector<opentelemetry::sdk::metrics::ScopeMetrics>{
        {scope.get(), std::vector<opentelemetry::sdk::metrics::MetricData>{metric_data}}};

    auto no_send_client = std::static_pointer_cast<http_client::nosend::HttpClient>(client);
    auto mock_session =
        std::static_pointer_cast<http_client::nosend::Session>(no_send_client->session_);

    EXPECT_CALL(*mock_session, SendRequest)
        .WillOnce([&mock_session](
                      const std::shared_ptr<opentelemetry::ext::http::client::EventHandler>
                          &callback) {
          opentelemetry::proto::collector::metrics::v1::ExportMetricsServiceRequest request_body;
          request_body.ParseFromArray(&mock_session->GetRequest()->body_[0],
                                      static_cast<int>(mock_session->GetRequest()->body_.size()));
          auto &scope_metrics = request_body.resource_metrics(0).scope_metrics(0);
          auto &scope         = scope_metrics.scope();
          EXPECT_EQ("library_name", scope.name());
          EXPECT_EQ("1.5.0", scope.version());

          auto &metric = scope_metrics.metrics(0);
          EXPECT_EQ("metrics_library_name", metric.name());
          EXPECT_EQ("metrics_description", metric.description());
          EXPECT_EQ("metrics_unit", metric.unit());

          auto &data_points = metric.sum().data_points();
          EXPECT_EQ(10.0, data_points.Get(0).as_double());
          bool has_attributes = false;
          for (auto &kv : data_points.Get(0).attributes())
          {
            if (kv.key() == "a1")
            {
              EXPECT_EQ("b1", kv.value().string_value());
              has_attributes = true;
            }
          }
          EXPECT_TRUE(has_attributes);

          EXPECT_EQ(20.0, data_points.Get(1).as_double());
          has_attributes = false;
          for (auto &kv : data_points.Get(1).attributes())
          {
            if (kv.key() == "a2")
            {
              EXPECT_EQ("b2", kv.value().string_value());
              has_attributes = true;
            }
          }
          EXPECT_TRUE(has_attributes);

          http_client::nosend::Response response;
          response.Finish(*callback.get());
        });

    auto result = exporter->Export(data);
    EXPECT_EQ(result, opentelemetry::sdk::common::ExportResult::kSuccess);

    exporter->ForceFlush();
  }

  void ExportJsonIntegrationTestExportLastValuePointData(
#ifdef ENABLE_ASYNC_EXPORT
      bool async_mode
#endif
  )
  {
    auto mock_otlp_client =
        OtlpHttpMetricExporterTestPeer::GetMockOtlpHttpClient(HttpRequestContentType::kJson
#ifdef ENABLE_ASYNC_EXPORT
                                                              ,
                                                              async_mode
#endif
        );
    auto mock_otlp_http_client = mock_otlp_client.first;
    auto client                = mock_otlp_client.second;
    auto exporter = GetExporter(std::unique_ptr<OtlpHttpClient>{mock_otlp_http_client});

    opentelemetry::sdk::metrics::LastValuePointData last_value_point_data{};
    last_value_point_data.value_              = 10.0;
    last_value_point_data.is_lastvalue_valid_ = true;
    last_value_point_data.sample_ts_          = opentelemetry::common::SystemTimestamp{};
    opentelemetry::sdk::metrics::LastValuePointData last_value_point_data2{};
    last_value_point_data2.value_              = static_cast<int64_t>(20);
    last_value_point_data2.is_lastvalue_valid_ = true;
    last_value_point_data2.sample_ts_          = opentelemetry::common::SystemTimestamp{};
    opentelemetry::sdk::metrics::MetricData metric_data{
        opentelemetry::sdk::metrics::InstrumentDescriptor{
            "metrics_library_name", "metrics_description", "metrics_unit",
            opentelemetry::sdk::metrics::InstrumentType::kObservableGauge,
            opentelemetry::sdk::metrics::InstrumentValueType::kDouble},
        opentelemetry::sdk::metrics::AggregationTemporality::kDelta,
        opentelemetry::common::SystemTimestamp{}, opentelemetry::common::SystemTimestamp{},
        std::vector<opentelemetry::sdk::metrics::PointDataAttributes>{
            {opentelemetry::sdk::metrics::PointAttributes{{"a1", "b1"}}, last_value_point_data},
            {opentelemetry::sdk::metrics::PointAttributes{{"a2", "b2"}}, last_value_point_data2}}};

    opentelemetry::sdk::metrics::ResourceMetrics data;
    auto resource = opentelemetry::sdk::resource::Resource::Create(
        opentelemetry::sdk::resource::ResourceAttributes{});
    data.resource_ = &resource;
    auto scope     = opentelemetry::sdk::instrumentationscope::InstrumentationScope::Create(
        "library_name", "1.5.0");
    data.scope_metric_data_ = std::vector<opentelemetry::sdk::metrics::ScopeMetrics>{
        {scope.get(), std::vector<opentelemetry::sdk::metrics::MetricData>{metric_data}}};

    auto no_send_client = std::static_pointer_cast<http_client::nosend::HttpClient>(client);
    auto mock_session =
        std::static_pointer_cast<http_client::nosend::Session>(no_send_client->session_);
    EXPECT_CALL(*mock_session, SendRequest)
        .WillOnce(
            [&mock_session](
                const std::shared_ptr<opentelemetry::ext::http::client::EventHandler> &callback) {
              auto check_json =
                  nlohmann::json::parse(mock_session->GetRequest()->body_, nullptr, false);

              auto resource_metrics = *check_json["resourceMetrics"].begin();
              auto scope_metrics    = *resource_metrics["scopeMetrics"].begin();
              auto scope            = scope_metrics["scope"];
              EXPECT_EQ("library_name", scope["name"].get<std::string>());
              EXPECT_EQ("1.5.0", scope["version"].get<std::string>());

              auto metric = *scope_metrics["metrics"].begin();
              EXPECT_EQ("metrics_library_name", metric["name"].get<std::string>());
              EXPECT_EQ("metrics_description", metric["description"].get<std::string>());
              EXPECT_EQ("metrics_unit", metric["unit"].get<std::string>());

              auto data_points = metric["gauge"]["dataPoints"];
              EXPECT_EQ(10.0, data_points[0]["asDouble"].get<double>());
              EXPECT_EQ(20l, JsonToInteger<int64_t>(data_points[1]["asInt"]));

              auto custom_header = mock_session->GetRequest()->headers_.find("Custom-Header-Key");
              ASSERT_TRUE(custom_header != mock_session->GetRequest()->headers_.end());
              if (custom_header != mock_session->GetRequest()->headers_.end())
              {
                EXPECT_EQ("Custom-Header-Value", custom_header->second);
              }

              http_client::nosend::Response response;
              response.Finish(*callback.get());
            });

    auto result = exporter->Export(data);
    EXPECT_EQ(result, opentelemetry::sdk::common::ExportResult::kSuccess);

    exporter->ForceFlush();
  }

  void ExportBinaryIntegrationTestExportLastValuePointData(
#ifdef ENABLE_ASYNC_EXPORT
      bool async_mode
#endif
  )
  {
    auto mock_otlp_client =
        OtlpHttpMetricExporterTestPeer::GetMockOtlpHttpClient(HttpRequestContentType::kBinary
#ifdef ENABLE_ASYNC_EXPORT
                                                              ,
                                                              async_mode
#endif
        );
    auto mock_otlp_http_client = mock_otlp_client.first;
    auto client                = mock_otlp_client.second;
    auto exporter = GetExporter(std::unique_ptr<OtlpHttpClient>{mock_otlp_http_client});

    opentelemetry::sdk::metrics::SumPointData sum_point_data{};
    sum_point_data.value_ = 10.0;
    opentelemetry::sdk::metrics::SumPointData sum_point_data2{};
    sum_point_data2.value_ = 20.0;
    opentelemetry::sdk::metrics::ResourceMetrics data;
    auto resource = opentelemetry::sdk::resource::Resource::Create(
        opentelemetry::sdk::resource::ResourceAttributes{});
    data.resource_ = &resource;
    auto scope     = opentelemetry::sdk::instrumentationscope::InstrumentationScope::Create(
        "library_name", "1.5.0");
    opentelemetry::sdk::metrics::LastValuePointData last_value_point_data{};
    last_value_point_data.value_              = 10.0;
    last_value_point_data.is_lastvalue_valid_ = true;
    last_value_point_data.sample_ts_          = opentelemetry::common::SystemTimestamp{};
    opentelemetry::sdk::metrics::LastValuePointData last_value_point_data2{};
    last_value_point_data2.value_              = static_cast<int64_t>(20);
    last_value_point_data2.is_lastvalue_valid_ = true;
    last_value_point_data2.sample_ts_          = opentelemetry::common::SystemTimestamp{};
    opentelemetry::sdk::metrics::MetricData metric_data{
        opentelemetry::sdk::metrics::InstrumentDescriptor{
            "metrics_library_name", "metrics_description", "metrics_unit",
            opentelemetry::sdk::metrics::InstrumentType::kObservableGauge,
            opentelemetry::sdk::metrics::InstrumentValueType::kDouble},
        opentelemetry::sdk::metrics::AggregationTemporality::kDelta,
        opentelemetry::common::SystemTimestamp{}, opentelemetry::common::SystemTimestamp{},
        std::vector<opentelemetry::sdk::metrics::PointDataAttributes>{
            {opentelemetry::sdk::metrics::PointAttributes{{"a1", "b1"}}, last_value_point_data},
            {opentelemetry::sdk::metrics::PointAttributes{{"a2", "b2"}}, last_value_point_data2}}};

    data.scope_metric_data_ = std::vector<opentelemetry::sdk::metrics::ScopeMetrics>{
        {scope.get(), std::vector<opentelemetry::sdk::metrics::MetricData>{metric_data}}};

    auto no_send_client = std::static_pointer_cast<http_client::nosend::HttpClient>(client);
    auto mock_session =
        std::static_pointer_cast<http_client::nosend::Session>(no_send_client->session_);

    EXPECT_CALL(*mock_session, SendRequest)
        .WillOnce([&mock_session](
                      const std::shared_ptr<opentelemetry::ext::http::client::EventHandler>
                          &callback) {
          opentelemetry::proto::collector::metrics::v1::ExportMetricsServiceRequest request_body;
          request_body.ParseFromArray(&mock_session->GetRequest()->body_[0],
                                      static_cast<int>(mock_session->GetRequest()->body_.size()));
          auto &scope_metrics = request_body.resource_metrics(0).scope_metrics(0);
          auto &scope         = scope_metrics.scope();
          EXPECT_EQ("library_name", scope.name());
          EXPECT_EQ("1.5.0", scope.version());

          auto &metric = scope_metrics.metrics(0);
          EXPECT_EQ("metrics_library_name", metric.name());
          EXPECT_EQ("metrics_description", metric.description());
          EXPECT_EQ("metrics_unit", metric.unit());

          auto &data_points = metric.gauge().data_points();
          EXPECT_EQ(10.0, data_points.Get(0).as_double());
          bool has_attributes = false;
          for (auto &kv : data_points.Get(0).attributes())
          {
            if (kv.key() == "a1")
            {
              EXPECT_EQ("b1", kv.value().string_value());
              has_attributes = true;
            }
          }
          EXPECT_TRUE(has_attributes);

          EXPECT_EQ(20, data_points.Get(1).as_int());
          has_attributes = false;
          for (auto &kv : data_points.Get(1).attributes())
          {
            if (kv.key() == "a2")
            {
              EXPECT_EQ("b2", kv.value().string_value());
              has_attributes = true;
            }
          }
          EXPECT_TRUE(has_attributes);

          http_client::nosend::Response response;
          response.Finish(*callback.get());
        });

    auto result = exporter->Export(data);
    EXPECT_EQ(result, opentelemetry::sdk::common::ExportResult::kSuccess);

    exporter->ForceFlush();
  }

  void ExportJsonIntegrationTestExportHistogramPointData(
#ifdef ENABLE_ASYNC_EXPORT
      bool async_mode
#endif
  )
  {
    auto mock_otlp_client =
        OtlpHttpMetricExporterTestPeer::GetMockOtlpHttpClient(HttpRequestContentType::kJson
#ifdef ENABLE_ASYNC_EXPORT
                                                              ,
                                                              async_mode
#endif
        );
    auto mock_otlp_http_client = mock_otlp_client.first;
    auto client                = mock_otlp_client.second;
    auto exporter = GetExporter(std::unique_ptr<OtlpHttpClient>{mock_otlp_http_client});

    opentelemetry::sdk::metrics::HistogramPointData histogram_point_data{};
    histogram_point_data.boundaries_ = {10.1, 20.2, 30.2};
    histogram_point_data.count_      = 3;
    histogram_point_data.counts_     = {200, 300, 400, 500};
    histogram_point_data.sum_        = 900.5;
    histogram_point_data.min_        = 1.8;
    histogram_point_data.max_        = 19.0;
    opentelemetry::sdk::metrics::HistogramPointData histogram_point_data2{};
    histogram_point_data2.boundaries_ = {10.0, 20.0, 30.0};
    histogram_point_data2.count_      = 3;
    histogram_point_data2.counts_     = {200, 300, 400, 500};
    histogram_point_data2.sum_        = static_cast<int64_t>(900);

    opentelemetry::sdk::metrics::MetricData metric_data{
        opentelemetry::sdk::metrics::InstrumentDescriptor{
            "metrics_library_name", "metrics_description", "metrics_unit",
            opentelemetry::sdk::metrics::InstrumentType::kHistogram,
            opentelemetry::sdk::metrics::InstrumentValueType::kDouble},
        opentelemetry::sdk::metrics::AggregationTemporality::kDelta,
        opentelemetry::common::SystemTimestamp{}, opentelemetry::common::SystemTimestamp{},
        std::vector<opentelemetry::sdk::metrics::PointDataAttributes>{
            {opentelemetry::sdk::metrics::PointAttributes{{"a1", "b1"}}, histogram_point_data},
            {opentelemetry::sdk::metrics::PointAttributes{{"a2", "b2"}}, histogram_point_data2}}};

    opentelemetry::sdk::metrics::ResourceMetrics data;
    auto resource = opentelemetry::sdk::resource::Resource::Create(
        opentelemetry::sdk::resource::ResourceAttributes{});
    data.resource_ = &resource;
    auto scope     = opentelemetry::sdk::instrumentationscope::InstrumentationScope::Create(
        "library_name", "1.5.0");
    data.scope_metric_data_ = std::vector<opentelemetry::sdk::metrics::ScopeMetrics>{
        {scope.get(), std::vector<opentelemetry::sdk::metrics::MetricData>{metric_data}}};

    auto no_send_client = std::static_pointer_cast<http_client::nosend::HttpClient>(client);
    auto mock_session =
        std::static_pointer_cast<http_client::nosend::Session>(no_send_client->session_);
    EXPECT_CALL(*mock_session, SendRequest)
        .WillOnce(
            [&mock_session](
                const std::shared_ptr<opentelemetry::ext::http::client::EventHandler> &callback) {
              auto check_json =
                  nlohmann::json::parse(mock_session->GetRequest()->body_, nullptr, false);

              auto resource_metrics = *check_json["resourceMetrics"].begin();
              auto scope_metrics    = *resource_metrics["scopeMetrics"].begin();
              auto scope            = scope_metrics["scope"];
              EXPECT_EQ("library_name", scope["name"].get<std::string>());
              EXPECT_EQ("1.5.0", scope["version"].get<std::string>());

              auto metric = *scope_metrics["metrics"].begin();
              EXPECT_EQ("metrics_library_name", metric["name"].get<std::string>());
              EXPECT_EQ("metrics_description", metric["description"].get<std::string>());
              EXPECT_EQ("metrics_unit", metric["unit"].get<std::string>());

              auto data_points = metric["histogram"]["dataPoints"];
              EXPECT_EQ(3, JsonToInteger<int64_t>(data_points[0]["count"]));
              EXPECT_EQ(900.5, data_points[0]["sum"].get<double>());
              EXPECT_EQ(1.8, data_points[0]["min"].get<double>());
              EXPECT_EQ(19, data_points[0]["max"].get<double>());
              EXPECT_EQ(4, data_points[0]["bucketCounts"].size());
              if (4 == data_points[0]["bucketCounts"].size())
              {
                EXPECT_EQ(200, JsonToInteger<int64_t>(data_points[0]["bucketCounts"][0]));
                EXPECT_EQ(300, JsonToInteger<int64_t>(data_points[0]["bucketCounts"][1]));
                EXPECT_EQ(400, JsonToInteger<int64_t>(data_points[0]["bucketCounts"][2]));
                EXPECT_EQ(500, JsonToInteger<int64_t>(data_points[0]["bucketCounts"][3]));
              }
              EXPECT_EQ(3, data_points[0]["explicitBounds"].size());
              if (3 == data_points[0]["explicitBounds"].size())
              {
                EXPECT_EQ(10.1, data_points[0]["explicitBounds"][0].get<double>());
                EXPECT_EQ(20.2, data_points[0]["explicitBounds"][1].get<double>());
                EXPECT_EQ(30.2, data_points[0]["explicitBounds"][2].get<double>());
              }

              EXPECT_EQ(3, JsonToInteger<int64_t>(data_points[1]["count"]));
              EXPECT_EQ(900.0, data_points[1]["sum"].get<double>());
              EXPECT_EQ(4, data_points[1]["bucketCounts"].size());
              if (4 == data_points[1]["bucketCounts"].size())
              {
                EXPECT_EQ(200, JsonToInteger<int64_t>(data_points[1]["bucketCounts"][0]));
                EXPECT_EQ(300, JsonToInteger<int64_t>(data_points[1]["bucketCounts"][1]));
                EXPECT_EQ(400, JsonToInteger<int64_t>(data_points[1]["bucketCounts"][2]));
                EXPECT_EQ(500, JsonToInteger<int64_t>(data_points[1]["bucketCounts"][3]));
              }
              EXPECT_EQ(3, data_points[1]["explicitBounds"].size());
              if (3 == data_points[1]["explicitBounds"].size())
              {
                EXPECT_EQ(10.0, data_points[1]["explicitBounds"][0].get<double>());
                EXPECT_EQ(20.0, data_points[1]["explicitBounds"][1].get<double>());
                EXPECT_EQ(30.0, data_points[1]["explicitBounds"][2].get<double>());
              }

              auto custom_header = mock_session->GetRequest()->headers_.find("Custom-Header-Key");
              ASSERT_TRUE(custom_header != mock_session->GetRequest()->headers_.end());
              if (custom_header != mock_session->GetRequest()->headers_.end())
              {
                EXPECT_EQ("Custom-Header-Value", custom_header->second);
              }

              http_client::nosend::Response response;
              response.Finish(*callback.get());
            });

    auto result = exporter->Export(data);
    EXPECT_EQ(result, opentelemetry::sdk::common::ExportResult::kSuccess);

    exporter->ForceFlush();
  }

  void ExportBinaryIntegrationTestExportHistogramPointData(
#ifdef ENABLE_ASYNC_EXPORT
      bool async_mode
#endif
  )
  {
    auto mock_otlp_client =
        OtlpHttpMetricExporterTestPeer::GetMockOtlpHttpClient(HttpRequestContentType::kBinary
#ifdef ENABLE_ASYNC_EXPORT
                                                              ,
                                                              async_mode
#endif
        );
    auto mock_otlp_http_client = mock_otlp_client.first;
    auto client                = mock_otlp_client.second;
    auto exporter = GetExporter(std::unique_ptr<OtlpHttpClient>{mock_otlp_http_client});

    opentelemetry::sdk::metrics::SumPointData sum_point_data{};
    sum_point_data.value_ = 10.0;
    opentelemetry::sdk::metrics::SumPointData sum_point_data2{};
    sum_point_data2.value_ = 20.0;
    opentelemetry::sdk::metrics::ResourceMetrics data;
    auto resource = opentelemetry::sdk::resource::Resource::Create(
        opentelemetry::sdk::resource::ResourceAttributes{});
    data.resource_ = &resource;
    auto scope     = opentelemetry::sdk::instrumentationscope::InstrumentationScope::Create(
        "library_name", "1.5.0");

    opentelemetry::sdk::metrics::HistogramPointData histogram_point_data{};
    histogram_point_data.boundaries_ = {10.1, 20.2, 30.2};
    histogram_point_data.count_      = 3;
    histogram_point_data.counts_     = {200, 300, 400, 500};
    histogram_point_data.sum_        = 900.5;
    opentelemetry::sdk::metrics::HistogramPointData histogram_point_data2{};
    histogram_point_data2.boundaries_ = {10.0, 20.0, 30.0};
    histogram_point_data2.count_      = 3;
    histogram_point_data2.counts_     = {200, 300, 400, 500};
    histogram_point_data2.sum_        = static_cast<int64_t>(900);

    opentelemetry::sdk::metrics::MetricData metric_data{
        opentelemetry::sdk::metrics::InstrumentDescriptor{
            "metrics_library_name", "metrics_description", "metrics_unit",
            opentelemetry::sdk::metrics::InstrumentType::kHistogram,
            opentelemetry::sdk::metrics::InstrumentValueType::kDouble},
        opentelemetry::sdk::metrics::AggregationTemporality::kDelta,
        opentelemetry::common::SystemTimestamp{}, opentelemetry::common::SystemTimestamp{},
        std::vector<opentelemetry::sdk::metrics::PointDataAttributes>{
            {opentelemetry::sdk::metrics::PointAttributes{{"a1", "b1"}}, histogram_point_data},
            {opentelemetry::sdk::metrics::PointAttributes{{"a2", "b2"}}, histogram_point_data2}}};

    data.scope_metric_data_ = std::vector<opentelemetry::sdk::metrics::ScopeMetrics>{
        {scope.get(), std::vector<opentelemetry::sdk::metrics::MetricData>{metric_data}}};

    auto no_send_client = std::static_pointer_cast<http_client::nosend::HttpClient>(client);
    auto mock_session =
        std::static_pointer_cast<http_client::nosend::Session>(no_send_client->session_);

    EXPECT_CALL(*mock_session, SendRequest)
        .WillOnce([&mock_session](
                      const std::shared_ptr<opentelemetry::ext::http::client::EventHandler>
                          &callback) {
          opentelemetry::proto::collector::metrics::v1::ExportMetricsServiceRequest request_body;
          request_body.ParseFromArray(&mock_session->GetRequest()->body_[0],
                                      static_cast<int>(mock_session->GetRequest()->body_.size()));
          auto &scope_metrics = request_body.resource_metrics(0).scope_metrics(0);
          auto &scope         = scope_metrics.scope();
          EXPECT_EQ("library_name", scope.name());
          EXPECT_EQ("1.5.0", scope.version());

          auto &metric = scope_metrics.metrics(0);
          EXPECT_EQ("metrics_library_name", metric.name());
          EXPECT_EQ("metrics_description", metric.description());
          EXPECT_EQ("metrics_unit", metric.unit());

          auto &data_points = metric.histogram().data_points();
          EXPECT_EQ(3, data_points.Get(0).count());
          EXPECT_EQ(900.5, data_points.Get(0).sum());
          EXPECT_EQ(4, data_points.Get(0).bucket_counts_size());
          if (4 == data_points.Get(0).bucket_counts_size())
          {
            EXPECT_EQ(200, data_points.Get(0).bucket_counts(0));
            EXPECT_EQ(300, data_points.Get(0).bucket_counts(1));
            EXPECT_EQ(400, data_points.Get(0).bucket_counts(2));
            EXPECT_EQ(500, data_points.Get(0).bucket_counts(3));
          }
          EXPECT_EQ(3, data_points.Get(0).explicit_bounds_size());
          if (3 == data_points.Get(0).explicit_bounds_size())
          {
            EXPECT_EQ(10.1, data_points.Get(0).explicit_bounds(0));
            EXPECT_EQ(20.2, data_points.Get(0).explicit_bounds(1));
            EXPECT_EQ(30.2, data_points.Get(0).explicit_bounds(2));
          }

          bool has_attributes = false;
          for (auto &kv : data_points.Get(0).attributes())
          {
            if (kv.key() == "a1")
            {
              EXPECT_EQ("b1", kv.value().string_value());
              has_attributes = true;
            }
          }
          EXPECT_TRUE(has_attributes);

          EXPECT_EQ(3, data_points.Get(1).count());
          EXPECT_EQ(900l, data_points.Get(1).sum());
          EXPECT_EQ(4, data_points.Get(1).bucket_counts_size());
          if (4 == data_points.Get(1).bucket_counts_size())
          {
            EXPECT_EQ(200, data_points.Get(1).bucket_counts(0));
            EXPECT_EQ(300, data_points.Get(1).bucket_counts(1));
            EXPECT_EQ(400, data_points.Get(1).bucket_counts(2));
            EXPECT_EQ(500, data_points.Get(1).bucket_counts(3));
          }
          EXPECT_EQ(3, data_points.Get(1).explicit_bounds_size());
          if (3 == data_points.Get(1).explicit_bounds_size())
          {
            EXPECT_EQ(10, data_points.Get(1).explicit_bounds(0));
            EXPECT_EQ(20, data_points.Get(1).explicit_bounds(1));
            EXPECT_EQ(30, data_points.Get(1).explicit_bounds(2));
          }
          has_attributes = false;
          for (auto &kv : data_points.Get(1).attributes())
          {
            if (kv.key() == "a2")
            {
              EXPECT_EQ("b2", kv.value().string_value());
              has_attributes = true;
            }
          }
          EXPECT_TRUE(has_attributes);

          http_client::nosend::Response response;
          response.Finish(*callback.get());
        });

    auto result = exporter->Export(data);
    EXPECT_EQ(result, opentelemetry::sdk::common::ExportResult::kSuccess);

    exporter->ForceFlush();
  }
};

TEST(OtlpHttpMetricExporterTest, Shutdown)
{
  auto exporter = std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>(
      new OtlpHttpMetricExporter());
  ASSERT_TRUE(exporter->Shutdown());
  auto result = exporter->Export(opentelemetry::sdk::metrics::ResourceMetrics{});
  EXPECT_EQ(result, opentelemetry::sdk::common::ExportResult::kFailure);
}

#ifdef ENABLE_ASYNC_EXPORT
TEST_F(OtlpHttpMetricExporterTestPeer, ExportJsonIntegrationTestSumPointDataAsync)
{
  ExportJsonIntegrationTestExportSumPointData(true);
}
TEST_F(OtlpHttpMetricExporterTestPeer, ExportJsonIntegrationTestSumPointDataSync)
{
  ExportJsonIntegrationTestExportSumPointData(false);
}
TEST_F(OtlpHttpMetricExporterTestPeer, ExportBinaryIntegrationTestSumPointDataAsync)
{
  ExportBinaryIntegrationTestExportSumPointData(true);
}
TEST_F(OtlpHttpMetricExporterTestPeer, ExportBinaryIntegrationTestSumPointDataSync)
{
  ExportBinaryIntegrationTestExportSumPointData(false);
}

TEST_F(OtlpHttpMetricExporterTestPeer, ExportJsonIntegrationTestLastValuePointDataAsync)
{
  ExportJsonIntegrationTestExportLastValuePointData(true);
}
TEST_F(OtlpHttpMetricExporterTestPeer, ExportJsonIntegrationTestLastValuePointDataSync)
{
  ExportJsonIntegrationTestExportLastValuePointData(false);
}
TEST_F(OtlpHttpMetricExporterTestPeer, ExportBinaryIntegrationTestLastValuePointDataAsync)
{
  ExportBinaryIntegrationTestExportLastValuePointData(true);
}
TEST_F(OtlpHttpMetricExporterTestPeer, ExportBinaryIntegrationTestLastValuePointDataSync)
{
  ExportBinaryIntegrationTestExportLastValuePointData(false);
}

TEST_F(OtlpHttpMetricExporterTestPeer, ExportJsonIntegrationTestHistogramPointDataAsync)
{
  ExportJsonIntegrationTestExportHistogramPointData(true);
}
TEST_F(OtlpHttpMetricExporterTestPeer, ExportJsonIntegrationTestHistogramPointDataSync)
{
  ExportJsonIntegrationTestExportHistogramPointData(false);
}
TEST_F(OtlpHttpMetricExporterTestPeer, ExportBinaryIntegrationTestHistogramPointDataAsync)
{
  ExportBinaryIntegrationTestExportHistogramPointData(true);
}
TEST_F(OtlpHttpMetricExporterTestPeer, ExportBinaryIntegrationTestHistogramPointDataSync)
{
  ExportBinaryIntegrationTestExportHistogramPointData(false);
}

#else
TEST_F(OtlpHttpMetricExporterTestPeer, ExportJsonIntegrationTestSumPointData)
{
  ExportJsonIntegrationTestExportSumPointData();
}
TEST_F(OtlpHttpMetricExporterTestPeer, ExportBinaryIntegrationTestSumPointData)
{
  ExportBinaryIntegrationTestExportSumPointData();
}

TEST_F(OtlpHttpMetricExporterTestPeer, ExportJsonIntegrationTestLastValuePointData)
{
  ExportJsonIntegrationTestExportLastValuePointData();
}
TEST_F(OtlpHttpMetricExporterTestPeer, ExportBinaryIntegrationTestLastValuePointData)
{
  ExportBinaryIntegrationTestExportLastValuePointData();
}

TEST_F(OtlpHttpMetricExporterTestPeer, ExportJsonIntegrationTestHistogramPointData)
{
  ExportJsonIntegrationTestExportHistogramPointData();
}
TEST_F(OtlpHttpMetricExporterTestPeer, ExportBinaryIntegrationTestHistogramPointData)
{
  ExportBinaryIntegrationTestExportHistogramPointData();
}
#endif

// Test exporter configuration options
TEST_F(OtlpHttpMetricExporterTestPeer, ConfigTest)
{
  OtlpHttpMetricExporterOptions opts;
  opts.url = "http://localhost:45456/v1/metrics";
  std::unique_ptr<OtlpHttpMetricExporter> exporter(new OtlpHttpMetricExporter(opts));
  EXPECT_EQ(GetOptions(exporter).url, "http://localhost:45456/v1/metrics");
}

// Test exporter configuration options with use_json_name
TEST_F(OtlpHttpMetricExporterTestPeer, ConfigUseJsonNameTest)
{
  OtlpHttpMetricExporterOptions opts;
  opts.use_json_name = true;
  std::unique_ptr<OtlpHttpMetricExporter> exporter(new OtlpHttpMetricExporter(opts));
  EXPECT_EQ(GetOptions(exporter).use_json_name, true);
}

// Test exporter configuration options with json_bytes_mapping=JsonBytesMappingKind::kHex
TEST_F(OtlpHttpMetricExporterTestPeer, ConfigJsonBytesMappingTest)
{
  OtlpHttpMetricExporterOptions opts;
  opts.json_bytes_mapping = JsonBytesMappingKind::kHex;
  std::unique_ptr<OtlpHttpMetricExporter> exporter(new OtlpHttpMetricExporter(opts));
  EXPECT_EQ(GetOptions(exporter).json_bytes_mapping, JsonBytesMappingKind::kHex);
  google::protobuf::ShutdownProtobufLibrary();
}

TEST(OtlpHttpMetricExporterTest, ConfigDefaultProtocolTest)
{
  OtlpHttpMetricExporterOptions opts;
  EXPECT_EQ(opts.content_type, HttpRequestContentType::kBinary);
}

#ifndef NO_GETENV
// Test exporter configuration options with use_ssl_credentials
TEST_F(OtlpHttpMetricExporterTestPeer, ConfigFromEnv)
{
  const std::string url = "http://localhost:9999/v1/metrics";
  setenv("OTEL_EXPORTER_OTLP_ENDPOINT", "http://localhost:9999", 1);
  setenv("OTEL_EXPORTER_OTLP_TIMEOUT", "20s", 1);
  setenv("OTEL_EXPORTER_OTLP_HEADERS", "k1=v1,k2=v2", 1);
  setenv("OTEL_EXPORTER_OTLP_METRICS_HEADERS", "k1=v3,k1=v4", 1);
  setenv("OTEL_EXPORTER_OTLP_PROTOCOL", "http/json", 1);

  std::unique_ptr<OtlpHttpMetricExporter> exporter(new OtlpHttpMetricExporter());
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
  unsetenv("OTEL_EXPORTER_OTLP_METRICS_HEADERS");
  unsetenv("OTEL_EXPORTER_OTLP_PROTOCOL");
}

TEST_F(OtlpHttpMetricExporterTestPeer, ConfigFromMetricsEnv)
{
  const std::string url = "http://localhost:9999/v1/metrics";
  setenv("OTEL_EXPORTER_OTLP_METRICS_ENDPOINT", url.c_str(), 1);
  setenv("OTEL_EXPORTER_OTLP_METRICS_TIMEOUT", "20s", 1);
  setenv("OTEL_EXPORTER_OTLP_HEADERS", "k1=v1,k2=v2", 1);
  setenv("OTEL_EXPORTER_OTLP_METRICS_HEADERS", "k1=v3,k1=v4", 1);
  setenv("OTEL_EXPORTER_OTLP_METRICS_PROTOCOL", "http/json", 1);

  std::unique_ptr<OtlpHttpMetricExporter> exporter(new OtlpHttpMetricExporter());
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

  unsetenv("OTEL_EXPORTER_OTLP_METRICS_ENDPOINT");
  unsetenv("OTEL_EXPORTER_OTLP_METRICS_TIMEOUT");
  unsetenv("OTEL_EXPORTER_OTLP_HEADERS");
  unsetenv("OTEL_EXPORTER_OTLP_METRICS_HEADERS");
  unsetenv("OTEL_EXPORTER_OTLP_METRICS_PROTOCOL");
}

TEST_F(OtlpHttpMetricExporterTestPeer, DefaultEndpoint)
{
  EXPECT_EQ("http://localhost:4318/v1/metrics", GetOtlpDefaultMetricsEndpoint());
}

TEST_F(OtlpHttpMetricExporterTestPeer, CheckDefaultTemporality)
{
  std::unique_ptr<OtlpHttpMetricExporter> exporter(new OtlpHttpMetricExporter());
  EXPECT_EQ(
      opentelemetry::sdk::metrics::AggregationTemporality::kCumulative,
      exporter->GetAggregationTemporality(opentelemetry::sdk::metrics::InstrumentType::kCounter));
  EXPECT_EQ(
      opentelemetry::sdk::metrics::AggregationTemporality::kCumulative,
      exporter->GetAggregationTemporality(opentelemetry::sdk::metrics::InstrumentType::kHistogram));
  EXPECT_EQ(opentelemetry::sdk::metrics::AggregationTemporality::kCumulative,
            exporter->GetAggregationTemporality(
                opentelemetry::sdk::metrics::InstrumentType::kUpDownCounter));
  EXPECT_EQ(opentelemetry::sdk::metrics::AggregationTemporality::kCumulative,
            exporter->GetAggregationTemporality(
                opentelemetry::sdk::metrics::InstrumentType::kObservableCounter));
  EXPECT_EQ(opentelemetry::sdk::metrics::AggregationTemporality::kCumulative,
            exporter->GetAggregationTemporality(
                opentelemetry::sdk::metrics::InstrumentType::kObservableGauge));
  EXPECT_EQ(opentelemetry::sdk::metrics::AggregationTemporality::kCumulative,
            exporter->GetAggregationTemporality(
                opentelemetry::sdk::metrics::InstrumentType::kObservableUpDownCounter));
}

TEST_F(OtlpHttpMetricExporterTestPeer, ConfigRetryDefaultValues)
{
  std::unique_ptr<OtlpHttpMetricExporter> exporter(new OtlpHttpMetricExporter());
  const auto options = GetOptions(exporter);
  ASSERT_EQ(options.retry_policy_max_attempts, 5);
  ASSERT_FLOAT_EQ(options.retry_policy_initial_backoff.count(), 1.0f);
  ASSERT_FLOAT_EQ(options.retry_policy_max_backoff.count(), 5.0f);
  ASSERT_FLOAT_EQ(options.retry_policy_backoff_multiplier, 1.5f);
}

TEST_F(OtlpHttpMetricExporterTestPeer, ConfigRetryValuesFromEnv)
{
  setenv("OTEL_CPP_EXPORTER_OTLP_METRICS_RETRY_MAX_ATTEMPTS", "123", 1);
  setenv("OTEL_CPP_EXPORTER_OTLP_METRICS_RETRY_INITIAL_BACKOFF", "4.5", 1);
  setenv("OTEL_CPP_EXPORTER_OTLP_METRICS_RETRY_MAX_BACKOFF", "6.7", 1);
  setenv("OTEL_CPP_EXPORTER_OTLP_METRICS_RETRY_BACKOFF_MULTIPLIER", "8.9", 1);

  std::unique_ptr<OtlpHttpMetricExporter> exporter(new OtlpHttpMetricExporter());
  const auto options = GetOptions(exporter);
  ASSERT_EQ(options.retry_policy_max_attempts, 123);
  ASSERT_FLOAT_EQ(options.retry_policy_initial_backoff.count(), 4.5f);
  ASSERT_FLOAT_EQ(options.retry_policy_max_backoff.count(), 6.7f);
  ASSERT_FLOAT_EQ(options.retry_policy_backoff_multiplier, 8.9f);

  unsetenv("OTEL_CPP_EXPORTER_OTLP_METRICS_RETRY_MAX_ATTEMPTS");
  unsetenv("OTEL_CPP_EXPORTER_OTLP_METRICS_RETRY_INITIAL_BACKOFF");
  unsetenv("OTEL_CPP_EXPORTER_OTLP_METRICS_RETRY_MAX_BACKOFF");
  unsetenv("OTEL_CPP_EXPORTER_OTLP_METRICS_RETRY_BACKOFF_MULTIPLIER");
}

TEST_F(OtlpHttpMetricExporterTestPeer, ConfigRetryGenericValuesFromEnv)
{
  setenv("OTEL_CPP_EXPORTER_OTLP_RETRY_MAX_ATTEMPTS", "321", 1);
  setenv("OTEL_CPP_EXPORTER_OTLP_RETRY_INITIAL_BACKOFF", "5.4", 1);
  setenv("OTEL_CPP_EXPORTER_OTLP_RETRY_MAX_BACKOFF", "7.6", 1);
  setenv("OTEL_CPP_EXPORTER_OTLP_RETRY_BACKOFF_MULTIPLIER", "9.8", 1);

  std::unique_ptr<OtlpHttpMetricExporter> exporter(new OtlpHttpMetricExporter());
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

// Test Preferred aggregtion temporality selection
TEST_F(OtlpHttpMetricExporterTestPeer, PreferredAggergationTemporality)
{
  // Cummulative aggregation selector : use cummulative aggregation for all instruments.
  std::unique_ptr<OtlpHttpMetricExporter> exporter(new OtlpHttpMetricExporter());
  EXPECT_EQ(GetOptions(exporter).aggregation_temporality,
            PreferredAggregationTemporality::kCumulative);
  auto cumm_selector =
      OtlpMetricUtils::ChooseTemporalitySelector(GetOptions(exporter).aggregation_temporality);
  EXPECT_EQ(cumm_selector(opentelemetry::sdk::metrics::InstrumentType::kCounter),
            opentelemetry::sdk::metrics::AggregationTemporality::kCumulative);
  EXPECT_EQ(cumm_selector(opentelemetry::sdk::metrics::InstrumentType::kHistogram),
            opentelemetry::sdk::metrics::AggregationTemporality::kCumulative);
  EXPECT_EQ(cumm_selector(opentelemetry::sdk::metrics::InstrumentType::kUpDownCounter),
            opentelemetry::sdk::metrics::AggregationTemporality::kCumulative);
  EXPECT_EQ(cumm_selector(opentelemetry::sdk::metrics::InstrumentType::kObservableCounter),
            opentelemetry::sdk::metrics::AggregationTemporality::kCumulative);
  EXPECT_EQ(cumm_selector(opentelemetry::sdk::metrics::InstrumentType::kObservableGauge),
            opentelemetry::sdk::metrics::AggregationTemporality::kCumulative);
  EXPECT_EQ(cumm_selector(opentelemetry::sdk::metrics::InstrumentType::kObservableUpDownCounter),
            opentelemetry::sdk::metrics::AggregationTemporality::kCumulative);

  // LowMemory aggregation selector use:
  //   - cummulative aggregtion for Counter and Histogram
  //   - delta aggregation for up-down counter, observable counter, observable gauge, observable
  //   up-down counter
  OtlpHttpMetricExporterOptions opts2;
  opts2.aggregation_temporality = PreferredAggregationTemporality::kLowMemory;
  std::unique_ptr<OtlpHttpMetricExporter> exporter2(new OtlpHttpMetricExporter(opts2));
  EXPECT_EQ(GetOptions(exporter2).aggregation_temporality,
            PreferredAggregationTemporality::kLowMemory);
  auto lowmemory_selector =
      OtlpMetricUtils::ChooseTemporalitySelector(GetOptions(exporter2).aggregation_temporality);
  EXPECT_EQ(lowmemory_selector(opentelemetry::sdk::metrics::InstrumentType::kCounter),
            opentelemetry::sdk::metrics::AggregationTemporality::kDelta);
  EXPECT_EQ(lowmemory_selector(opentelemetry::sdk::metrics::InstrumentType::kHistogram),
            opentelemetry::sdk::metrics::AggregationTemporality::kDelta);

  EXPECT_EQ(lowmemory_selector(opentelemetry::sdk::metrics::InstrumentType::kUpDownCounter),
            opentelemetry::sdk::metrics::AggregationTemporality::kCumulative);
  EXPECT_EQ(lowmemory_selector(opentelemetry::sdk::metrics::InstrumentType::kObservableCounter),
            opentelemetry::sdk::metrics::AggregationTemporality::kCumulative);
  EXPECT_EQ(lowmemory_selector(opentelemetry::sdk::metrics::InstrumentType::kObservableGauge),
            opentelemetry::sdk::metrics::AggregationTemporality::kCumulative);
  EXPECT_EQ(
      lowmemory_selector(opentelemetry::sdk::metrics::InstrumentType::kObservableUpDownCounter),
      opentelemetry::sdk::metrics::AggregationTemporality::kCumulative);

  // Delta aggregation selector use:
  //   - delta aggregtion for Counter, Histogram, Observable Counter, Observable Gauge
  //   - cummulative aggregation for up-down counter, observable up-down counter
  OtlpHttpMetricExporterOptions opts3;
  opts3.aggregation_temporality = PreferredAggregationTemporality::kDelta;
  std::unique_ptr<OtlpHttpMetricExporter> exporter3(new OtlpHttpMetricExporter(opts3));
  EXPECT_EQ(GetOptions(exporter3).aggregation_temporality, PreferredAggregationTemporality::kDelta);
  auto delta_selector =
      OtlpMetricUtils::ChooseTemporalitySelector(GetOptions(exporter3).aggregation_temporality);
  EXPECT_EQ(delta_selector(opentelemetry::sdk::metrics::InstrumentType::kCounter),
            opentelemetry::sdk::metrics::AggregationTemporality::kDelta);
  EXPECT_EQ(delta_selector(opentelemetry::sdk::metrics::InstrumentType::kHistogram),
            opentelemetry::sdk::metrics::AggregationTemporality::kDelta);
  EXPECT_EQ(delta_selector(opentelemetry::sdk::metrics::InstrumentType::kObservableCounter),
            opentelemetry::sdk::metrics::AggregationTemporality::kDelta);
  EXPECT_EQ(delta_selector(opentelemetry::sdk::metrics::InstrumentType::kObservableGauge),
            opentelemetry::sdk::metrics::AggregationTemporality::kDelta);

  EXPECT_EQ(delta_selector(opentelemetry::sdk::metrics::InstrumentType::kUpDownCounter),
            opentelemetry::sdk::metrics::AggregationTemporality::kCumulative);
  EXPECT_EQ(delta_selector(opentelemetry::sdk::metrics::InstrumentType::kObservableUpDownCounter),
            opentelemetry::sdk::metrics::AggregationTemporality::kCumulative);
}

}  // namespace otlp
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
