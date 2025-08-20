// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <chrono>

// clang-format off
#include "opentelemetry/exporters/otlp/protobuf_include_prefix.h"  // IWYU pragma: keep
#include "opentelemetry/proto/collector/logs/v1/logs_service.pb.h"
#include "opentelemetry/proto/collector/trace/v1/trace_service.pb.h"
#include "opentelemetry/proto/collector/metrics/v1/metrics_service.pb.h"
#include "opentelemetry/proto/common/v1/common.pb.h"
#include "opentelemetry/proto/logs/v1/logs.pb.h"
#include "opentelemetry/proto/resource/v1/resource.pb.h"
#include "opentelemetry/proto/metrics/v1/metrics.pb.h"
#include "opentelemetry/proto/trace/v1/trace.pb.h"
#include "opentelemetry/exporters/otlp/protobuf_include_suffix.h"  // IWYU pragma: keep
// clang-format on

#include <opentelemetry/sdk/metrics/data/metric_data.h>

#include <opentelemetry/exporters/otlp/otlp_log_recordable.h>
#include <opentelemetry/exporters/otlp/otlp_metric_utils.h>
#include <opentelemetry/exporters/otlp/otlp_recordable.h>
#include <opentelemetry/exporters/otlp/otlp_recordable_utils.h>

namespace nostd         = opentelemetry::nostd;
namespace resource_sdk  = opentelemetry::sdk::resource;
namespace proto         = opentelemetry::proto;
namespace metrics_sdk   = opentelemetry::sdk::metrics;
namespace trace_sdk     = opentelemetry::sdk::trace;
namespace logs_sdk      = opentelemetry::sdk::logs;
namespace otlp_exporter = opentelemetry::exporter::otlp;

static metrics_sdk::MetricData CreateSumAggregationData()
{
  metrics_sdk::MetricData data;
  data.start_ts = opentelemetry::common::SystemTimestamp(std::chrono::system_clock::now());
  metrics_sdk::InstrumentDescriptor inst_desc = {"Counter", "desc", "unit",
                                                 metrics_sdk::InstrumentType::kCounter,
                                                 metrics_sdk::InstrumentValueType::kDouble};
  metrics_sdk::SumPointData s_data_1, s_data_2;
  s_data_1.value_ = 10.2;
  s_data_2.value_ = 20.2;

  data.aggregation_temporality = metrics_sdk::AggregationTemporality::kCumulative;
  data.end_ts = opentelemetry::common::SystemTimestamp(std::chrono::system_clock::now());
  data.instrument_descriptor = inst_desc;
  metrics_sdk::PointDataAttributes point_data_attr_1, point_data_attr_2;
  point_data_attr_1.attributes = {{"k1", "v1"}};
  point_data_attr_1.point_data = s_data_1;

  point_data_attr_2.attributes = {{"k2", "v2"}};
  point_data_attr_2.point_data = s_data_2;
  std::vector<metrics_sdk::PointDataAttributes> point_data_attr;
  point_data_attr.push_back(point_data_attr_1);
  point_data_attr.push_back(point_data_attr_2);
  data.point_data_attr_ = std::move(point_data_attr);
  return data;
}

TEST(ExportersOtlpCommon, OtlpSpanRecordable)
{
  auto resource = resource_sdk::Resource::Create({{"service.name", "one"}});
  auto scope    = trace_sdk::InstrumentationScope::Create("one", "1", "scope_schema",
                                                          {{"scope_key", "scope_value"}});
  ASSERT_TRUE(scope != nullptr);

  auto span_recordable = std::unique_ptr<trace_sdk::Recordable>(new otlp_exporter::OtlpRecordable);
  ASSERT_TRUE(span_recordable != nullptr);
  span_recordable->SetResource(resource);
  span_recordable->SetInstrumentationScope(*scope);
  span_recordable->SetAttribute("test", "test");
  span_recordable->SetName("test-name");

  proto::collector::trace::v1::ExportTraceServiceRequest request;
  std::vector<std::unique_ptr<trace_sdk::Recordable>> spans;
  spans.push_back(std::move(span_recordable));
  const nostd::span<std::unique_ptr<trace_sdk::Recordable>, 1> spans_span(spans.data(), 1);
  otlp_exporter::OtlpRecordableUtils::PopulateRequest(spans_span, &request);

  ASSERT_EQ(request.resource_spans().size(), 1);

  auto resource_spans = request.resource_spans().at(0);

  ASSERT_EQ(resource_spans.scope_spans().size(), 1);

  auto spans_proto = resource_spans.scope_spans().at(0).spans();
  auto &span       = spans_proto.at(0);

  EXPECT_EQ(span.name(), "test-name");
}

TEST(ExportersOtlpCommon, OtlpLogRecordable)
{
  auto resource = resource_sdk::Resource::Create({{"service.name", "one"}});
  auto scope    = trace_sdk::InstrumentationScope::Create("one", "1", "scope_schema",
                                                          {{"scope_key", "scope_value"}});
  ASSERT_TRUE(scope != nullptr);

  auto logs_recordable =
      std::unique_ptr<opentelemetry::sdk::logs::Recordable>(new otlp_exporter::OtlpLogRecordable);
  ASSERT_TRUE(logs_recordable != nullptr);

  logs_recordable->SetResource(resource);
  logs_recordable->SetInstrumentationScope(*scope);
  logs_recordable->SetAttribute("test", "test");
  logs_recordable->SetBody("testing 123");
  logs_recordable->SetSeverity(opentelemetry::logs::Severity::kInfo);

  std::vector<std::unique_ptr<logs_sdk::Recordable>> logs;
  logs.push_back(std::move(logs_recordable));

  opentelemetry::proto::collector::logs::v1::ExportLogsServiceRequest request;
  const opentelemetry::nostd::span<std::unique_ptr<opentelemetry::sdk::logs::Recordable>> logs_span(
      logs.data(), 1);

  otlp_exporter::OtlpRecordableUtils::PopulateRequest(logs_span, &request);
  ASSERT_EQ(request.resource_logs().size(), 1);
  auto logs_proto        = request.resource_logs().at(0).scope_logs();
  auto log_records_proto = logs_proto.at(0).log_records();

  ASSERT_EQ(log_records_proto.size(), 1);
  auto &log = log_records_proto.at(0);
  EXPECT_EQ(log.body().string_value(), "testing 123");
}

TEST(ExportersOtlpCommon, ExportMetricsServiceRequest)
{
  const auto resource = resource_sdk::Resource::Create({{"service.name", "test_service_name"}},
                                                       "resource_schema_url");
  const auto scope    = opentelemetry::sdk::instrumentationscope::InstrumentationScope::Create(
      "scope_name", "scope_version", "scope_schema_url", {{"scope_key", "scope_value"}});

  metrics_sdk::ScopeMetrics scope_metrics{scope.get(), CreateSumAggregationData()};
  metrics_sdk::ResourceMetrics resource_metrics{&resource, scope_metrics};

  proto::collector::metrics::v1::ExportMetricsServiceRequest request_proto;
  otlp_exporter::OtlpMetricUtils::PopulateRequest(resource_metrics, &request_proto);

  ASSERT_EQ(1, request_proto.resource_metrics_size());
  const auto &resource_metrics_proto = request_proto.resource_metrics(0);
  EXPECT_EQ("resource_schema_url", resource_metrics_proto.schema_url());

  ASSERT_EQ(1, resource_metrics_proto.scope_metrics_size());
  const auto &scope_metrics_proto = resource_metrics_proto.scope_metrics(0);
  EXPECT_EQ("scope_schema_url", scope_metrics_proto.schema_url());

  ASSERT_EQ(1, scope_metrics_proto.metrics_size());
  const auto &metric_proto = scope_metrics_proto.metrics(0);
  EXPECT_EQ("Counter", metric_proto.name());

  const auto &scope_proto = scope_metrics_proto.scope();
  EXPECT_EQ("scope_name", scope_proto.name());
  EXPECT_EQ("scope_version", scope_proto.version());

  ASSERT_EQ(1, scope_proto.attributes_size());
  const auto &scope_attributes_proto = scope_proto.attributes(0);
  EXPECT_EQ("scope_key", scope_attributes_proto.key());
  EXPECT_EQ("scope_value", scope_attributes_proto.value().string_value());
}