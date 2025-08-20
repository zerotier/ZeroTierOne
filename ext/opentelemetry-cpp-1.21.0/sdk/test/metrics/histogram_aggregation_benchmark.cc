// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <benchmark/benchmark.h>
#include <stddef.h>
#include <algorithm>
#include <chrono>
#include <functional>
#include <random>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include "common.h"
#include "opentelemetry/context/context.h"
#include "opentelemetry/metrics/meter.h"
#include "opentelemetry/metrics/sync_instruments.h"
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
#include "opentelemetry/sdk/metrics/view/view_registry.h"

using namespace opentelemetry;
using namespace opentelemetry::sdk::instrumentationscope;
using namespace opentelemetry::sdk::metrics;

namespace
{

template <class T>
void HistogramAggregation(benchmark::State &state, std::unique_ptr<ViewRegistry> views)
{
  MeterProvider mp(std::move(views));
  auto m = mp.GetMeter("meter1", "version1", "schema1");

  std::unique_ptr<MockMetricExporter> exporter(new MockMetricExporter());
  std::shared_ptr<MetricReader> reader{new MockMetricReader(std::move(exporter))};
  mp.AddMetricReader(reader);
  auto h = m->CreateDoubleHistogram("histogram1", "histogram1_description", "histogram1_unit");
  std::default_random_engine generator;
  std::uniform_int_distribution<int> distribution(0, 1000000);
  // Generate 100000 measurements
  constexpr size_t TOTAL_MEASUREMENTS = 100000;
  double measurements[TOTAL_MEASUREMENTS];
  for (size_t i = 0; i < TOTAL_MEASUREMENTS; i++)
  {
    measurements[i] = static_cast<double>(distribution(generator));
  }
  std::vector<T> actuals;
  std::vector<std::thread> collectionThreads;
  std::function<void()> collectMetrics = [&reader, &actuals]() {
    reader->Collect([&](ResourceMetrics &rm) {
      for (const ScopeMetrics &smd : rm.scope_metric_data_)
      {
        for (const MetricData &md : smd.metric_data_)
        {
          for (const PointDataAttributes &dp : md.point_data_attr_)
          {
            actuals.push_back(opentelemetry::nostd::get<T>(dp.point_data));
          }
        }
      }
      return true;
    });
  };

  while (state.KeepRunningBatch(TOTAL_MEASUREMENTS))
  {
    for (size_t i = 0; i < TOTAL_MEASUREMENTS; i++)
    {
      h->Record(measurements[i], {});
      if (i % 1000 == 0 || i == TOTAL_MEASUREMENTS - 1)
      {
        collectMetrics();
      }
      if (i == 100)
      {
        std::this_thread::sleep_for(std::chrono::nanoseconds(4));
      }
    }
  }
}

void BM_HistogramAggregation(benchmark::State &state)
{
  std::unique_ptr<ViewRegistry> views{new ViewRegistry()};
  HistogramAggregation<HistogramPointData>(state, std::move(views));
}

BENCHMARK(BM_HistogramAggregation);

// Add this helper function before your benchmark functions

void RunBase2ExponentialHistogramAggregation(benchmark::State &state, int scale)
{
  std::string instrument_unit = "histogram1_unit";
  std::unique_ptr<InstrumentSelector> histogram_instrument_selector{
      new InstrumentSelector(InstrumentType::kHistogram, ".*", instrument_unit)};
  std::unique_ptr<MeterSelector> histogram_meter_selector{
      new MeterSelector("meter1", "version1", "schema1")};

  Base2ExponentialHistogramAggregationConfig config;
  config.max_scale_ = scale;

  std::unique_ptr<View> histogram_view{
      new View("base2_expohisto", "description", instrument_unit,
               AggregationType::kBase2ExponentialHistogram,
               std::make_shared<Base2ExponentialHistogramAggregationConfig>(config))};

  std::unique_ptr<ViewRegistry> views{new ViewRegistry()};
  views->AddView(std::move(histogram_instrument_selector), std::move(histogram_meter_selector),
                 std::move(histogram_view));

  HistogramAggregation<Base2ExponentialHistogramPointData>(state, std::move(views));
}

void BM_Base2ExponentialHistogramAggregationZeroScale(benchmark::State &state)
{
  RunBase2ExponentialHistogramAggregation(state, 0);
}
BENCHMARK(BM_Base2ExponentialHistogramAggregationZeroScale);

void BM_Base2ExponentialHistogramAggregationOneScale(benchmark::State &state)
{
  RunBase2ExponentialHistogramAggregation(state, 1);
}
BENCHMARK(BM_Base2ExponentialHistogramAggregationOneScale);

void BM_Base2ExponentialHistogramAggregationTwoScale(benchmark::State &state)
{
  RunBase2ExponentialHistogramAggregation(state, 2);
}
BENCHMARK(BM_Base2ExponentialHistogramAggregationTwoScale);

void BM_Base2ExponentialHistogramAggregationFourScale(benchmark::State &state)
{
  RunBase2ExponentialHistogramAggregation(state, 4);
}
BENCHMARK(BM_Base2ExponentialHistogramAggregationFourScale);

void BM_Base2ExponentialHistogramAggregationEightScale(benchmark::State &state)
{
  RunBase2ExponentialHistogramAggregation(state, 8);
}
BENCHMARK(BM_Base2ExponentialHistogramAggregationEightScale);

void BM_Base2ExponentialHistogramAggregationSixteenScale(benchmark::State &state)
{
  RunBase2ExponentialHistogramAggregation(state, 16);
}
BENCHMARK(BM_Base2ExponentialHistogramAggregationSixteenScale);

}  // namespace
BENCHMARK_MAIN();
