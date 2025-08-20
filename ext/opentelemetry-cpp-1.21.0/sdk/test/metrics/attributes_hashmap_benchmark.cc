// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <benchmark/benchmark.h>
#include <stddef.h>
#include <stdint.h>
#include <algorithm>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "opentelemetry/nostd/function_ref.h"
#include "opentelemetry/sdk/metrics/aggregation/aggregation.h"
#include "opentelemetry/sdk/metrics/aggregation/drop_aggregation.h"
#include "opentelemetry/sdk/metrics/state/attributes_hashmap.h"
#include "opentelemetry/sdk/metrics/view/attributes_processor.h"

using namespace opentelemetry::sdk::metrics;
constexpr size_t MAX_THREADS = 500;
namespace
{

void BM_AttributseHashMap(benchmark::State &state)
{

  AttributesHashMap hash_map;
  std::vector<std::thread> workers;
  std::vector<MetricAttributes> attributes = {{{"k1", "v1"}, {"k2", "v2"}},
                                              {{"k1", "v1"}, {"k2", "v2"}, {"k3", "v3"}}};

  std::mutex m;

  auto work = [&attributes, &hash_map, &m](const size_t i) {
    std::function<std::unique_ptr<Aggregation>()> create_default_aggregation =
        []() -> std::unique_ptr<Aggregation> {
      return std::unique_ptr<Aggregation>(new DropAggregation);
    };
    m.lock();
    hash_map.GetOrSetDefault(attributes[i % 2], create_default_aggregation)
        ->Aggregate(static_cast<int64_t>(1));
    benchmark::DoNotOptimize(hash_map.Has(attributes[i % 2]));
    m.unlock();
  };
  while (state.KeepRunning())
  {
    for (size_t i = 0; i < MAX_THREADS; i++)
    {
      workers.push_back(std::thread(work, i));
    }
  }

  for (auto &t : workers)
  {
    t.join();
  }
}

BENCHMARK(BM_AttributseHashMap);
}  // namespace

BENCHMARK_MAIN();
