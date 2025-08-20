// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <benchmark/benchmark.h>
#include <stdlib.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "opentelemetry/common/key_value_iterable_view.h"
#include "opentelemetry/context/context.h"
#include "opentelemetry/metrics/meter.h"
#include "opentelemetry/metrics/sync_instruments.h"
#include "opentelemetry/nostd/function_ref.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/unique_ptr.h"
#include "opentelemetry/sdk/common/attribute_utils.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/metrics/export/metric_producer.h"
#include "opentelemetry/sdk/metrics/instruments.h"
#include "opentelemetry/sdk/metrics/meter_provider.h"
#include "opentelemetry/sdk/metrics/metric_reader.h"

using namespace opentelemetry;
using namespace opentelemetry::sdk::instrumentationscope;
using namespace opentelemetry::sdk::metrics;
using namespace opentelemetry::sdk::common;

class MockMetricExporter : public MetricReader
{
public:
  MockMetricExporter() = default;

  opentelemetry::sdk::metrics::AggregationTemporality GetAggregationTemporality(
      opentelemetry::sdk::metrics::InstrumentType) const noexcept override
  {
    return AggregationTemporality::kCumulative;
  }

private:
  bool OnForceFlush(std::chrono::microseconds /*timeout*/) noexcept override { return true; }

  bool OnShutDown(std::chrono::microseconds /*timeout*/) noexcept override { return true; }

  void OnInitialized() noexcept override {}
};

namespace
{
void BM_MeasurementsTest(benchmark::State &state)
{
  MeterProvider mp;
  auto m = mp.GetMeter("meter1", "version1", "schema1");

  std::shared_ptr<MetricReader> exporter(new MockMetricExporter());
  mp.AddMetricReader(exporter);
  auto h = m->CreateDoubleCounter("counter1", "counter1_description", "counter1_unit");
  size_t MAX_MEASUREMENTS = 10000;  // keep low to prevent CI failure due to timeout
  size_t NUM_CORES        = 1;
  std::vector<std::thread> threads;
  std::map<std::string, uint32_t> attributes[1000];
  size_t total_index = 0;
  for (uint32_t i = 0; i < 10; i++)
  {
    for (uint32_t j = 0; j < 10; j++)
      for (uint32_t k = 0; k < 10; k++)
        attributes[total_index++] = {{"dim1", i}, {"dim2", j}, {"dim3", k}};
  }
  while (state.KeepRunning())
  {
    threads.clear();
    std::atomic<size_t> cur_processed{0};
    for (size_t i = 0; i < NUM_CORES; i++)
    {
      threads.push_back(std::thread([&h, &cur_processed, &MAX_MEASUREMENTS, &attributes]() {
        while (cur_processed++ <= MAX_MEASUREMENTS)
        {
          size_t index = rand() % 1000;
          h->Add(1.0,
                 opentelemetry::common::KeyValueIterableView<std::map<std::string, uint32_t>>(
                     attributes[index]),
                 opentelemetry::context::Context{});
        }
      }));
    }
    for (auto &thread : threads)
    {
      thread.join();
    }
  }
  exporter->Collect([&](ResourceMetrics & /*rm*/) { return true; });
}
BENCHMARK(BM_MeasurementsTest);

}  // namespace
BENCHMARK_MAIN();
