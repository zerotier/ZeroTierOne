// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <benchmark/benchmark.h>
#include <map>
#include <string>

#include "opentelemetry/common/key_value_iterable_view.h"
#include "opentelemetry/sdk/metrics/view/attributes_processor.h"

using namespace opentelemetry::sdk::metrics;
namespace
{
void BM_AttributseProcessorFilter(benchmark::State &state)
{
  std::map<std::string, int> attributes = {
      {"att1", 10}, {"attr1", 20}, {"attr3", 30}, {"attr4", 40}};
  FilteringAttributesProcessor attributes_processor(
      {{"attr2", true}, {"attr4", true}, {"attr6", true}});
  opentelemetry::common::KeyValueIterableView<std::map<std::string, int>> iterable(attributes);
  while (state.KeepRunning())
  {
    auto filtered_attributes = attributes_processor.process(iterable);
  }
}

BENCHMARK(BM_AttributseProcessorFilter);
}  // namespace

BENCHMARK_MAIN();
