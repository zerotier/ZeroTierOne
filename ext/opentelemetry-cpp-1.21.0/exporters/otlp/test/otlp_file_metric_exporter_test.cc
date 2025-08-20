// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "opentelemetry/common/timestamp.h"
#include "opentelemetry/exporters/otlp/otlp_file_client_options.h"
#include "opentelemetry/exporters/otlp/otlp_file_metric_exporter.h"
#include "opentelemetry/exporters/otlp/otlp_file_metric_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_file_metric_exporter_options.h"
#include "opentelemetry/exporters/otlp/otlp_metric_utils.h"
#include "opentelemetry/exporters/otlp/otlp_preferred_temporality.h"
#include "opentelemetry/nostd/unique_ptr.h"
#include "opentelemetry/sdk/common/exporter_utils.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/metrics/data/metric_data.h"
#include "opentelemetry/sdk/metrics/data/point_data.h"
#include "opentelemetry/sdk/metrics/export/metric_producer.h"
#include "opentelemetry/sdk/metrics/instruments.h"
#include "opentelemetry/sdk/metrics/push_metric_exporter.h"
#include "opentelemetry/sdk/resource/resource.h"
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

class ProtobufGlobalSymbolGuard
{
public:
  ProtobufGlobalSymbolGuard() {}
  ~ProtobufGlobalSymbolGuard() { google::protobuf::ShutdownProtobufLibrary(); }
};

template <class IntegerType>
static IntegerType JsonToInteger(nlohmann::json value)
{
  if (value.is_string())
  {
    return static_cast<IntegerType>(strtol(value.get<std::string>().c_str(), nullptr, 10));
  }

  return value.get<IntegerType>();
}

class OtlpFileMetricExporterTestPeer : public ::testing::Test
{
public:
  // Get the options associated with the given exporter.
  const OtlpFileMetricExporterOptions &GetOptions(std::unique_ptr<OtlpFileMetricExporter> &exporter)
  {
    return exporter->options_;
  }

  void ExportJsonIntegrationTestExportSumPointData()
  {
    static ProtobufGlobalSymbolGuard global_symbol_guard;

    std::stringstream output;
    OtlpFileMetricExporterOptions opts;
    opts.backend_options = std::ref(output);

    auto exporter = OtlpFileMetricExporterFactory::Create(opts);

    opentelemetry::sdk::metrics::SumPointData sum_point_data{};
    sum_point_data.value_ = 10.0;
    opentelemetry::sdk::metrics::SumPointData sum_point_data2{};
    sum_point_data2.value_ = 20.0;
    opentelemetry::sdk::metrics::ResourceMetrics data;

    auto resource = opentelemetry::sdk::resource::Resource::Create(
        opentelemetry::sdk::resource::ResourceAttributes{}, "resource_url");
    data.resource_ = &resource;

    auto scope = opentelemetry::sdk::instrumentationscope::InstrumentationScope::Create(
        "library_name", "1.5.0", "scope_url", {{"scope_key", "scope_value"}});

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

    auto result = exporter->Export(data);
    EXPECT_EQ(result, opentelemetry::sdk::common::ExportResult::kSuccess);

    exporter->ForceFlush();

    output.flush();
    output.sync();
    auto check_json_text = output.str();

    if (!check_json_text.empty())
    {
      auto check_json = nlohmann::json::parse(check_json_text, nullptr, false);

      auto resource_metrics = *check_json["resourceMetrics"].begin();
      auto scope_metrics    = *resource_metrics["scopeMetrics"].begin();
      auto scope            = scope_metrics["scope"];

      EXPECT_EQ("resource_url", resource_metrics["schemaUrl"].get<std::string>());
      EXPECT_EQ("library_name", scope["name"].get<std::string>());
      EXPECT_EQ("1.5.0", scope["version"].get<std::string>());
      EXPECT_EQ("scope_url", scope_metrics["schemaUrl"].get<std::string>());
      ASSERT_EQ(1, scope["attributes"].size());
      const auto scope_attribute = scope["attributes"].front();
      EXPECT_EQ("scope_key", scope_attribute["key"].get<std::string>());
      EXPECT_EQ("scope_value", scope_attribute["value"]["stringValue"].get<std::string>());

      auto metric = *scope_metrics["metrics"].begin();
      EXPECT_EQ("metrics_library_name", metric["name"].get<std::string>());
      EXPECT_EQ("metrics_description", metric["description"].get<std::string>());
      EXPECT_EQ("metrics_unit", metric["unit"].get<std::string>());

      auto data_points = metric["sum"]["dataPoints"];
      EXPECT_EQ(10.0, data_points[0]["asDouble"].get<double>());
      EXPECT_EQ(20.0, data_points[1]["asDouble"].get<double>());
    }
  }

  void ExportJsonIntegrationTestExportLastValuePointData()
  {
    std::stringstream output;
    OtlpFileMetricExporterOptions opts;
    opts.backend_options = std::ref(output);

    auto exporter = OtlpFileMetricExporterFactory::Create(opts);

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

    auto result = exporter->Export(data);
    EXPECT_EQ(result, opentelemetry::sdk::common::ExportResult::kSuccess);

    exporter->ForceFlush();

    output.flush();
    output.sync();
    auto check_json_text = output.str();
    if (!check_json_text.empty())
    {
      auto check_json = nlohmann::json::parse(check_json_text, nullptr, false);

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
    }
  }

  void ExportJsonIntegrationTestExportHistogramPointData()
  {
    std::stringstream output;
    OtlpFileMetricExporterOptions opts;
    opts.backend_options = std::ref(output);

    auto exporter = OtlpFileMetricExporterFactory::Create(opts);

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

    auto result = exporter->Export(data);
    EXPECT_EQ(result, opentelemetry::sdk::common::ExportResult::kSuccess);

    exporter->ForceFlush();

    output.flush();
    output.sync();
    auto check_json_text = output.str();
    if (!check_json_text.empty())
    {
      auto check_json = nlohmann::json::parse(check_json_text, nullptr, false);

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
    }
  }
};

TEST(OtlpFileMetricExporterTest, Shutdown)
{
  auto exporter = std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>(
      new OtlpFileMetricExporter());
  ASSERT_TRUE(exporter->Shutdown());
  auto result = exporter->Export(opentelemetry::sdk::metrics::ResourceMetrics{});
  EXPECT_EQ(result, opentelemetry::sdk::common::ExportResult::kFailure);
}

TEST_F(OtlpFileMetricExporterTestPeer, ExportJsonIntegrationTestSumPointDataSync)
{
  ExportJsonIntegrationTestExportSumPointData();
}

TEST_F(OtlpFileMetricExporterTestPeer, ExportJsonIntegrationTestLastValuePointDataSync)
{
  ExportJsonIntegrationTestExportLastValuePointData();
}

TEST_F(OtlpFileMetricExporterTestPeer, ExportJsonIntegrationTestHistogramPointDataSync)
{
  ExportJsonIntegrationTestExportHistogramPointData();
}

// Test Preferred aggregtion temporality selection
TEST_F(OtlpFileMetricExporterTestPeer, PreferredAggergationTemporality)
{
  // Cummulative aggregation selector : use cummulative aggregation for all instruments.
  std::unique_ptr<OtlpFileMetricExporter> exporter(new OtlpFileMetricExporter());
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
  OtlpFileMetricExporterOptions opts2;
  opts2.aggregation_temporality = PreferredAggregationTemporality::kLowMemory;
  std::unique_ptr<OtlpFileMetricExporter> exporter2(new OtlpFileMetricExporter(opts2));
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
  OtlpFileMetricExporterOptions opts3;
  opts3.aggregation_temporality = PreferredAggregationTemporality::kDelta;
  std::unique_ptr<OtlpFileMetricExporter> exporter3(new OtlpFileMetricExporter(opts3));
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
