// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <benchmark/benchmark.h>

#include "opentelemetry/sdk/common/attribute_utils.h"
#include "opentelemetry/sdk/common/attributemap_hash.h"

using namespace opentelemetry::sdk::common;
namespace
{
void BM_AttributeMapHash(benchmark::State &state)
{
  OrderedAttributeMap map1 = {{"k1", "v1"}, {"k2", "v2"}, {"k3", "v3"},  {"k4", "v4"},
                              {"k5", true}, {"k6", 12},   {"k7", 12.209}};
  while (state.KeepRunning())
  {
    benchmark::DoNotOptimize(GetHashForAttributeMap(map1));
  }
}
BENCHMARK(BM_AttributeMapHash);

}  // namespace
BENCHMARK_MAIN();
