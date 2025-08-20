// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <benchmark/benchmark.h>
#include <stddef.h>
#include <algorithm>
#include <chrono>
#include <functional>
#include <initializer_list>
#include <random>
#include <thread>
#include <utility>
#include <vector>
#include "common.h"

#include "opentelemetry/metrics/meter.h"
#include "opentelemetry/metrics/sync_instruments.h"
#include "opentelemetry/nostd/function_ref.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/variant.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/metrics/data/metric_data.h"
#include "opentelemetry/sdk/metrics/data/point_data.h"
#include "opentelemetry/sdk/metrics/export/metric_producer.h"
#include "opentelemetry/sdk/metrics/meter_provider.h"
#include "opentelemetry/sdk/metrics/metric_reader.h"
#include "opentelemetry/sdk/metrics/push_metric_exporter.h"

using namespace opentelemetry;
using namespace opentelemetry::sdk::instrumentationscope;
using namespace opentelemetry::sdk::metrics;

namespace
{
void BM_SumAggregation(benchmark::State &state)
{
  MeterProvider mp;
  auto m = mp.GetMeter("meter1", "version1", "schema1");

  std::unique_ptr<MockMetricExporter> exporter(new MockMetricExporter());
  std::shared_ptr<MetricReader> reader{new MockMetricReader(std::move(exporter))};
  mp.AddMetricReader(reader);
  auto h = m->CreateDoubleCounter("counter1", "counter1_description", "counter1_unit");
  std::default_random_engine generator;
  std::uniform_int_distribution<int> distribution(0, 1000000);
  // Generate 100000 measurements
  constexpr size_t TOTAL_MEASUREMENTS = 100000;
  double measurements[TOTAL_MEASUREMENTS];
  for (size_t i = 0; i < TOTAL_MEASUREMENTS; i++)
  {
    measurements[i] = static_cast<double>(distribution(generator));
  }
  std::vector<SumPointData> actuals;
  std::vector<std::thread> collectionThreads;
  std::function<void()> collectMetrics = [&reader, &actuals]() {
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
  };

  while (state.KeepRunning())
  {
    for (size_t i = 0; i < TOTAL_MEASUREMENTS; i++)
    {
      h->Add(measurements[i], {});
      if (i % 1000 == 0 || i == TOTAL_MEASUREMENTS - 1)
      {
        collectMetrics();
      }
      if (i == 500)
      {
        std::this_thread::sleep_for(std::chrono::nanoseconds(2));
      }
    }
  }
}

BENCHMARK(BM_SumAggregation);

}  // namespace
BENCHMARK_MAIN();
