// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <prometheus/client_metric.h>
#include <prometheus/metric_family.h>
#include <prometheus/metric_type.h>
#include <stddef.h>
#include <limits>
#include <list>
#include <string>
#include <utility>
#include <vector>

#include "opentelemetry/common/timestamp.h"
#include "opentelemetry/exporters/prometheus/exporter_utils.h"
#include "opentelemetry/nostd/unique_ptr.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/metrics/data/metric_data.h"
#include "opentelemetry/sdk/metrics/export/metric_producer.h"
#include "opentelemetry/sdk/metrics/instruments.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/version.h"
#include "prometheus_test_helper.h"

using opentelemetry::exporter::metrics::PrometheusExporterUtils;
namespace metric_sdk        = opentelemetry::sdk::metrics;
namespace prometheus_client = ::prometheus;

OPENTELEMETRY_BEGIN_NAMESPACE

namespace exporter
{
namespace metrics
{
class SanitizeNameTester
{
public:
  static std::string sanitize(std::string name)
  {
    return PrometheusExporterUtils::SanitizeNames(std::move(name));
  }
  static std::string getPrometheusUnit(const std::string &unit_abbreviation)
  {
    return PrometheusExporterUtils::GetPrometheusUnit(unit_abbreviation);
  }
  static std::string getPrometheusPerUnit(const std::string &per_unit_abbreviation)
  {
    return PrometheusExporterUtils::GetPrometheusPerUnit(per_unit_abbreviation);
  }
  static std::string removeUnitPortionInBraces(const std::string &unit)
  {
    return PrometheusExporterUtils::RemoveUnitPortionInBraces(unit);
  }
  static std::string convertRateExpressedToPrometheusUnit(const std::string &rate_expressed_unit)
  {
    return PrometheusExporterUtils::ConvertRateExpressedToPrometheusUnit(rate_expressed_unit);
  }
  static std::string cleanUpString(const std::string &str)
  {
    return PrometheusExporterUtils::CleanUpString(str);
  }
  static std::string getEquivalentPrometheusUnit(const std::string &raw_metric_unit_name)
  {
    return PrometheusExporterUtils::GetEquivalentPrometheusUnit(raw_metric_unit_name);
  }
  static std::string mapToPrometheusName(const std::string &name,
                                         const std::string &unit,
                                         prometheus_client::MetricType prometheus_type,
                                         bool without_units       = false,
                                         bool without_type_suffix = false)
  {
    return PrometheusExporterUtils::MapToPrometheusName(name, unit, prometheus_type, without_units,
                                                        without_type_suffix);
  }
};
}  // namespace metrics
}  // namespace exporter

template <typename T>
void assert_basic(prometheus_client::MetricFamily &metric,
                  const std::string &expected_name,
                  const std::string &description,
                  prometheus_client::MetricType type,
                  size_t label_num,
                  std::vector<T> vals)
{
  ASSERT_EQ(metric.name, expected_name);  // name sanitized
  ASSERT_EQ(metric.help, description);    // description not changed
  ASSERT_EQ(metric.type, type);           // type translated

  // Prometheus metric data points should not have explicit timestamps
  for (const prometheus::ClientMetric &cm : metric.metric)
  {
    ASSERT_EQ(cm.timestamp_ms, 0);
  }

  auto metric_data = metric.metric[0];
  ASSERT_EQ(metric_data.label.size(), label_num);

  switch (type)
  {
    case prometheus_client::MetricType::Counter: {
      ASSERT_DOUBLE_EQ(metric_data.counter.value, vals[0]);
      break;
    }
    case prometheus_client::MetricType::Histogram: {
      ASSERT_DOUBLE_EQ(metric_data.histogram.sample_count, vals[0]);
      ASSERT_DOUBLE_EQ(metric_data.histogram.sample_sum, vals[1]);
      auto buckets = metric_data.histogram.bucket;
      ASSERT_EQ(buckets.size(), vals[2]);
      break;
    }
    case prometheus_client::MetricType::Gauge: {
      ASSERT_DOUBLE_EQ(metric_data.gauge.value, vals[0]);
      break;
    }
    break;
    case prometheus_client::MetricType::Summary:
      // Summary and Info type not supported
      ASSERT_TRUE(false);
      break;
    case prometheus::MetricType::Untyped:
    default:
      break;
  }
}

void assert_histogram(prometheus_client::MetricFamily &metric,
                      const std::list<double> &boundaries,
                      std::vector<int> correct)
{
  int cumulative_count = 0;
  auto buckets         = metric.metric[0].histogram.bucket;
  auto boundary_it     = boundaries.cbegin();
  for (size_t i = 0; i < buckets.size(); i++)
  {
    auto bucket = buckets[i];
    if (i != buckets.size() - 1)
    {
      ASSERT_DOUBLE_EQ(*boundary_it++, bucket.upper_bound);
    }
    else
    {
      ASSERT_DOUBLE_EQ(std::numeric_limits<double>::infinity(), bucket.upper_bound);
    }
    cumulative_count += correct[i];
    ASSERT_EQ(cumulative_count, bucket.cumulative_count);
  }
}

TEST(PrometheusExporterUtils, TranslateToPrometheusEmptyInputReturnsEmptyCollection)
{
  metric_sdk::ResourceMetrics metrics_data = {};
  auto translated = PrometheusExporterUtils::TranslateToPrometheus(metrics_data);
  ASSERT_EQ(translated.size(), 0);
}

TEST(PrometheusExporterUtils, TranslateToPrometheusIntegerCounter)
{

  opentelemetry::sdk::resource::Resource resource = opentelemetry::sdk::resource::Resource::Create(
      {{"service.name", "test_service"},
       {"service.namespace", "test_namespace"},
       {"service.instance.id", "localhost:8000"},
       {"custom_resource_attr", "custom_resource_value"}});
  TestDataPoints dp;
  metric_sdk::ResourceMetrics metrics_data = dp.CreateSumPointData();
  metrics_data.resource_                   = &resource;

  auto translated = PrometheusExporterUtils::TranslateToPrometheus(metrics_data);
  ASSERT_EQ(translated.size(), 2);
  auto metric1          = translated[1];
  std::vector<int> vals = {10};
  assert_basic(metric1, "library_name_unit_total", "description",
               prometheus_client::MetricType::Counter, 3, vals);
  int checked_label_num = 0;
  for (auto &label : translated[0].metric[0].label)
  {
    if (label.name == "service_namespace")
    {
      ASSERT_EQ(label.value, "test_namespace");
      checked_label_num++;
    }
    else if (label.name == "service_name")
    {
      ASSERT_EQ(label.value, "test_service");
      checked_label_num++;
    }
    else if (label.name == "service_instance_id")
    {
      ASSERT_EQ(label.value, "localhost:8000");
      checked_label_num++;
    }
    else if (label.name == "custom_resource_attr")
    {
      ASSERT_EQ(label.value, "custom_resource_value");
      checked_label_num++;
    }
  }
  ASSERT_EQ(checked_label_num, 4);
}

TEST(PrometheusExporterUtils, TranslateToPrometheusIntegerLastValue)
{
  opentelemetry::sdk::resource::Resource resource = opentelemetry::sdk::resource::Resource::Create(
      {{"service.name", "test_service"},
       {"service.instance.id", "localhost:8000"},
       {"custom_resource_attr", "custom_resource_value"}});
  TestDataPoints dp;
  metric_sdk::ResourceMetrics metrics_data = dp.CreateLastValuePointData();
  metrics_data.resource_                   = &resource;

  auto translated = PrometheusExporterUtils::TranslateToPrometheus(metrics_data);
  ASSERT_EQ(translated.size(), 2);

  auto metric1          = translated[1];
  std::vector<int> vals = {10};
  assert_basic(metric1, "library_name_unit", "description", prometheus_client::MetricType::Gauge, 3,
               vals);

  int checked_label_num = 0;
  for (auto &label : translated[0].metric[0].label)
  {
    if (label.name == "service_name")
    {
      ASSERT_EQ(label.value, "test_service");
      checked_label_num++;
    }
    else if (label.name == "service_instance_id")
    {
      ASSERT_EQ(label.value, "localhost:8000");
      checked_label_num++;
    }
    else if (label.name == "custom_resource_attr")
    {
      ASSERT_EQ(label.value, "custom_resource_value");
      checked_label_num++;
    }
  }
  ASSERT_EQ(checked_label_num, 3);
}

TEST(PrometheusExporterUtils, TranslateToPrometheusHistogramNormal)
{
  opentelemetry::sdk::resource::Resource resource = opentelemetry::sdk::resource::Resource::Create(
      {{"service.instance.id", "localhost:8001"},
       {"custom_resource_attr", "custom_resource_value"}});
  TestDataPoints dp;
  metric_sdk::ResourceMetrics metrics_data = dp.CreateHistogramPointData();
  metrics_data.resource_                   = &resource;

  auto translated = PrometheusExporterUtils::TranslateToPrometheus(metrics_data);
  ASSERT_EQ(translated.size(), 2);

  auto metric              = translated[1];
  std::vector<double> vals = {3, 900.5, 4};
  assert_basic(metric, "library_name_unit", "description", prometheus_client::MetricType::Histogram,
               3, vals);
  assert_histogram(metric, std::list<double>{10.1, 20.2, 30.2}, {200, 300, 400, 500});

  int checked_label_num = 0;
  for (auto &label : translated[0].metric[0].label)
  {
    if (label.name == "service_name")
    {
      // default service name is "unknown_service"
      ASSERT_EQ(label.value, "unknown_service");
      checked_label_num++;
    }
    else if (label.name == "service_instance_id")
    {
      ASSERT_EQ(label.value, "localhost:8001");
      checked_label_num++;
    }
    else if (label.name == "custom_resource_attr")
    {
      ASSERT_EQ(label.value, "custom_resource_value");
      checked_label_num++;
    }
  }
  ASSERT_EQ(checked_label_num, 3);
}

class SanitizeTest : public ::testing::Test
{
  Resource resource_ = Resource::Create({});
  nostd::unique_ptr<InstrumentationScope> instrumentation_scope_ =
      InstrumentationScope::Create("library_name", "1.2.0");

protected:
  void CheckSanitizeLabel(const std::string &original, const std::string &sanitized)
  {
    metric_sdk::InstrumentDescriptor instrument_descriptor{
        "name", "description", "unit", metric_sdk::InstrumentType::kCounter,
        metric_sdk::InstrumentValueType::kDouble};
    auto result = PrometheusExporterUtils::TranslateToPrometheus(
        {&resource_,
         std::vector<metric_sdk::ScopeMetrics>{
             {instrumentation_scope_.get(),
              std::vector<metric_sdk::MetricData>{
                  {instrument_descriptor, {}, {}, {}, {{{{original, "value"}}, {}}}}}}}},
        false);
    EXPECT_EQ(result.begin()->metric.begin()->label.begin()->name, sanitized);
  }
};

TEST_F(SanitizeTest, Label)
{
  CheckSanitizeLabel("name", "name");
  CheckSanitizeLabel("name?", "name_");
  CheckSanitizeLabel("name???", "name_");
  CheckSanitizeLabel("name?__", "name_");
  CheckSanitizeLabel("name?__name", "name_name");
  CheckSanitizeLabel("name?__name:", "name_name_");
}

TEST_F(SanitizeTest, Name) {}

class AttributeCollisionTest : public ::testing::Test
{
  Resource resource_ = Resource::Create(ResourceAttributes{});
  nostd::unique_ptr<InstrumentationScope> instrumentation_scope_ =
      InstrumentationScope::Create("library_name", "1.2.0");
  metric_sdk::InstrumentDescriptor instrument_descriptor_{"library_name", "description", "unit",
                                                          metric_sdk::InstrumentType::kCounter,
                                                          metric_sdk::InstrumentValueType::kDouble};

protected:
  void CheckTranslation(const metric_sdk::PointAttributes &attrs,
                        const std::vector<prometheus::ClientMetric::Label> &expected)
  {
    std::vector<prometheus::MetricFamily> result = PrometheusExporterUtils::TranslateToPrometheus(
        {&resource_,
         std::vector<metric_sdk::ScopeMetrics>{
             {instrumentation_scope_.get(),
              std::vector<metric_sdk::MetricData>{
                  {instrument_descriptor_, {}, {}, {}, {{attrs, {}}}}}}}},
        false);
    for (auto &expected_kv : expected)
    {
      bool found = false;
      for (auto &found_kv : result.begin()->metric.begin()->label)
      {
        if (found_kv.name == expected_kv.name)
        {
          EXPECT_EQ(found_kv.value, expected_kv.value);
          found = true;
        }
      }
      EXPECT_TRUE(found);
    }
  }
};

TEST_F(AttributeCollisionTest, SeparatesDistinctKeys)
{
  CheckTranslation({{"foo.a", "value1"}, {"foo.b", "value2"}}, {{"foo_a", "value1"},
                                                                {"foo_b", "value2"},
                                                                {"otel_scope_name", "library_name"},
                                                                {"otel_scope_version", "1.2.0"}});
}

TEST(PrometheusExporterUtils, PrometheusUnit)
{
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::getPrometheusUnit("d"), "days");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::getPrometheusUnit("h"), "hours");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::getPrometheusUnit("min"), "minutes");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::getPrometheusUnit("s"), "seconds");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::getPrometheusUnit("ms"), "milliseconds");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::getPrometheusUnit("us"), "microseconds");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::getPrometheusUnit("ns"), "nanoseconds");

  ASSERT_EQ(exporter::metrics::SanitizeNameTester::getPrometheusUnit("By"), "bytes");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::getPrometheusUnit("KiBy"), "kibibytes");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::getPrometheusUnit("MiBy"), "mebibytes");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::getPrometheusUnit("GiBy"), "gibibytes");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::getPrometheusUnit("TiBy"), "tibibytes");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::getPrometheusUnit("KBy"), "kilobytes");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::getPrometheusUnit("MBy"), "megabytes");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::getPrometheusUnit("GBy"), "gigabytes");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::getPrometheusUnit("TBy"), "terabytes");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::getPrometheusUnit("By"), "bytes");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::getPrometheusUnit("KBy"), "kilobytes");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::getPrometheusUnit("MBy"), "megabytes");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::getPrometheusUnit("GBy"), "gigabytes");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::getPrometheusUnit("TBy"), "terabytes");

  ASSERT_EQ(exporter::metrics::SanitizeNameTester::getPrometheusUnit("m"), "meters");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::getPrometheusUnit("V"), "volts");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::getPrometheusUnit("A"), "amperes");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::getPrometheusUnit("J"), "joules");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::getPrometheusUnit("W"), "watts");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::getPrometheusUnit("g"), "grams");

  ASSERT_EQ(exporter::metrics::SanitizeNameTester::getPrometheusUnit("Cel"), "celsius");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::getPrometheusUnit("Hz"), "hertz");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::getPrometheusUnit("1"), "");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::getPrometheusUnit("%"), "percent");
}

TEST(PrometheusExporterUtils, PrometheusPerUnit)
{
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::getPrometheusPerUnit("s"), "second");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::getPrometheusPerUnit("m"), "minute");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::getPrometheusPerUnit("h"), "hour");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::getPrometheusPerUnit("d"), "day");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::getPrometheusPerUnit("w"), "week");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::getPrometheusPerUnit("mo"), "month");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::getPrometheusPerUnit("y"), "year");
}

TEST(PrometheusExporterUtils, RemoveUnitPortionInBraces)
{
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::removeUnitPortionInBraces("{unit}"), "");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::removeUnitPortionInBraces("unit{unit}"), "unit");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::removeUnitPortionInBraces("unit_{unit}"),
            "unit_");
}

TEST(PrometheusExporterUtils, ConvertRateExpressedToPrometheusUnit)
{
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::convertRateExpressedToPrometheusUnit("unit/d"),
            "unit_per_day");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::convertRateExpressedToPrometheusUnit("unit/s"),
            "unit_per_second");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::convertRateExpressedToPrometheusUnit("unit/"),
            "unit/");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::convertRateExpressedToPrometheusUnit("unit"),
            "unit");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::convertRateExpressedToPrometheusUnit("unit/m"),
            "unit_per_minute");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::convertRateExpressedToPrometheusUnit("/m"),
            "_per_minute");
}

TEST(PromentheusExporterUtils, PrometheusNameMapping)
{
  // General test cases on unit expansions and name sanitization
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::mapToPrometheusName(
                "sample_metric___name", "g", prometheus::MetricType::Counter),
            "sample_metric_name_grams_total");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::mapToPrometheusName(
                "sample_metric_name", "s", prometheus::MetricType::Counter),
            "sample_metric_name_seconds_total");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::mapToPrometheusName(
                "sample_metric_name", "s", prometheus::MetricType::Gauge),
            "sample_metric_name_seconds");
  // Test without_units & without_type_suffix with Counters and unit = 1
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::mapToPrometheusName(
                "sample_metric_name", "1", prometheus::MetricType::Counter),
            "sample_metric_name_total");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::mapToPrometheusName(
                "sample_metric_name", "1", prometheus::MetricType::Counter, true, false),
            "sample_metric_name_total");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::mapToPrometheusName(
                "sample_metric_name", "1", prometheus::MetricType::Counter, false, true),
            "sample_metric_name");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::mapToPrometheusName(
                "sample_metric_name", "1", prometheus::MetricType::Counter, true, true),
            "sample_metric_name");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::mapToPrometheusName(
                "sample_metric_name", "1", prometheus::MetricType::Counter, true, true),
            "sample_metric_name");
  // Test without_units & without_type_suffix with Counters and non-special units
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::mapToPrometheusName(
                "sample_metric_name", "%", prometheus::MetricType::Counter),
            "sample_metric_name_percent_total");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::mapToPrometheusName(
                "sample_metric_name", "m", prometheus::MetricType::Counter, true, false),
            "sample_metric_name_total");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::mapToPrometheusName(
                "sample_metric_name", "By", prometheus::MetricType::Counter, false, true),
            "sample_metric_name_bytes");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::mapToPrometheusName(
                "sample_metric_name", "s", prometheus::MetricType::Counter, true, true),
            "sample_metric_name");
  // Special case Gauges & ratio
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::mapToPrometheusName(
                "sample_metric_name", "1", prometheus::MetricType::Gauge),
            "sample_metric_name_ratio");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::mapToPrometheusName(
                "sample_metric_name", "1", prometheus::MetricType::Gauge, false, true),
            "sample_metric_name_ratio");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::mapToPrometheusName(
                "sample_metric_name", "1", prometheus::MetricType::Gauge, true, false),
            "sample_metric_name");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::mapToPrometheusName(
                "sample_metric_name", "1", prometheus::MetricType::Gauge, true, true),
            "sample_metric_name");
  // Test without_type_suffix affects only counters
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::mapToPrometheusName(
                "sample_metric_name", "Hz", prometheus::MetricType::Counter),
            "sample_metric_name_hertz_total");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::mapToPrometheusName(
                "sample_metric_name", "Hz", prometheus::MetricType::Counter, false, true),
            "sample_metric_name_hertz");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::mapToPrometheusName(
                "sample_metric_name", "Hz", prometheus::MetricType::Gauge),
            "sample_metric_name_hertz");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::mapToPrometheusName(
                "sample_metric_name", "Hz", prometheus::MetricType::Gauge, false, true),
            "sample_metric_name_hertz");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::mapToPrometheusName(
                "sample_metric_name", "Hz", prometheus::MetricType::Histogram),
            "sample_metric_name_hertz");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::mapToPrometheusName(
                "sample_metric_name", "Hz", prometheus::MetricType::Histogram, false, true),
            "sample_metric_name_hertz");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::mapToPrometheusName(
                "sample_metric_name", "Hz", prometheus::MetricType::Summary),
            "sample_metric_name_hertz");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::mapToPrometheusName(
                "sample_metric_name", "Hz", prometheus::MetricType::Summary, false, true),
            "sample_metric_name_hertz");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::mapToPrometheusName(
                "sample_metric_name", "Hz", prometheus::MetricType::Info),
            "sample_metric_name_hertz");
  ASSERT_EQ(exporter::metrics::SanitizeNameTester::mapToPrometheusName(
                "sample_metric_name", "Hz", prometheus::MetricType::Info, false, true),
            "sample_metric_name_hertz");
}

TEST_F(AttributeCollisionTest, JoinsCollidingKeys)
{
  CheckTranslation({{"foo.a", "value1"}, {"foo_a", "value2"}}, {{"foo_a", "value1;value2"},
                                                                {"otel_scope_name", "library_name"},
                                                                {"otel_scope_version", "1.2.0"}});
}

TEST_F(AttributeCollisionTest, DropsInvertedKeys)
{
  CheckTranslation({{"foo.a", "value1"}, {"foo.b", "value2"}, {"foo__a", "value3"}},
                   {{"foo_a", "value1"},
                    {"foo_b", "value2"},
                    {"otel_scope_name", "library_name"},
                    {"otel_scope_version", "1.2.0"}});
}

OPENTELEMETRY_END_NAMESPACE
