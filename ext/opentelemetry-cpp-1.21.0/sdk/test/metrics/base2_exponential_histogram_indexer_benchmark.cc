// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <benchmark/benchmark.h>
#include <stdint.h>
#include <array>
#include <random>

#include "opentelemetry/sdk/metrics/aggregation/base2_exponential_histogram_indexer.h"

using namespace opentelemetry::sdk::metrics;
namespace
{

void BM_NewIndexer(benchmark::State &state)
{
  std::array<int, 1000> batch;
  std::default_random_engine generator;
  std::uniform_int_distribution<int> distribution(1, 32);

  while (state.KeepRunningBatch(static_cast<benchmark::IterationCount>(batch.size())))
  {
    state.PauseTiming();
    for (auto &value : batch)
    {
      value = distribution(generator);
    }
    state.ResumeTiming();

    for (const auto value : batch)
    {
      benchmark::DoNotOptimize(Base2ExponentialHistogramIndexer(value));
    }
  }
}

BENCHMARK(BM_NewIndexer);

void BM_ComputeIndex(benchmark::State &state)
{
  std::array<double, 1000> batch;
  std::default_random_engine generator;
  std::uniform_real_distribution<double> distribution(0, 1000);
  Base2ExponentialHistogramIndexer indexer(static_cast<int32_t>(state.range(0)));

  while (state.KeepRunningBatch(static_cast<benchmark::IterationCount>(batch.size())))
  {
    state.PauseTiming();
    for (auto &value : batch)
    {
      value = distribution(generator);
    }
    state.ResumeTiming();

    for (const auto value : batch)
    {
      benchmark::DoNotOptimize(indexer.ComputeIndex(value));
    }
  }
}

BENCHMARK(BM_ComputeIndex)->Arg(-1)->Arg(0)->Arg(1)->Arg(20);

}  // namespace

BENCHMARK_MAIN();
