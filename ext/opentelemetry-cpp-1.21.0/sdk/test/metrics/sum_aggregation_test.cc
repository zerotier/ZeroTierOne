// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <algorithm>
#include <initializer_list>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "common.h"

#include "opentelemetry/common/macros.h"
#include "opentelemetry/context/context.h"
#include "opentelemetry/metrics/meter.h"
#include "opentelemetry/metrics/sync_instruments.h"
#include "opentelemetry/nostd/function_ref.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/variant.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/metrics/aggregation/aggregation_config.h"
#include "opentelemetry/sdk/metrics/data/metric_data.h"
#include "opentelemetry/sdk/metrics/data/point_data.h"
#include "opentelemetry/sdk/metrics/export/metric_producer.h"
#include "opentelemetry/sdk/metrics/instruments.h"
#include "opentelemetry/sdk/metrics/meter_provider.h"
#include "opentelemetry/sdk/metrics/metric_reader.h"
#include "opentelemetry/sdk/metrics/push_metric_exporter.h"
#include "opentelemetry/sdk/metrics/view/attributes_processor.h"
#include "opentelemetry/sdk/metrics/view/instrument_selector.h"
#include "opentelemetry/sdk/metrics/view/meter_selector.h"
#include "opentelemetry/sdk/metrics/view/view.h"

#if OPENTELEMETRY_HAVE_WORKING_REGEX

using namespace opentelemetry;
using namespace opentelemetry::sdk::instrumentationscope;
using namespace opentelemetry::sdk::metrics;

TEST(HistogramToSum, Double)
{
  MeterProvider mp;
  auto m                      = mp.GetMeter("meter1", "version1", "schema1");
  std::string instrument_unit = "ms";
  std::string instrument_name = "historgram1";
  std::string instrument_desc = "histogram metrics";

  std::unique_ptr<MockMetricExporter> exporter(new MockMetricExporter());
  std::shared_ptr<MetricReader> reader{new MockMetricReader(std::move(exporter))};
  mp.AddMetricReader(reader);

  std::unique_ptr<View> view{
      new View("view1", "view1_description", instrument_unit, AggregationType::kSum)};
  std::unique_ptr<InstrumentSelector> instrument_selector{
      new InstrumentSelector(InstrumentType::kHistogram, instrument_name, instrument_unit)};
  std::unique_ptr<MeterSelector> meter_selector{new MeterSelector("meter1", "version1", "schema1")};
  mp.AddView(std::move(instrument_selector), std::move(meter_selector), std::move(view));

  auto h = m->CreateDoubleHistogram(instrument_name, instrument_desc, instrument_unit);

  h->Record(5, {});
  h->Record(10, {});
  h->Record(15, {});
  h->Record(20, {});
  h->Record(25, {});
  h->Record(30, {});
  h->Record(35, {});
  h->Record(40, {});
  h->Record(45, {});
  h->Record(50, {});
  h->Record(1e6, {});

  std::vector<SumPointData> actuals;
  reader->Collect([&](ResourceMetrics &rm) {
    for (const ScopeMetrics &smd : rm.scope_metric_data_)
    {
      for (const MetricData &md : smd.metric_data_)
      {
        for (const PointDataAttributes &dp : md.point_data_attr_)
        {
          actuals.push_back(opentelemetry::nostd::get<SumPointData>(dp.point_data));
        }
      }
    }
    return true;
  });

  ASSERT_EQ(1, actuals.size());
  const auto &actual = actuals.at(0);
  ASSERT_EQ(1000275.0, opentelemetry::nostd::get<double>(actual.value_));
}

TEST(HistogramToSumFilterAttributes, Double)
{
  MeterProvider mp;
  auto m                      = mp.GetMeter("meter1", "version1", "schema1");
  std::string instrument_unit = "ms";
  std::string instrument_name = "historgram1";
  std::string instrument_desc = "histogram metrics";

  std::unordered_map<std::string, bool> allowedattr;
  allowedattr["attr1"] = true;
  std::unique_ptr<opentelemetry::sdk::metrics::AttributesProcessor> attrproc{
      new opentelemetry::sdk::metrics::FilteringAttributesProcessor(allowedattr)};

  std::shared_ptr<opentelemetry::sdk::metrics::AggregationConfig> dummy_aggregation_config{
      new opentelemetry::sdk::metrics::AggregationConfig};
  std::unique_ptr<MockMetricExporter> exporter(new MockMetricExporter());
  std::shared_ptr<MetricReader> reader{new MockMetricReader(std::move(exporter))};
  mp.AddMetricReader(reader);

  std::unique_ptr<View> view{new View("view1", "view1_description", instrument_unit,
                                      AggregationType::kSum, dummy_aggregation_config,
                                      std::move(attrproc))};
  std::unique_ptr<InstrumentSelector> instrument_selector{
      new InstrumentSelector(InstrumentType::kHistogram, instrument_name, instrument_unit)};
  std::unique_ptr<MeterSelector> meter_selector{new MeterSelector("meter1", "version1", "schema1")};
  mp.AddView(std::move(instrument_selector), std::move(meter_selector), std::move(view));

  auto h = m->CreateDoubleHistogram(instrument_name, instrument_desc, instrument_unit);
  std::unordered_map<std::string, std::string> attr1 = {{"attr1", "val1"}, {"attr2", "val2"}};
  std::unordered_map<std::string, std::string> attr2 = {{"attr1", "val1"}, {"attr2", "val2"}};
  h->Record(5, attr1, opentelemetry::context::Context{});
  h->Record(10, attr2, opentelemetry::context::Context{});

  reader->Collect([&](ResourceMetrics &rm) {
    for (const ScopeMetrics &smd : rm.scope_metric_data_)
    {
      for (const MetricData &md : smd.metric_data_)
      {
        EXPECT_EQ(1, md.point_data_attr_.size());
        if (md.point_data_attr_.size() == 1)
        {
          EXPECT_EQ(15.0, opentelemetry::nostd::get<double>(opentelemetry::nostd::get<SumPointData>(
                                                                md.point_data_attr_[0].point_data)
                                                                .value_));
          EXPECT_EQ(1, md.point_data_attr_[0].attributes.size());
          EXPECT_NE(md.point_data_attr_[0].attributes.end(),
                    md.point_data_attr_[0].attributes.find("attr1"));
        }
      }
    }
    return true;
  });
}

TEST(CounterToSum, Double)
{
  MeterProvider mp;
  auto m = mp.GetMeter("meter1", "version1", "schema1");

  std::unique_ptr<MockMetricExporter> exporter(new MockMetricExporter());
  std::shared_ptr<MetricReader> reader{new MockMetricReader(std::move(exporter))};
  mp.AddMetricReader(reader);

  std::unique_ptr<View> view{new View("view1", "view1_description", "ms", AggregationType::kSum)};
  std::unique_ptr<InstrumentSelector> instrument_selector{
      new InstrumentSelector(InstrumentType::kCounter, "counter1", "ms")};
  std::unique_ptr<MeterSelector> meter_selector{new MeterSelector("meter1", "version1", "schema1")};
  mp.AddView(std::move(instrument_selector), std::move(meter_selector), std::move(view));

  auto h = m->CreateDoubleCounter("counter1", "counter1_description", "counter1_unit");

  h->Add(5, {});
  h->Add(10, {});
  h->Add(15, {});
  h->Add(20, {});
  h->Add(25, {});
  h->Add(30, {});
  h->Add(35, {});
  h->Add(40, {});
  h->Add(45, {});
  h->Add(50, {});
  h->Add(1e6, {});

  std::vector<SumPointData> actuals;
  reader->Collect([&](ResourceMetrics &rm) {
    for (const ScopeMetrics &smd : rm.scope_metric_data_)
    {
      for (const MetricData &md : smd.metric_data_)
      {
        for (const PointDataAttributes &dp : md.point_data_attr_)
        {
          actuals.push_back(opentelemetry::nostd::get<SumPointData>(dp.point_data));
        }
      }
    }
    return true;
  });

  ASSERT_EQ(1, actuals.size());
  const auto &actual = actuals.at(0);
  ASSERT_EQ(1000275.0, opentelemetry::nostd::get<double>(actual.value_));
}

TEST(CounterToSumFilterAttributes, Double)
{
  MeterProvider mp;
  auto m                      = mp.GetMeter("meter1", "version1", "schema1");
  std::string instrument_unit = "ms";
  std::string instrument_name = "counter1";
  std::string instrument_desc = "counter metrics";

  std::unordered_map<std::string, bool> allowedattr;
  allowedattr["attr1"] = true;
  std::unique_ptr<opentelemetry::sdk::metrics::AttributesProcessor> attrproc{
      new opentelemetry::sdk::metrics::FilteringAttributesProcessor(allowedattr)};

  std::shared_ptr<opentelemetry::sdk::metrics::AggregationConfig> dummy_aggregation_config{
      new opentelemetry::sdk::metrics::AggregationConfig};
  std::unique_ptr<MockMetricExporter> exporter(new MockMetricExporter());
  std::shared_ptr<MetricReader> reader{new MockMetricReader(std::move(exporter))};
  mp.AddMetricReader(reader);

  std::unique_ptr<View> view{new View("view1", "view1_description", instrument_unit,
                                      AggregationType::kSum, dummy_aggregation_config,
                                      std::move(attrproc))};
  std::unique_ptr<InstrumentSelector> instrument_selector{
      new InstrumentSelector(InstrumentType::kCounter, instrument_name, instrument_unit)};
  std::unique_ptr<MeterSelector> meter_selector{new MeterSelector("meter1", "version1", "schema1")};
  mp.AddView(std::move(instrument_selector), std::move(meter_selector), std::move(view));

  auto c = m->CreateDoubleCounter(instrument_name, instrument_desc, instrument_unit);
  std::unordered_map<std::string, std::string> attr1 = {{"attr1", "val1"}, {"attr2", "val2"}};
  std::unordered_map<std::string, std::string> attr2 = {{"attr1", "val1"}, {"attr2", "val2"}};
  c->Add(5, attr1, opentelemetry::context::Context{});
  c->Add(10, attr2, opentelemetry::context::Context{});

  reader->Collect([&](ResourceMetrics &rm) {
    for (const ScopeMetrics &smd : rm.scope_metric_data_)
    {
      for (const MetricData &md : smd.metric_data_)
      {
        EXPECT_EQ(1, md.point_data_attr_.size());
        if (md.point_data_attr_.size() == 1)
        {
          EXPECT_EQ(15.0, opentelemetry::nostd::get<double>(opentelemetry::nostd::get<SumPointData>(
                                                                md.point_data_attr_[0].point_data)
                                                                .value_));
          EXPECT_EQ(1, md.point_data_attr_[0].attributes.size());
          EXPECT_NE(md.point_data_attr_[0].attributes.end(),
                    md.point_data_attr_[0].attributes.find("attr1"));
        }
      }
    }
    return true;
  });
}

class UpDownCounterToSumFixture : public ::testing::TestWithParam<bool>
{};

TEST_P(UpDownCounterToSumFixture, Double)
{
  bool is_matching_view = GetParam();
  MeterProvider mp;
  auto m                      = mp.GetMeter("meter1", "version1", "schema1");
  std::string instrument_name = "updowncounter1";
  std::string instrument_desc = "updowncounter desc";
  std::string instrument_unit = "ms";

  std::unique_ptr<MockMetricExporter> exporter(new MockMetricExporter());
  std::shared_ptr<MetricReader> reader{new MockMetricReader(std::move(exporter))};
  mp.AddMetricReader(reader);

  if (is_matching_view)
  {
    std::unique_ptr<View> view{
        new View("view1", "view1_description", instrument_unit, AggregationType::kSum)};
    std::unique_ptr<InstrumentSelector> instrument_selector{
        new InstrumentSelector(InstrumentType::kUpDownCounter, instrument_name, instrument_unit)};
    std::unique_ptr<MeterSelector> meter_selector{
        new MeterSelector("meter1", "version1", "schema1")};
    mp.AddView(std::move(instrument_selector), std::move(meter_selector), std::move(view));
  }
  auto h = m->CreateDoubleUpDownCounter(instrument_name, instrument_desc, instrument_unit);

  h->Add(5, {});
  h->Add(10, {});
  h->Add(-15, {});
  h->Add(20, {});
  h->Add(25, {});
  h->Add(-30, {});

  std::vector<SumPointData> actuals;
  reader->Collect([&](ResourceMetrics &rm) {
    for (const ScopeMetrics &smd : rm.scope_metric_data_)
    {
      for (const MetricData &md : smd.metric_data_)
      {
        for (const PointDataAttributes &dp : md.point_data_attr_)
        {
          actuals.push_back(opentelemetry::nostd::get<SumPointData>(dp.point_data));
        }
      }
    }
    return true;
  });

  ASSERT_EQ(1, actuals.size());
  const auto &actual = actuals.at(0);
  ASSERT_EQ(15.0, opentelemetry::nostd::get<double>(actual.value_));
}
INSTANTIATE_TEST_SUITE_P(UpDownCounterToSum,
                         UpDownCounterToSumFixture,
                         ::testing::Values(true, false));
#endif
