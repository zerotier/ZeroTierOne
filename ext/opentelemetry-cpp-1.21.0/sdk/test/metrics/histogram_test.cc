// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <string>
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
#include "opentelemetry/sdk/metrics/view/instrument_selector.h"
#include "opentelemetry/sdk/metrics/view/meter_selector.h"
#include "opentelemetry/sdk/metrics/view/view.h"

using namespace opentelemetry;
using namespace opentelemetry::sdk::instrumentationscope;
using namespace opentelemetry::sdk::metrics;

TEST(Histogram, Double)
{
  MeterProvider mp;
  auto m = mp.GetMeter("meter1", "version1", "schema1");

  std::unique_ptr<MockMetricExporter> exporter(new MockMetricExporter());
  std::shared_ptr<MetricReader> reader{new MockMetricReader(std::move(exporter))};
  mp.AddMetricReader(reader);

  auto h = m->CreateDoubleHistogram("histogram1", "histogram1_description", "histogram1_unit");

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

  std::vector<HistogramPointData> actuals;
  reader->Collect([&](ResourceMetrics &rm) {
    for (const ScopeMetrics &smd : rm.scope_metric_data_)
    {
      for (const MetricData &md : smd.metric_data_)
      {
        for (const PointDataAttributes &dp : md.point_data_attr_)
        {
          actuals.push_back(opentelemetry::nostd::get<HistogramPointData>(dp.point_data));
        }
      }
    }
    return true;
  });

  ASSERT_EQ(1, actuals.size());

  const auto &actual = actuals.at(0);
  ASSERT_EQ(1000275.0, opentelemetry::nostd::get<double>(actual.sum_));
  ASSERT_EQ(11, actual.count_);
  ASSERT_EQ(5.0, opentelemetry::nostd::get<double>(actual.min_));
  ASSERT_EQ(1e6, opentelemetry::nostd::get<double>(actual.max_));
  ASSERT_EQ(std::vector<double>(
                {0, 5, 10, 25, 50, 75, 100, 250, 500, 750, 1000, 2500, 5000, 7500, 10000}),
            actual.boundaries_);
  ASSERT_EQ(std::vector<uint64_t>({0, 1, 1, 3, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}),
            actual.counts_);
}

#if OPENTELEMETRY_HAVE_WORKING_REGEX
// FIXME - View Preficate search is only supported through regex
TEST(Histogram, DoubleCustomBuckets)
{
  MeterProvider mp;
  auto m                      = mp.GetMeter("meter1", "version1", "schema1");
  std::string instrument_unit = "ms";
  std::string instrument_name = "historgram1";
  std::string instrument_desc = "histogram metrics";

  std::unique_ptr<MockMetricExporter> exporter(new MockMetricExporter());
  std::shared_ptr<MetricReader> reader{new MockMetricReader(std::move(exporter))};
  mp.AddMetricReader(reader);

  std::shared_ptr<HistogramAggregationConfig> config(new HistogramAggregationConfig());
  config->boundaries_ = {10, 20, 30, 40};
  std::unique_ptr<View> view{
      new View("view1", "view1_description", instrument_unit, AggregationType::kHistogram, config)};
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

  std::vector<HistogramPointData> actuals;
  reader->Collect([&](ResourceMetrics &rm) {
    for (const ScopeMetrics &smd : rm.scope_metric_data_)
    {
      for (const MetricData &md : smd.metric_data_)
      {
        for (const PointDataAttributes &dp : md.point_data_attr_)
        {
          actuals.push_back(opentelemetry::nostd::get<HistogramPointData>(dp.point_data));
        }
      }
    }
    return true;
  });

  ASSERT_EQ(1, actuals.size());

  const auto &actual = actuals.at(0);
  ASSERT_EQ(275.0, opentelemetry::nostd::get<double>(actual.sum_));
  ASSERT_EQ(10, actual.count_);
  ASSERT_EQ(5.0, opentelemetry::nostd::get<double>(actual.min_));
  ASSERT_EQ(50.0, opentelemetry::nostd::get<double>(actual.max_));
  ASSERT_EQ(std::vector<double>({10, 20, 30, 40}), actual.boundaries_);
  ASSERT_EQ(std::vector<uint64_t>({2, 2, 2, 2, 2}), actual.counts_);
}

TEST(Histogram, DoubleEmptyBuckets)
{
  MeterProvider mp;
  auto m                      = mp.GetMeter("meter1", "version1", "schema1");
  std::string instrument_unit = "ms";
  std::string instrument_name = "historgram1";
  std::string instrument_desc = "histogram metrics";

  std::unique_ptr<MockMetricExporter> exporter(new MockMetricExporter());
  std::shared_ptr<MetricReader> reader{new MockMetricReader(std::move(exporter))};
  mp.AddMetricReader(reader);

  std::shared_ptr<HistogramAggregationConfig> config(new HistogramAggregationConfig());
  config->boundaries_ = {};
  std::unique_ptr<View> view{
      new View("view1", "view1_description", instrument_unit, AggregationType::kHistogram, config)};
  std::unique_ptr<InstrumentSelector> instrument_selector{
      new InstrumentSelector(InstrumentType::kHistogram, instrument_name, instrument_unit)};
  std::unique_ptr<MeterSelector> meter_selector{new MeterSelector("meter1", "version1", "schema1")};
  mp.AddView(std::move(instrument_selector), std::move(meter_selector), std::move(view));

  auto h = m->CreateDoubleHistogram(instrument_name, instrument_desc, instrument_unit);

  h->Record(-5, {});
  h->Record(10, {});
  h->Record(15, {});
  h->Record(20, {});
  h->Record(25, {});
  h->Record(30, {});
  h->Record(35, {});
  h->Record(40, {});
  h->Record(45, {});
  h->Record(50, {});

  std::vector<HistogramPointData> actuals;
  reader->Collect([&](ResourceMetrics &rm) {
    for (const ScopeMetrics &smd : rm.scope_metric_data_)
    {
      for (const MetricData &md : smd.metric_data_)
      {
        for (const PointDataAttributes &dp : md.point_data_attr_)
        {
          actuals.push_back(opentelemetry::nostd::get<HistogramPointData>(dp.point_data));
        }
      }
    }
    return true;
  });

  ASSERT_EQ(1, actuals.size());

  const auto &actual = actuals.at(0);
  ASSERT_EQ(270.0, opentelemetry::nostd::get<double>(actual.sum_));
  ASSERT_EQ(9, actual.count_);
  ASSERT_EQ(10.0, opentelemetry::nostd::get<double>(actual.min_));
  ASSERT_EQ(50.0, opentelemetry::nostd::get<double>(actual.max_));
  ASSERT_EQ(std::vector<double>({}), actual.boundaries_);
  ASSERT_EQ(std::vector<uint64_t>({9}), actual.counts_);
}
#endif

TEST(Histogram, UInt64)
{
  MeterProvider mp;
  auto m = mp.GetMeter("meter1", "version1", "schema1");

  std::unique_ptr<MockMetricExporter> exporter(new MockMetricExporter());
  std::shared_ptr<MetricReader> reader{new MockMetricReader(std::move(exporter))};
  mp.AddMetricReader(reader);

  auto h = m->CreateUInt64Histogram("histogram1", "histogram1_description", "histogram1_unit");

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
  h->Record(1000000, {});

  std::vector<HistogramPointData> actuals;
  reader->Collect([&](ResourceMetrics &rm) {
    for (const ScopeMetrics &smd : rm.scope_metric_data_)
    {
      for (const MetricData &md : smd.metric_data_)
      {
        for (const PointDataAttributes &dp : md.point_data_attr_)
        {
          actuals.push_back(opentelemetry::nostd::get<HistogramPointData>(dp.point_data));
        }
      }
    }
    return true;
  });

  ASSERT_EQ(1, actuals.size());

  const auto &actual = actuals.at(0);
  ASSERT_EQ(1000275, opentelemetry::nostd::get<int64_t>(actual.sum_));
  ASSERT_EQ(11, actual.count_);
  ASSERT_EQ(5, opentelemetry::nostd::get<int64_t>(actual.min_));
  ASSERT_EQ(1000000, opentelemetry::nostd::get<int64_t>(actual.max_));
  ASSERT_EQ(std::vector<double>(
                {0, 5, 10, 25, 50, 75, 100, 250, 500, 750, 1000, 2500, 5000, 7500, 10000}),
            actual.boundaries_);
  ASSERT_EQ(std::vector<uint64_t>({0, 1, 1, 3, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}),
            actual.counts_);
}

#if OPENTELEMETRY_HAVE_WORKING_REGEX
// FIXME - View Preficate search is only supported through regex
TEST(Histogram, UInt64CustomBuckets)
{
  MeterProvider mp;
  auto m                      = mp.GetMeter("meter1", "version1", "schema1");
  std::string instrument_name = "historgram1";
  std::string instrument_desc = "histogram metrics";
  std::string instrument_unit = "ms";

  std::unique_ptr<MockMetricExporter> exporter(new MockMetricExporter());
  std::shared_ptr<MetricReader> reader{new MockMetricReader(std::move(exporter))};
  mp.AddMetricReader(reader);

  std::shared_ptr<HistogramAggregationConfig> config(new HistogramAggregationConfig());
  config->boundaries_ = {10, 20, 30, 40};
  std::unique_ptr<View> view{
      new View("view1", "view1_description", "ms", AggregationType::kHistogram, config)};
  std::unique_ptr<InstrumentSelector> instrument_selector{
      new InstrumentSelector(InstrumentType::kHistogram, instrument_name, instrument_unit)};
  std::unique_ptr<MeterSelector> meter_selector{new MeterSelector("meter1", "version1", "schema1")};
  mp.AddView(std::move(instrument_selector), std::move(meter_selector), std::move(view));

  auto h = m->CreateUInt64Histogram(instrument_name, instrument_desc, instrument_unit);

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

  std::vector<HistogramPointData> actuals;
  reader->Collect([&](ResourceMetrics &rm) {
    for (const ScopeMetrics &smd : rm.scope_metric_data_)
    {
      for (const MetricData &md : smd.metric_data_)
      {
        for (const PointDataAttributes &dp : md.point_data_attr_)
        {
          actuals.push_back(opentelemetry::nostd::get<HistogramPointData>(dp.point_data));
        }
      }
    }
    return true;
  });

  ASSERT_EQ(1, actuals.size());

  const auto &actual = actuals.at(0);
  ASSERT_EQ(275, opentelemetry::nostd::get<int64_t>(actual.sum_));
  ASSERT_EQ(10, actual.count_);
  ASSERT_EQ(5, opentelemetry::nostd::get<int64_t>(actual.min_));
  ASSERT_EQ(50, opentelemetry::nostd::get<int64_t>(actual.max_));
  ASSERT_EQ(std::vector<double>({10, 20, 30, 40}), actual.boundaries_);
  ASSERT_EQ(std::vector<uint64_t>({2, 2, 2, 2, 2}), actual.counts_);
}

TEST(Histogram, UInt64EmptyBuckets)
{
  MeterProvider mp;
  auto m                      = mp.GetMeter("meter1", "version1", "schema1");
  std::string instrument_name = "historgram1";
  std::string instrument_desc = "histogram metrics";
  std::string instrument_unit = "ms";

  std::unique_ptr<MockMetricExporter> exporter(new MockMetricExporter());
  std::shared_ptr<MetricReader> reader{new MockMetricReader(std::move(exporter))};
  mp.AddMetricReader(reader);

  std::shared_ptr<HistogramAggregationConfig> config(new HistogramAggregationConfig());
  config->boundaries_ = {};
  std::unique_ptr<View> view{
      new View("view1", "view1_description", "ms", AggregationType::kHistogram, config)};
  std::unique_ptr<InstrumentSelector> instrument_selector{
      new InstrumentSelector(InstrumentType::kHistogram, instrument_name, instrument_unit)};
  std::unique_ptr<MeterSelector> meter_selector{new MeterSelector("meter1", "version1", "schema1")};
  mp.AddView(std::move(instrument_selector), std::move(meter_selector), std::move(view));

  auto h = m->CreateUInt64Histogram(instrument_name, instrument_desc, instrument_unit);

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

  std::vector<HistogramPointData> actuals;
  reader->Collect([&](ResourceMetrics &rm) {
    for (const ScopeMetrics &smd : rm.scope_metric_data_)
    {
      for (const MetricData &md : smd.metric_data_)
      {
        for (const PointDataAttributes &dp : md.point_data_attr_)
        {
          actuals.push_back(opentelemetry::nostd::get<HistogramPointData>(dp.point_data));
        }
      }
    }
    return true;
  });

  ASSERT_EQ(1, actuals.size());

  const auto &actual = actuals.at(0);
  ASSERT_EQ(275, opentelemetry::nostd::get<int64_t>(actual.sum_));
  ASSERT_EQ(10, actual.count_);
  ASSERT_EQ(5, opentelemetry::nostd::get<int64_t>(actual.min_));
  ASSERT_EQ(50, opentelemetry::nostd::get<int64_t>(actual.max_));
  ASSERT_EQ(std::vector<double>({}), actual.boundaries_);
  ASSERT_EQ(std::vector<uint64_t>({10}), actual.counts_);
}
#endif
